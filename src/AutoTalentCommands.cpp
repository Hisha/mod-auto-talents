#include "AutoTalentMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <cstdio>
#include <cstdlib>
#include <string>

class AutoTalentCommands : public CommandScript
{
public:
    AutoTalentCommands() : CommandScript("AutoTalentCommands") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> autoTalentCommandTable =
        {
            { "list",   SEC_PLAYER, false, &HandleListCommand,   "" },
            { "status", SEC_PLAYER, false, &HandleStatusCommand, "" },
            { "set",    SEC_PLAYER, false, &HandleSetCommand,    "" },
            { "clear",  SEC_PLAYER, false, &HandleClearCommand,  "" }
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "autotalent", SEC_PLAYER, false, nullptr, "", autoTalentCommandTable }
        };

        return commandTable;
    }

private:
    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    static bool HandleListCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        std::vector<AutoTalentBuild const*> builds = sAutoTalentMgr->GetBuildsForClass(player->getClass());
        if (builds.empty())
        {
            handler->SendSysMessage("Auto Talents: no enabled builds are available for your class.");
            return true;
        }

        handler->SendSysMessage("Auto Talents - available builds:");
        for (AutoTalentBuild const* build : builds)
            handler->PSendSysMessage("  {} - {} ({})", build->Id, build->Name, build->Description);

        handler->SendSysMessage("Use: .autotalent set <1|2> <buildId>");
        return true;
    }

    static bool HandleStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        handler->PSendSysMessage("Auto Talents - active talent slot: {}", uint32(player->GetActiveSpec() + 1));

        for (uint8 slot = 0; slot < 2; ++slot)
        {
            AutoTalentAssignment assignment = sAutoTalentMgr->GetAssignment(player->GetGUID().GetCounter(), slot);
            if (!assignment.Found)
            {
                handler->PSendSysMessage("  Spec {}: no auto-talent build assigned", uint32(slot + 1));
                continue;
            }

            AutoTalentBuild const* build = sAutoTalentMgr->GetBuild(assignment.BuildId);
            if (!build)
            {
                handler->PSendSysMessage("  Spec {}: build {} is missing", uint32(slot + 1), assignment.BuildId);
                continue;
            }

            handler->PSendSysMessage("  Spec {}: {} ({})", uint32(slot + 1), build->Name, build->Id);
        }

        return true;
    }

    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        unsigned int buildId = 0;
        if (!args || std::sscanf(args, "%u %u", &slotInput, &buildId) != 2 || slotInput < 1 || slotInput > 2)
        {
            handler->SendSysMessage("Usage: .autotalent set <1|2> <buildId>");
            return true;
        }

        std::string error;
        if (!sAutoTalentMgr->SetAssignment(player, uint8(slotInput - 1), uint32(buildId), error))
        {
            handler->PSendSysMessage("Auto Talents: {}", error);
            return true;
        }

        AutoTalentBuild const* build = sAutoTalentMgr->GetBuild(uint32(buildId));
        handler->PSendSysMessage("Auto Talents: Spec {} assigned to {}.", slotInput, build ? build->Name : "selected build");

        if (player->GetActiveSpec() == slotInput - 1)
            sAutoTalentMgr->HandleReconcileTrigger(player, "assignment-change");

        return true;
    }

    static bool HandleClearCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!args || std::sscanf(args, "%u", &slotInput) != 1 || slotInput < 1 || slotInput > 2)
        {
            handler->SendSysMessage("Usage: .autotalent clear <1|2>");
            return true;
        }

        std::string error;
        if (!sAutoTalentMgr->ClearAssignment(player, uint8(slotInput - 1), error))
        {
            handler->PSendSysMessage("Auto Talents: {}", error);
            return true;
        }

        handler->PSendSysMessage("Auto Talents: Spec {} assignment cleared.", slotInput);
        return true;
    }
};

void AddSC_AutoTalentCommands()
{
    new AutoTalentCommands();
}
