#include "AutoTalentMgr.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <cstdio>
#include <string>
#include <vector>

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

        static ChatCommandTable uiCommandTable =
        {
            { "load",   HandleUiLoadCommand,   SEC_PLAYER, Console::No },
            { "begin",  HandleUiBeginCommand,  SEC_PLAYER, Console::No },
            { "add",    HandleUiAddCommand,    SEC_PLAYER, Console::No },
            { "commit", HandleUiCommitCommand, SEC_PLAYER, Console::No },
            { "cancel", HandleUiCancelCommand, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable autoTalentCommandTable =
        {
            { "list",   HandleListCommand,   SEC_PLAYER, Console::No },
            { "status", HandleStatusCommand, SEC_PLAYER, Console::No },
            { "set",    HandleSetCommand,    SEC_PLAYER, Console::No },
            { "clear",  HandleClearCommand,  SEC_PLAYER, Console::No },
            { "custom", customCommandTable },
            { "ui",     uiCommandTable }
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

    static std::string ProtocolSafe(std::string value)
    {
        for (char& ch : value)
        {
            if (ch == '|')
                ch = '/';
            else if (ch == '\r' || ch == '\n')
                ch = ' ';
        }
        return value;
    }

    static void SendUiError(ChatHandler* handler, std::string const& error)
    {
        handler->PSendSysMessage("ATUI|ERROR|{}", ProtocolSafe(error));
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

    static bool HandleUiLoadCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!ParseSlot(args, slotInput))
        {
            SendUiError(handler, "Usage: .autotalent ui load <1|2>");
            return true;
        }

        uint8 slot = uint8(slotInput - 1);
        uint32 guid = player->GetGUID().GetCounter();
        uint32 cost = sAutoTalentMgr->GetNextPersonalBuildCost(guid, slot);
        AutoTalentPersonalBuildInfo info = sAutoTalentMgr->GetPersonalBuildInfo(guid, slot);

        if (!info.Found)
        {
            handler->PSendSysMessage("ATUI|LOADBEGIN|{}|0|0|{}|Personal Build", slotInput, cost);
            handler->PSendSysMessage("ATUI|LOADEND|{}", slotInput);
            return true;
        }

        AutoTalentBuild build;
        std::string error;
        if (!sAutoTalentMgr->LoadPersonalBuild(guid, slot, build, error))
        {
            SendUiError(handler, error);
            return true;
        }

        handler->PSendSysMessage("ATUI|LOADBEGIN|{}|1|{}|{}|{}", slotInput, info.SaveCount, cost, ProtocolSafe(info.Name));
        for (AutoTalentBuildStep const& step : build.Steps)
            handler->PSendSysMessage("ATUI|STEP|{}|{}|{}|{}", slotInput, uint32(step.Sequence), uint32(step.Rank), ProtocolSafe(step.TalentName));
        handler->PSendSysMessage("ATUI|LOADEND|{}", slotInput);
        return true;
    }

    static bool HandleUiBeginCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        int consumed = 0;
        if (!args || std::sscanf(args, "%u %n", &slotInput, &consumed) < 1 || slotInput < 1 || slotInput > 2)
        {
            SendUiError(handler, "Usage: .autotalent ui begin <1|2> [name]");
            return true;
        }

        std::string name = consumed > 0 && args[consumed] != '\0' ? std::string(args + consumed) : "Personal Build";
        std::string error;
        if (!sAutoTalentMgr->BeginPersonalBuildDraft(player, uint8(slotInput - 1), name, error))
            SendUiError(handler, error);
        else
            handler->PSendSysMessage("ATUI|BEGINOK|{}", slotInput);
        return true;
    }

    static bool HandleUiAddCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        unsigned int sequence = 0;
        unsigned int rank = 0;
        int consumed = 0;
        if (!args || std::sscanf(args, "%u %u %u %n", &slotInput, &sequence, &rank, &consumed) < 3 ||
            slotInput < 1 || slotInput > 2 || sequence < 1 || sequence > 71 || rank < 1 || rank > 5 ||
            consumed <= 0 || args[consumed] == '\0')
        {
            SendUiError(handler, "Usage: .autotalent ui add <1|2> <sequence> <rank> <talent name>");
            return true;
        }

        std::string talentName(args + consumed);
        std::string error;
        if (!sAutoTalentMgr->AddPersonalBuildDraftStep(player, uint8(slotInput - 1), uint16(sequence), uint8(rank), talentName, error))
            SendUiError(handler, error);
        else if (sequence == 71)
            handler->PSendSysMessage("ATUI|STEPSOK|{}|71", slotInput);
        return true;
    }

    static bool HandleUiCommitCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!ParseSlot(args, slotInput))
        {
            SendUiError(handler, "Usage: .autotalent ui commit <1|2>");
            return true;
        }

        uint32 charged = 0;
        std::string error;
        if (!sAutoTalentMgr->CommitPersonalBuildDraft(player, uint8(slotInput - 1), charged, error))
        {
            SendUiError(handler, error);
            return true;
        }

        AutoTalentPersonalBuildInfo info = sAutoTalentMgr->GetPersonalBuildInfo(player->GetGUID().GetCounter(), uint8(slotInput - 1));
        handler->PSendSysMessage("ATUI|SAVEOK|{}|{}|{}|{}", slotInput, charged, info.SaveCount, ProtocolSafe(info.Name));
        if (player->GetActiveSpec() == slotInput - 1)
            sAutoTalentMgr->HandleReconcileTrigger(player, "personal-build-ui-save");
        return true;
    }

    static bool HandleUiCancelCommand(ChatHandler* handler, char const* args)
    {
        Player* player = GetPlayer(handler);
        if (!player)
            return false;

        unsigned int slotInput = 0;
        if (!ParseSlot(args, slotInput))
        {
            SendUiError(handler, "Usage: .autotalent ui cancel <1|2>");
            return true;
        }

        sAutoTalentMgr->CancelPersonalBuildDraft(player->GetGUID().GetCounter(), uint8(slotInput - 1));
        handler->PSendSysMessage("ATUI|CANCELOK|{}", slotInput);
        return true;
    }
};

void AddSC_AutoTalentCommands()
{
    new AutoTalentCommands();
}
