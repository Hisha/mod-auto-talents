#include "AutoTalentMgr.h"

#include "ScriptMgr.h"

class AutoTalentWorldScript : public WorldScript
{
public:
    AutoTalentWorldScript() : WorldScript("AutoTalentWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sAutoTalentMgr->LoadConfig();
    }

    void OnStartup() override
    {
        if (sAutoTalentMgr->IsEnabled())
            sAutoTalentMgr->LoadBuilds();
    }
};

void AddSC_AutoTalentWorldScript()
{
    new AutoTalentWorldScript();
}
