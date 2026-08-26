#include "AutoTalentMgr.h"

#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"

class AutoTalentPlayerScript : public PlayerScript
{
public:
    AutoTalentPlayerScript() : PlayerScript("AutoTalentPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_AFTER_SPEC_SLOT_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!sAutoTalentMgr->IsEnabled())
            return;

        if (sAutoTalentMgr->IsLoginMessageEnabled())
            ChatHandler(player->GetSession()).SendSysMessage("Auto Talents is enabled on this server.");

        sAutoTalentMgr->HandleReconcileTrigger(player, "login");
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!sAutoTalentMgr->IsEnabled() || oldLevel >= player->GetLevel())
            return;

        sAutoTalentMgr->HandleReconcileTrigger(player, "level-up");
    }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 /*newSlot*/) override
    {
        if (!sAutoTalentMgr->IsEnabled())
            return;

        sAutoTalentMgr->HandleReconcileTrigger(player, "spec-change");
    }
};

void AddSC_AutoTalentPlayerScript()
{
    new AutoTalentPlayerScript();
}
