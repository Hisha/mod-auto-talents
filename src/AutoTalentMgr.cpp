#include "AutoTalentMgr.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"

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
        "SELECT `build_id`, `sequence`, `talent_id`, `rank` "
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
            step.TalentId = fields[2].Get<uint32>();
            step.Rank = fields[3].Get<uint8>();
            itr->second.Steps.push_back(step);
        } while (stepResult->NextRow());
    }

    LOG_INFO("module", "AutoTalents: loaded {} build definition(s).", _builds.size());
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
    {
        if (_debug)
            LOG_INFO("module", "AutoTalents: {} trigger for {} (level {}, slot {}) - no assigned build.",
                reason, player->GetName(), player->GetLevel(), uint32(activeSlot + 1));
        return;
    }

    AutoTalentBuild const* build = GetBuild(assignment.BuildId);
    if (!build || !build->Enabled)
    {
        LOG_WARN("module", "AutoTalents: {} has build {} assigned to slot {}, but the build is missing or disabled.",
            player->GetName(), assignment.BuildId, uint32(activeSlot + 1));
        return;
    }

    if (_debug)
    {
        uint32 expectedPoints = player->GetLevel() >= 10 ? player->GetLevel() - 9 : 0;
        LOG_INFO("module", "AutoTalents: {} trigger for {} - level {}, active slot {}, build '{}' ({}), target points {}, defined steps {}.",
            reason, player->GetName(), player->GetLevel(), uint32(activeSlot + 1), build->Name, build->Id,
            expectedPoints, build->Steps.size());
    }

    // Milestone 1 intentionally stops here.
    // The talent reconciliation/application engine is added after the module,
    // SQL schema, commands, and event hooks are verified on a live server.
}
