#ifndef MOD_AUTO_TALENTS_MGR_H
#define MOD_AUTO_TALENTS_MGR_H

#include "Define.h"

#include <map>
#include <string>
#include <vector>

class Player;

struct AutoTalentBuildStep
{
    uint16 Sequence = 0;
    uint32 TalentId = 0;
    uint8 Rank = 0; // Human-readable rank: 1..5, not zero-based.
};

struct AutoTalentBuild
{
    uint32 Id = 0;
    uint8 ClassId = 0;
    std::string Name;
    std::string Description;
    bool Enabled = false;
    std::vector<AutoTalentBuildStep> Steps;
};

struct AutoTalentAssignment
{
    bool Found = false;
    uint32 BuildId = 0;
};

class AutoTalentMgr
{
public:
    static AutoTalentMgr* instance();

    void LoadConfig();
    void LoadBuilds();

    bool IsEnabled() const { return _enabled; }
    bool IsDebugEnabled() const { return _debug; }
    bool IsLoginMessageEnabled() const { return _loginMessage; }

    AutoTalentBuild const* GetBuild(uint32 buildId) const;
    std::vector<AutoTalentBuild const*> GetBuildsForClass(uint8 classId) const;

    AutoTalentAssignment GetAssignment(uint32 guid, uint8 specSlot) const;
    bool SetAssignment(Player* player, uint8 specSlot, uint32 buildId, std::string& error);
    bool ClearAssignment(Player* player, uint8 specSlot, std::string& error);

    void HandleReconcileTrigger(Player* player, char const* reason);

private:
    AutoTalentMgr() = default;

    bool _enabled = true;
    bool _debug = false;
    bool _loginMessage = false;

    std::map<uint32, AutoTalentBuild> _builds;
};

#define sAutoTalentMgr AutoTalentMgr::instance()

#endif
