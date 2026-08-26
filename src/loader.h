#ifndef MOD_AUTO_TALENTS_LOADER_H
#define MOD_AUTO_TALENTS_LOADER_H

void AddSC_AutoTalentPlayerScript();
void AddSC_AutoTalentWorldScript();
void AddSC_AutoTalentCommands();

void AddAutoTalentsScripts()
{
    AddSC_AutoTalentWorldScript();
    AddSC_AutoTalentPlayerScript();
    AddSC_AutoTalentCommands();
}

#endif
