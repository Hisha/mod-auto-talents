#ifndef MOD_AUTO_TALENTS_MGR_H
#define MOD_AUTO_TALENTS_MGR_H

#include "Define.h"

#include <map>
#include <string>
#include <vector>

class Player;

enum class AutoTalentBuildType : uint8
{
    Prebuilt = 0,
    Personal = 1
};

struct AutoTalentBuildStep
{
    uint16 Sequence = 0;
    std::string TalentName;
    uint32 TalentId = 0; // Resolved Talent.dbc ID, populated when a build is validated.
    uint8 Rank = 0; // Human-readable rank: 1..5, not zero-based.
};

struct AutoTalentBuild
{
    uint32 Id = 0;
    uint8 ClassId = 0;
    std::string Name;
    std::string Description;
    bool Enabled = false;
    bool Personal = false;
    std::vector<AutoTalentBuildStep> Steps;
};

struct AutoTalentAssignment
{
    bool Found = false;
    AutoTalentBuildType BuildType = AutoTalentBuildType::Prebuilt;
    uint32 BuildId = 0;
};

struct AutoTalentPersonalBuildInfo
{
    bool Found = false;
    uint32 SaveCount = 0;
    std::string Name;
};

struct AutoTalentBuildDraft
{
    uint8 ClassId = 0;
    uint8 SpecSlot = 0;
    std::string Name;
    std::vector<AutoTalentBuildStep> Steps;
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
    bool ArePersonalBuildsEnabled() const { return _personalBuildsEnabled; }

    AutoTalentBuild const* GetBuild(uint32 buildId) const;
    std::vector<AutoTalentBuild const*> GetBuildsForClass(uint8 classId) const;

    AutoTalentAssignment GetAssignment(uint32 guid, uint8 specSlot) const;
    bool SetAssignment(Player* player, uint8 specSlot, uint32 buildId, std::string& error);
    bool SetPersonalAssignment(Player* player, uint8 specSlot, std::string& error);
    bool ClearAssignment(Player* player, uint8 specSlot, std::string& error);

    AutoTalentPersonalBuildInfo GetPersonalBuildInfo(uint32 guid, uint8 specSlot) const;
    bool LoadPersonalBuild(uint32 guid, uint8 specSlot, AutoTalentBuild& build, std::string& error) const;
    bool SavePersonalBuild(Player* player, uint8 specSlot, std::string const& name,
        std::vector<AutoTalentBuildStep> steps, uint32& chargedCost, std::string& error);
    bool ClonePrebuiltToPersonal(Player* player, uint8 specSlot, uint32 sourceBuildId,
        std::string const& name, uint32& chargedCost, std::string& error);
    uint32 GetNextPersonalBuildCost(uint32 guid, uint8 specSlot) const;

    bool BeginPersonalBuildDraft(Player* player, uint8 specSlot, std::string const& name, std::string& error);
    bool AddPersonalBuildDraftStep(Player* player, uint8 specSlot, uint16 sequence, uint8 rank,
        std::string const& talentName, std::string& error);
    bool CommitPersonalBuildDraft(Player* player, uint8 specSlot, uint32& chargedCost, std::string& error);
    void CancelPersonalBuildDraft(uint32 guid, uint8 specSlot);

    void HandleReconcileTrigger(Player* player, char const* reason);

private:
    AutoTalentMgr() = default;

    bool ValidateBuild(AutoTalentBuild& build, bool requireComplete, std::string& error) const;
    uint32 CalculatePersonalBuildCost(uint32 saveCount) const;
    static uint64 MakeDraftKey(uint32 guid, uint8 specSlot);

    bool _enabled = true;
    bool _debug = false;
    bool _loginMessage = false;

    bool _personalBuildsEnabled = true;
    uint8 _personalCostMode = 0;
    uint32 _personalBaseCost = 100000;
    uint32 _personalCostIncrease = 50000;
    float _personalCostMultiplier = 1.5f;
    uint32 _personalMaxCost = 0;

    std::map<uint32, AutoTalentBuild> _builds;
    std::map<uint64, AutoTalentBuildDraft> _drafts;
};

#define sAutoTalentMgr AutoTalentMgr::instance()

#endif
