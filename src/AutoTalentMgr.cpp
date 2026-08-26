#include "AutoTalentMgr.h"

#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <unordered_map>

namespace
{
using TalentRankMap = std::unordered_map<uint32, uint8>;

std::string NormalizeTalentName(std::string const& value)
{
    std::string normalized;
    normalized.reserve(value.size());

    for (unsigned char c : value)
    {
        if (std::isalnum(c))
            normalized.push_back(static_cast<char>(std::tolower(c)));
    }

    return normalized;
}

TalentEntry const* ResolveTalentName(std::string const& talentName, uint8 classId, uint32& talentId)
{
    std::string wanted = NormalizeTalentName(talentName);

    for (uint32 id = 0; id < sTalentStore.GetNumRows(); ++id)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(id);
        if (!talentInfo || !talentInfo->RankID[0])
            continue;

        TalentTabEntry const* talentTab = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTab || classId == 0 || classId >= MAX_CLASSES ||
            !(talentTab->ClassMask & (1u << (classId - 1))))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(talentInfo->RankID[0]);
        if (!spellInfo || !spellInfo->SpellName[0])
            continue;

        if (NormalizeTalentName(spellInfo->SpellName[0]) == wanted)
        {
            talentId = id;
            return talentInfo;
        }
    }

    return nullptr;
}

TalentRankMap BuildExpectedRanks(AutoTalentBuild const& build, uint32 pointCount)
{
    TalentRankMap expected;
    pointCount = std::min<uint32>(pointCount, build.Steps.size());

    for (uint32 i = 0; i < pointCount; ++i)
        expected[build.Steps[i].TalentId] = build.Steps[i].Rank;

    return expected;
}

TalentRankMap GetCurrentRanks(Player* player, uint8 specSlot, uint32& spentPoints)
{
    TalentRankMap current;
    spentPoints = 0;

    for (auto const& [spellId, playerTalent] : player->GetTalentMap())
    {
        if (!playerTalent || playerTalent->State == PLAYERSPELL_REMOVED || !playerTalent->IsInSpec(specSlot))
            continue;

        TalentSpellPos const* position = GetTalentSpellPos(spellId);
        if (!position)
            continue;

        uint8 rank = position->rank + 1;
        auto itr = current.find(position->talent_id);
        if (itr == current.end() || rank > itr->second)
            current[position->talent_id] = rank;
    }

    for (auto const& [talentId, rank] : current)
    {
        (void)talentId;
        spentPoints += rank;
    }

    return current;
}

bool CurrentTreeMatchesPrefix(Player* player, AutoTalentBuild const& build, uint8 specSlot, uint32 prefixPoints)
{
    uint32 spentPoints = 0;
    TalentRankMap current = GetCurrentRanks(player, specSlot, spentPoints);
    if (spentPoints != prefixPoints)
        return false;

    return current == BuildExpectedRanks(build, prefixPoints);
}

bool LearnBuildStep(Player* player, AutoTalentBuild const& build, AutoTalentBuildStep const& step)
{
    TalentEntry const* talentInfo = sTalentStore.LookupEntry(step.TalentId);
    if (!talentInfo || step.Rank == 0 || step.Rank > MAX_TALENT_RANK)
        return false;

    uint32 expectedSpell = talentInfo->RankID[step.Rank - 1];
    if (!expectedSpell)
        return false;

    player->LearnTalent(step.TalentId, step.Rank - 1, false);

    if (!player->HasTalent(expectedSpell, player->GetActiveSpec()))
    {
        LOG_ERROR("module", "AutoTalents: failed to apply build '{}' ({}) step {} to {}: talent {} rank {}.",
            build.Name, build.Id, step.Sequence, player->GetName(), step.TalentId, uint32(step.Rank));
        return false;
    }

    return true;
}
}

AutoTalentMgr* AutoTalentMgr::instance()
{
    static AutoTalentMgr instance;
    return &instance;
}

void AutoTalentMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("AutoTalents.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("AutoTalents.Debug", false);
    _loginMessage = sConfigMgr->GetOption<bool>("AutoTalents.LoginMessage", false);
}

void AutoTalentMgr::LoadBuilds()
{
    _builds.clear();

    QueryResult buildResult = WorldDatabase.Query(
        "SELECT `id`, `class_id`, `name`, `description`, `enabled` "
        "FROM `auto_talent_build` ORDER BY `class_id`, `id`");

    if (!buildResult)
    {
        LOG_WARN("module", "AutoTalents: no build definitions found in auto_talent_build.");
        return;
    }

    do
    {
        Field* fields = buildResult->Fetch();

        AutoTalentBuild build;
        build.Id = fields[0].Get<uint32>();
        build.ClassId = fields[1].Get<uint8>();
        build.Name = fields[2].Get<std::string>();
        build.Description = fields[3].Get<std::string>();
        build.Enabled = fields[4].Get<uint8>() != 0;

        _builds.emplace(build.Id, std::move(build));
    } while (buildResult->NextRow());

    QueryResult stepResult = WorldDatabase.Query(
        "SELECT `build_id`, `sequence`, `talent_name`, `rank` "
        "FROM `auto_talent_build_step` ORDER BY `build_id`, `sequence`");

    if (stepResult)
    {
        do
        {
            Field* fields = stepResult->Fetch();
            uint32 buildId = fields[0].Get<uint32>();

            auto itr = _builds.find(buildId);
            if (itr == _builds.end())
            {
                LOG_WARN("module", "AutoTalents: ignoring step for unknown build id {}.", buildId);
                continue;
            }

            AutoTalentBuildStep step;
            step.Sequence = fields[1].Get<uint16>();
            step.TalentName = fields[2].Get<std::string>();
            step.Rank = fields[3].Get<uint8>();
            itr->second.Steps.push_back(step);
        } while (stepResult->NextRow());
    }

    uint32 enabledBuilds = 0;
    for (auto& [id, build] : _builds)
    {
        (void)id;
        if (!build.Enabled)
            continue;

        bool valid = true;
        std::unordered_map<uint32, uint8> lastRankByTalent;

        for (uint32 index = 0; index < build.Steps.size(); ++index)
        {
            AutoTalentBuildStep& step = build.Steps[index];

            if (step.Sequence != index + 1)
            {
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): expected sequence {}, found {}.",
                    build.Name, build.Id, index + 1, step.Sequence);
                valid = false;
                break;
            }

            TalentEntry const* talentInfo = ResolveTalentName(step.TalentName, build.ClassId, step.TalentId);
            if (!talentInfo)
            {
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): step {} references unknown talent name '{}'.",
                    build.Name, build.Id, step.Sequence, step.TalentName);
                valid = false;
                break;
            }

            if (step.Rank == 0 || step.Rank > MAX_TALENT_RANK || !talentInfo->RankID[step.Rank - 1])
            {
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): step {} has invalid rank {} for talent {}.",
                    build.Name, build.Id, step.Sequence, uint32(step.Rank), step.TalentId);
                valid = false;
                break;
            }

            TalentTabEntry const* talentTab = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!talentTab || build.ClassId == 0 || build.ClassId >= MAX_CLASSES ||
                !(talentTab->ClassMask & (1u << (build.ClassId - 1))))
            {
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): step {} talent {} is not valid for class {}.",
                    build.Name, build.Id, step.Sequence, step.TalentId, uint32(build.ClassId));
                valid = false;
                break;
            }

            uint8 expectedRank = lastRankByTalent[step.TalentId] + 1;
            if (step.Rank != expectedRank)
            {
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): step {} talent {} expected rank {}, found rank {}.",
                    build.Name, build.Id, step.Sequence, step.TalentId, uint32(expectedRank), uint32(step.Rank));
                valid = false;
                break;
            }

            lastRankByTalent[step.TalentId] = step.Rank;
        }

        if (!valid || build.Steps.empty())
        {
            if (build.Steps.empty())
                LOG_ERROR("module", "AutoTalents: disabling build '{}' ({}): no talent steps are defined.", build.Name, build.Id);
            build.Enabled = false;
            continue;
        }

        ++enabledBuilds;
        if (_debug)
            LOG_INFO("module", "AutoTalents: validated build '{}' ({}) with {} ordered step(s).",
                build.Name, build.Id, build.Steps.size());
    }

    LOG_INFO("module", "AutoTalents: loaded {} build definition(s), {} enabled and valid.", _builds.size(), enabledBuilds);
}

AutoTalentBuild const* AutoTalentMgr::GetBuild(uint32 buildId) const
{
    auto itr = _builds.find(buildId);
    if (itr == _builds.end())
        return nullptr;

    return &itr->second;
}

std::vector<AutoTalentBuild const*> AutoTalentMgr::GetBuildsForClass(uint8 classId) const
{
    std::vector<AutoTalentBuild const*> result;

    for (auto const& [id, build] : _builds)
    {
        (void)id;
        if (build.Enabled && build.ClassId == classId)
            result.push_back(&build);
    }

    return result;
}

AutoTalentAssignment AutoTalentMgr::GetAssignment(uint32 guid, uint8 specSlot) const
{
    AutoTalentAssignment assignment;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `build_id` FROM `auto_talent_character` WHERE `guid` = {} AND `spec_slot` = {}",
        guid, uint32(specSlot));

    if (!result)
        return assignment;

    assignment.Found = true;
    assignment.BuildId = result->Fetch()[0].Get<uint32>();
    return assignment;
}

bool AutoTalentMgr::SetAssignment(Player* player, uint8 specSlot, uint32 buildId, std::string& error)
{
    if (!player)
    {
        error = "No player is available.";
        return false;
    }

    if (specSlot > 1)
    {
        error = "Spec slot must be 1 or 2.";
        return false;
    }

    AutoTalentBuild const* build = GetBuild(buildId);
    if (!build || !build->Enabled)
    {
        error = "That auto-talent build does not exist or is disabled.";
        return false;
    }

    if (build->ClassId != player->getClass())
    {
        error = "That auto-talent build is for a different class.";
        return false;
    }

    CharacterDatabase.Execute(
        "INSERT INTO `auto_talent_character` (`guid`, `spec_slot`, `build_id`) "
        "VALUES ({}, {}, {}) ON DUPLICATE KEY UPDATE `build_id` = VALUES(`build_id`)",
        player->GetGUID().GetCounter(), uint32(specSlot), buildId);

    return true;
}

bool AutoTalentMgr::ClearAssignment(Player* player, uint8 specSlot, std::string& error)
{
    if (!player)
    {
        error = "No player is available.";
        return false;
    }

    if (specSlot > 1)
    {
        error = "Spec slot must be 1 or 2.";
        return false;
    }

    CharacterDatabase.Execute(
        "DELETE FROM `auto_talent_character` WHERE `guid` = {} AND `spec_slot` = {}",
        player->GetGUID().GetCounter(), uint32(specSlot));

    return true;
}

void AutoTalentMgr::HandleReconcileTrigger(Player* player, char const* reason)
{
    if (!_enabled || !player)
        return;

    uint8 activeSlot = player->GetActiveSpec();
    AutoTalentAssignment assignment = GetAssignment(player->GetGUID().GetCounter(), activeSlot);
    if (!assignment.Found)
        return;

    AutoTalentBuild const* build = GetBuild(assignment.BuildId);
    if (!build || !build->Enabled)
    {
        LOG_WARN("module", "AutoTalents: {} has build {} assigned to slot {}, but the build is missing or disabled.",
            player->GetName(), assignment.BuildId, uint32(activeSlot + 1));
        return;
    }

    if (build->ClassId != player->getClass())
    {
        LOG_ERROR("module", "AutoTalents: refusing build '{}' ({}) for {} because the build class ({}) does not match player class ({}).",
            build->Name, build->Id, player->GetName(), uint32(build->ClassId), uint32(player->getClass()));
        return;
    }

    uint32 availablePoints = player->CalculateTalentsPoints();
    uint32 targetPoints = std::min<uint32>(availablePoints, build->Steps.size());

    uint32 currentSpent = 0;
    GetCurrentRanks(player, activeSlot, currentSpent);

    if (_debug)
    {
        LOG_INFO("module", "AutoTalents: {} trigger for {} - level {}, active slot {}, build '{}' ({}), spent {}, free {}, target {}.",
            reason, player->GetName(), player->GetLevel(), uint32(activeSlot + 1), build->Name, build->Id,
            currentSpent, player->GetFreeTalentPoints(), targetPoints);
    }

    if (currentSpent == targetPoints && CurrentTreeMatchesPrefix(player, *build, activeSlot, targetPoints))
        return;

    uint32 startStep = currentSpent;
    bool rebuilt = false;

    // The active tree is allowed to grow incrementally only when everything
    // already spent is exactly the prefix of the selected build.
    if (currentSpent > targetPoints || !CurrentTreeMatchesPrefix(player, *build, activeSlot, currentSpent))
    {
        if (currentSpent > 0)
        {
            player->resetTalents(true);
            player->SendTalentsInfoData(false);
            rebuilt = true;
        }

        startStep = 0;
    }

    if (targetPoints == 0)
        return;

    uint32 applied = 0;
    for (uint32 i = startStep; i < targetPoints; ++i)
    {
        if (player->GetFreeTalentPoints() == 0)
        {
            LOG_ERROR("module", "AutoTalents: {} ran out of free talent points while applying build '{}' ({}) at step {} of {}.",
                player->GetName(), build->Name, build->Id, i + 1, targetPoints);
            break;
        }

        if (!LearnBuildStep(player, *build, build->Steps[i]))
            break;

        ++applied;
    }

    player->SendTalentsInfoData(false);

    if (_debug && (applied > 0 || rebuilt))
    {
        LOG_INFO("module", "AutoTalents: {} {} build '{}' ({}) for slot {} through {} point(s); applied {} point(s) this pass{}.",
            player->GetName(), rebuilt ? "rebuilt" : "advanced", build->Name, build->Id, uint32(activeSlot + 1),
            targetPoints, applied, rebuilt ? " after a free reset" : "");
    }
}
