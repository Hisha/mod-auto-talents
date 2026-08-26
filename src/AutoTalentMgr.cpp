#include "AutoTalentMgr.h"

#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
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

    _personalBuildsEnabled = sConfigMgr->GetOption<bool>("AutoTalents.CustomBuilds.Enable", true);
    _personalCostMode = std::min<uint8>(sConfigMgr->GetOption<uint8>("AutoTalents.CustomBuilds.CostMode", 0), 2);
    _personalBaseCost = sConfigMgr->GetOption<uint32>("AutoTalents.CustomBuilds.BaseCost", 100000);
    _personalCostIncrease = sConfigMgr->GetOption<uint32>("AutoTalents.CustomBuilds.CostIncrease", 50000);
    _personalCostMultiplier = std::max<float>(sConfigMgr->GetOption<float>("AutoTalents.CustomBuilds.CostMultiplier", 1.5f), 1.0f);
    _personalMaxCost = sConfigMgr->GetOption<uint32>("AutoTalents.CustomBuilds.MaxCost", 0);
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
        "SELECT `build_type`, `build_id` FROM `auto_talent_character` WHERE `guid` = {} AND `spec_slot` = {}",
        guid, uint32(specSlot));

    if (!result)
        return assignment;

    assignment.Found = true;
    Field* fields = result->Fetch();
    assignment.BuildType = fields[0].Get<uint8>() == 1 ? AutoTalentBuildType::Personal : AutoTalentBuildType::Prebuilt;
    assignment.BuildId = fields[1].Get<uint32>();
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

    std::string sql = Acore::StringFormat(
        "INSERT INTO `auto_talent_character` (`guid`, `spec_slot`, `build_type`, `build_id`) "
        "VALUES ({}, {}, 0, {}) ON DUPLICATE KEY UPDATE `build_type` = 0, `build_id` = VALUES(`build_id`)",
        player->GetGUID().GetCounter(), uint32(specSlot), buildId);
    CharacterDatabase.DirectExecute(sql.c_str());

    return true;
}

AutoTalentPersonalBuildInfo AutoTalentMgr::GetPersonalBuildInfo(uint32 guid, uint8 specSlot) const
{
    AutoTalentPersonalBuildInfo info;
    QueryResult result = CharacterDatabase.Query(
        "SELECT `name`, `save_count` FROM `auto_talent_personal_build` WHERE `guid` = {} AND `spec_slot` = {}",
        guid, uint32(specSlot));
    if (!result)
        return info;

    Field* fields = result->Fetch();
    info.Found = true;
    info.Name = fields[0].Get<std::string>();
    info.SaveCount = fields[1].Get<uint32>();
    return info;
}

uint32 AutoTalentMgr::CalculatePersonalBuildCost(uint32 saveCount) const
{
    double cost = _personalBaseCost;
    if (_personalCostMode == 1)
        cost += double(saveCount) * double(_personalCostIncrease);
    else if (_personalCostMode == 2)
        cost *= std::pow(double(_personalCostMultiplier), double(saveCount));

    if (_personalMaxCost > 0)
        cost = std::min<double>(cost, _personalMaxCost);

    return uint32(std::min<double>(cost, std::numeric_limits<int32>::max()));
}

uint32 AutoTalentMgr::GetNextPersonalBuildCost(uint32 guid, uint8 specSlot) const
{
    AutoTalentPersonalBuildInfo info = GetPersonalBuildInfo(guid, specSlot);
    return CalculatePersonalBuildCost(info.Found ? info.SaveCount : 0);
}

uint64 AutoTalentMgr::MakeDraftKey(uint32 guid, uint8 specSlot)
{
    return (uint64(guid) << 8) | uint64(specSlot);
}

bool AutoTalentMgr::BeginPersonalBuildDraft(Player* player, uint8 specSlot, std::string const& name, std::string& error)
{
    if (!_personalBuildsEnabled)
    {
        error = "Personal builds are disabled on this server.";
        return false;
    }
    if (!player || specSlot > 1)
    {
        error = "Invalid player or spec slot.";
        return false;
    }

    AutoTalentBuildDraft draft;
    draft.ClassId = player->getClass();
    draft.SpecSlot = specSlot;
    draft.Name = name.empty() ? "Personal Build" : name.substr(0, 64);
    draft.Steps.reserve(71);
    _drafts[MakeDraftKey(player->GetGUID().GetCounter(), specSlot)] = std::move(draft);
    return true;
}

bool AutoTalentMgr::AddPersonalBuildDraftStep(Player* player, uint8 specSlot, uint16 sequence, uint8 rank,
    std::string const& talentName, std::string& error)
{
    if (!player || specSlot > 1)
    {
        error = "Invalid player or spec slot.";
        return false;
    }
    if (sequence < 1 || sequence > 71 || rank < 1 || rank > MAX_TALENT_RANK || talentName.empty())
    {
        error = "Invalid personal-build draft step.";
        return false;
    }

    auto itr = _drafts.find(MakeDraftKey(player->GetGUID().GetCounter(), specSlot));
    if (itr == _drafts.end())
    {
        error = "No personal-build draft is active for that spec slot.";
        return false;
    }
    if (itr->second.ClassId != player->getClass())
    {
        error = "The active personal-build draft belongs to a different class.";
        return false;
    }
    if (sequence != itr->second.Steps.size() + 1)
    {
        error = "Personal-build draft steps must arrive in order.";
        return false;
    }

    AutoTalentBuildStep step;
    step.Sequence = sequence;
    step.Rank = rank;
    step.TalentName = talentName.substr(0, 96);
    itr->second.Steps.push_back(std::move(step));
    return true;
}

bool AutoTalentMgr::CommitPersonalBuildDraft(Player* player, uint8 specSlot, uint32& chargedCost, std::string& error)
{
    chargedCost = 0;
    if (!player || specSlot > 1)
    {
        error = "Invalid player or spec slot.";
        return false;
    }

    uint64 key = MakeDraftKey(player->GetGUID().GetCounter(), specSlot);
    auto itr = _drafts.find(key);
    if (itr == _drafts.end())
    {
        error = "No personal-build draft is active for that spec slot.";
        return false;
    }

    AutoTalentBuildDraft draft = std::move(itr->second);
    _drafts.erase(itr);

    if (draft.ClassId != player->getClass())
    {
        error = "The personal-build draft belongs to a different class.";
        return false;
    }

    return SavePersonalBuild(player, specSlot, draft.Name, std::move(draft.Steps), chargedCost, error);
}

void AutoTalentMgr::CancelPersonalBuildDraft(uint32 guid, uint8 specSlot)
{
    _drafts.erase(MakeDraftKey(guid, specSlot));
}

bool AutoTalentMgr::ValidateBuild(AutoTalentBuild& build, bool requireComplete, std::string& error) const
{
    if (requireComplete && build.Steps.size() != 71)
    {
        error = "A personal build must contain exactly 71 ordered talent points.";
        return false;
    }

    if (build.Steps.empty())
    {
        error = "The build has no talent steps.";
        return false;
    }

    std::unordered_map<uint32, uint8> lastRankByTalent;
    for (uint32 index = 0; index < build.Steps.size(); ++index)
    {
        AutoTalentBuildStep& step = build.Steps[index];
        if (step.Sequence != index + 1)
        {
            error = "Talent steps are not contiguous.";
            return false;
        }

        TalentEntry const* talentInfo = ResolveTalentName(step.TalentName, build.ClassId, step.TalentId);
        if (!talentInfo)
        {
            error = "Unknown or wrong-class talent at step " + std::to_string(step.Sequence) + ": " + step.TalentName;
            return false;
        }

        if (step.Rank == 0 || step.Rank > MAX_TALENT_RANK || !talentInfo->RankID[step.Rank - 1])
        {
            error = "Invalid talent rank at step " + std::to_string(step.Sequence) + ".";
            return false;
        }

        uint8 expectedRank = lastRankByTalent[step.TalentId] + 1;
        if (step.Rank != expectedRank)
        {
            error = "Talent ranks are not sequential at step " + std::to_string(step.Sequence) + ".";
            return false;
        }
        lastRankByTalent[step.TalentId] = step.Rank;
    }

    return true;
}

bool AutoTalentMgr::LoadPersonalBuild(uint32 guid, uint8 specSlot, AutoTalentBuild& build, std::string& error) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT `class_id`, `name` FROM `auto_talent_personal_build` WHERE `guid` = {} AND `spec_slot` = {}",
        guid, uint32(specSlot));
    if (!result)
    {
        error = "No personal build is saved for that spec slot.";
        return false;
    }

    Field* fields = result->Fetch();
    build = AutoTalentBuild{};
    build.ClassId = fields[0].Get<uint8>();
    build.Name = fields[1].Get<std::string>();
    build.Description = "Personal build";
    build.Enabled = true;
    build.Personal = true;

    QueryResult steps = CharacterDatabase.Query(
        "SELECT `sequence`, `talent_name`, `rank` FROM `auto_talent_personal_build_step` "
        "WHERE `guid` = {} AND `spec_slot` = {} ORDER BY `sequence`",
        guid, uint32(specSlot));
    if (!steps)
    {
        error = "The personal build has no saved talent steps.";
        return false;
    }

    do
    {
        Field* sf = steps->Fetch();
        AutoTalentBuildStep step;
        step.Sequence = sf[0].Get<uint16>();
        step.TalentName = sf[1].Get<std::string>();
        step.Rank = sf[2].Get<uint8>();
        build.Steps.push_back(std::move(step));
    } while (steps->NextRow());

    return ValidateBuild(build, true, error);
}

bool AutoTalentMgr::SavePersonalBuild(Player* player, uint8 specSlot, std::string const& name,
    std::vector<AutoTalentBuildStep> steps, uint32& chargedCost, std::string& error)
{
    chargedCost = 0;
    if (!_personalBuildsEnabled)
    {
        error = "Personal builds are disabled on this server.";
        return false;
    }
    if (!player || specSlot > 1)
    {
        error = "Invalid player or spec slot.";
        return false;
    }

    AutoTalentBuild build;
    build.ClassId = player->getClass();
    build.Name = name.empty() ? "Personal Build" : name.substr(0, 64);
    build.Enabled = true;
    build.Personal = true;
    build.Steps = std::move(steps);
    if (!ValidateBuild(build, true, error))
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    AutoTalentPersonalBuildInfo existing = GetPersonalBuildInfo(guid, specSlot);
    chargedCost = CalculatePersonalBuildCost(existing.Found ? existing.SaveCount : 0);
    if (player->GetMoney() < chargedCost)
    {
        error = "You do not have enough money to save this personal build.";
        return false;
    }

    std::string escapedName = build.Name;
    CharacterDatabase.EscapeString(escapedName);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    std::string sql = Acore::StringFormat(
        "INSERT INTO `auto_talent_personal_build` (`guid`, `spec_slot`, `class_id`, `name`, `save_count`) "
        "VALUES ({}, {}, {}, '{}', 1) ON DUPLICATE KEY UPDATE `class_id` = VALUES(`class_id`), "
        "`name` = VALUES(`name`), `save_count` = `save_count` + 1",
        guid, uint32(specSlot), uint32(build.ClassId), escapedName);
    trans->Append(sql.c_str());

    sql = Acore::StringFormat(
        "DELETE FROM `auto_talent_personal_build_step` WHERE `guid` = {} AND `spec_slot` = {}",
        guid, uint32(specSlot));
    trans->Append(sql.c_str());

    for (AutoTalentBuildStep const& step : build.Steps)
    {
        std::string escapedTalent = step.TalentName;
        CharacterDatabase.EscapeString(escapedTalent);
        sql = Acore::StringFormat(
            "INSERT INTO `auto_talent_personal_build_step` (`guid`, `spec_slot`, `sequence`, `talent_name`, `rank`) "
            "VALUES ({}, {}, {}, '{}', {})",
            guid, uint32(specSlot), uint32(step.Sequence), escapedTalent, uint32(step.Rank));
        trans->Append(sql.c_str());
    }

    sql = Acore::StringFormat(
        "INSERT INTO `auto_talent_character` (`guid`, `spec_slot`, `build_type`, `build_id`) "
        "VALUES ({}, {}, 1, 0) ON DUPLICATE KEY UPDATE `build_type` = 1, `build_id` = 0",
        guid, uint32(specSlot));
    trans->Append(sql.c_str());
    CharacterDatabase.DirectCommitTransaction(trans);

    if (chargedCost > 0)
        player->ModifyMoney(-int32(chargedCost));

    return true;
}

bool AutoTalentMgr::ClonePrebuiltToPersonal(Player* player, uint8 specSlot, uint32 sourceBuildId,
    std::string const& name, uint32& chargedCost, std::string& error)
{
    AutoTalentBuild const* source = GetBuild(sourceBuildId);
    if (!source || !source->Enabled)
    {
        error = "That source build does not exist or is disabled.";
        return false;
    }
    if (!player || source->ClassId != player->getClass())
    {
        error = "That source build is for a different class.";
        return false;
    }

    std::string personalName = name.empty() ? source->Name + " Personal" : name;
    return SavePersonalBuild(player, specSlot, personalName, source->Steps, chargedCost, error);
}

bool AutoTalentMgr::SetPersonalAssignment(Player* player, uint8 specSlot, std::string& error)
{
    if (!player || specSlot > 1)
    {
        error = "Invalid player or spec slot.";
        return false;
    }

    AutoTalentBuild build;
    if (!LoadPersonalBuild(player->GetGUID().GetCounter(), specSlot, build, error))
        return false;
    if (build.ClassId != player->getClass())
    {
        error = "That personal build is for a different class.";
        return false;
    }

    std::string sql = Acore::StringFormat(
        "INSERT INTO `auto_talent_character` (`guid`, `spec_slot`, `build_type`, `build_id`) "
        "VALUES ({}, {}, 1, 0) ON DUPLICATE KEY UPDATE `build_type` = 1, `build_id` = 0",
        player->GetGUID().GetCounter(), uint32(specSlot));
    CharacterDatabase.DirectExecute(sql.c_str());
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

    AutoTalentBuild personalBuild;
    AutoTalentBuild const* build = nullptr;
    std::string loadError;
    if (assignment.BuildType == AutoTalentBuildType::Personal)
    {
        if (!LoadPersonalBuild(player->GetGUID().GetCounter(), activeSlot, personalBuild, loadError))
        {
            LOG_WARN("module", "AutoTalents: {} has a personal build assigned to slot {}, but it could not be loaded: {}",
                player->GetName(), uint32(activeSlot + 1), loadError);
            return;
        }
        build = &personalBuild;
    }
    else
    {
        build = GetBuild(assignment.BuildId);
        if (!build || !build->Enabled)
        {
            LOG_WARN("module", "AutoTalents: {} has build {} assigned to slot {}, but the build is missing or disabled.",
                player->GetName(), assignment.BuildId, uint32(activeSlot + 1));
            return;
        }
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
