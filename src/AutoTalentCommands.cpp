#include "AutoTalentMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <cstdio>
#include <string>

using namespace Acore::ChatCommands;

class AutoTalentCommands : public CommandScript
{
public:
    AutoTalentCommands() : CommandScript("AutoTalentCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable customCommandTable =
        {
            { "price", HandleCustomPriceCommand, SEC_PLAYER, Console::No },
            { "clone", HandleCustomCloneCommand, SEC_PLAYER, Console::No },
            { "use",   HandleCustomUseCommand,   SEC_PLAYER, Console::No }
        };

        static ChatCommandTable autoTalentCommandTable =
        {
            { "list",   HandleListCommand,   SEC_PLAYER, Console::No },
            { "status", HandleStatusCommand, SEC_PLAYER, Console::No },
            { "set",    HandleSetCommand,    SEC_PLAYER, Console::No },
            { "clear",  HandleClearCommand,  SEC_PLAYER, Console::No },
            { "custom", customCommandTable }
        };

        static ChatCommandTable commandTable =
        {
            { "autotalent", autoTalentCommandTable }
        };

        return commandTable;
    }

private:
    static Player* GetPlayer(ChatHandler* handler)
    {
        return handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    }

    static bool ParseSlot(char const* args, unsigned int& slotInput)
    {
        return args && std::sscanf(args, "%u", &slotInput) == 1 && slotInput >= 1 && slotInput <= 2;
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

        handler->SendSysMessage("Auto Talents - available prebuilt builds:");
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

        uint32 guid = player->GetGUID().GetCounter();
        handler->PSendSysMessage("Auto Talents - active talent slot: {}", uint32(player->GetActiveSpec() + 1));

        for (uint8 slot = 0; slot < 2; ++slot)
        {
            AutoTalentAssignment assignment = sAutoTalentMgr->GetAssignment(guid, slot);
            AutoTalentPersonalBuildInfo personal = sAutoTalentMgr->GetPersonalBuildInfo(guid, slot);

            if (!assignment.Found)
            {
                handler->PSendSysMessage("  Spec {}: no auto-talent build assigned{}", uint32(slot + 1),
                    personal.Found ? " (personal build is saved but not selected)" : "");
                continue;
            }

            if (assignment.BuildType == AutoTalentBuildType::Personal)
            {
                handler->PSendSysMessage("  Spec {}: Personal - {} (saved {} time(s))", uint32(slot + 1),
                    personal.Found ? personal.Name : "missing", personal.SaveCount);
                continue;
            }

            AutoTalentBuild const* build = sAutoTalentMgr->GetBuild(assignment.BuildId);
            if (!build)
                handler->PSendSysMessage("  Spec {}: prebuilt build {} is missing", uint32(slot + 1), assignment.BuildId);
            else
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
        if (!ParseSlot(args, slotInput))
        {
            handler->SendSysMessage("Usage: .autotalent clear <1|2>");
            return true;
        }

        std::string error;
        if (!sAutoTalentMgr->ClearAssignment(player, uint8(slotInput - 1), error))
            handler->PSendSysMessage("Auto Talents: {}", error);
        else
            handler->PSendSysMessage("Auto Talents: Spec {} assignment cleared.", slotInput);
        return true;
    }

    static bool HandleCustomPriceCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!ParseSlot(args, slotInput))
        {
            handler->SendSysMessage("Usage: .autotalent custom price <1|2>");
            return true;
        }

        uint32 cost = sAutoTalentMgr->GetNextPersonalBuildCost(player->GetGUID().GetCounter(), uint8(slotInput - 1));
        handler->PSendSysMessage("Auto Talents: next personal-build save for Spec {} costs {} copper ({}g {}s {}c).",
            slotInput, cost, cost / 10000, (cost / 100) % 100, cost % 100);
        return true;
    }

    static bool HandleCustomCloneCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        unsigned int buildId = 0;
        int consumed = 0;
        if (!args || std::sscanf(args, "%u %u %n", &slotInput, &buildId, &consumed) < 2 || slotInput < 1 || slotInput > 2)
        {
            handler->SendSysMessage("Usage: .autotalent custom clone <1|2> <prebuiltBuildId> [name]");
            return true;
        }

        std::string name;
        if (consumed > 0 && args[consumed] != '\0')
            name = std::string(args + consumed);

        uint32 charged = 0;
        std::string error;
        if (!sAutoTalentMgr->ClonePrebuiltToPersonal(player, uint8(slotInput - 1), uint32(buildId), name, charged, error))
        {
            handler->PSendSysMessage("Auto Talents: {}", error);
            return true;
        }

        handler->PSendSysMessage("Auto Talents: personal build saved to Spec {} for {} copper ({}g {}s {}c) and selected.",
            slotInput, charged, charged / 10000, (charged / 100) % 100, charged % 100);
        if (player->GetActiveSpec() == slotInput - 1)
            sAutoTalentMgr->HandleReconcileTrigger(player, "personal-build-save");
        return true;
    }

    static bool HandleCustomUseCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!ParseSlot(args, slotInput))
        {
            handler->SendSysMessage("Usage: .autotalent custom use <1|2>");
            return true;
        }

        std::string error;
        if (!sAutoTalentMgr->SetPersonalAssignment(player, uint8(slotInput - 1), error))
        {
            handler->PSendSysMessage("Auto Talents: {}", error);
            return true;
        }

        handler->PSendSysMessage("Auto Talents: Spec {} assigned to its saved personal build.", slotInput);
        if (player->GetActiveSpec() == slotInput - 1)
            sAutoTalentMgr->HandleReconcileTrigger(player, "assignment-change");
        return true;
    }
};

void AddSC_AutoTalentCommands()
{
    new AutoTalentCommands();
}
