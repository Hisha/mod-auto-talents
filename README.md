# mod-auto-talents

Server-side automatic talent leveling for AzerothCore WotLK 3.3.5a.

Players can select a predefined server build for either native dual-talent slot. The active slot is reconciled automatically on login, level-up, spec change, or when assigning a build to the currently active slot. Prebuilt builds are always free.

v0.4 added one personal/custom ordered build per character/spec slot with server-controlled pricing. v0.5 added the optional **AutoTalentsUI** 3.3.5a companion addon and personal talent planner. v0.6 adds a server-backed trainer manager protocol so the addon can display and change prebuilt, personal, or disabled assignments without hard-coded build IDs.

The addon is optional. Players without it retain all prebuilt-build and command functionality.

## Player commands

Prebuilt builds:

- `.autotalent list`
- `.autotalent status`
- `.autotalent set <1|2> <buildId>`
- `.autotalent clear <1|2>`

Personal-build test/admin-friendly commands:

- `.autotalent custom price <1|2>`
- `.autotalent custom clone <1|2> <prebuiltBuildId> [name]`
- `.autotalent custom use <1|2>`

The `.autotalent ui ...` command family is the transport used by the optional addon. It stages a 71-point ordered plan in server memory and commits it only after the complete sequence has arrived.

## Optional AutoTalentsUI addon

AutoTalentsUI is distributed separately from the server module. At a normal class trainer it adds a single **Auto Talents** button. The manager opened from that button receives the player's available prebuilt builds and current Spec 1 / Spec 2 assignments from the server. Players can select a free prebuilt build, edit/use the slot's saved personal build, or disable Auto Talents for that slot. Spec 2 is only shown by the addon after Dual Talent Specialization is unlocked.

The personal planner uses the client's own Wrath talent data through `GetNumTalentTabs`, `GetTalentTabInfo`, `GetTalentInfo`, and `GetTalentPrereqs`. The exact order of planned points becomes the leveling sequence.

The addon communicates with the module using player chat commands and tagged system-message responses. Protocol messages are filtered from the visible chat frame by the addon. The server remains authoritative for available prebuilt builds, assignment, price, validation, storage, and talent application.

Convenience/testing commands:

- `/atui` opens the assignment manager.
- `/atui 1` opens the Spec 1 personal planner directly.
- `/atui 2` opens the Spec 2 personal planner directly.

## Personal-build pricing

Prebuilt build selection is always free. Creating or updating a personal build uses settings in `mod_auto_talents.conf`:

- `AutoTalents.CustomBuilds.Enable`
- `AutoTalents.CustomBuilds.CostMode` (`0` flat, `1` linear, `2` multiplier)
- `AutoTalents.CustomBuilds.BaseCost`
- `AutoTalents.CustomBuilds.CostIncrease`
- `AutoTalents.CustomBuilds.CostMultiplier`
- `AutoTalents.CustomBuilds.MaxCost`

The save counter is maintained per character/spec slot. Switching between an existing personal build and a prebuilt build does not increase the counter and does not cost money; only a successful personal-build create/update does.

## Built-in build IDs

| Class | Builds |
|---|---|
| Warrior | 101 Arms, 102 Fury, 103 Protection |
| Paladin | 201 Protection, 202 Retribution, 203 Holy |
| Hunter | 301 Beast Mastery, 302 Marksmanship, 303 Survival |
| Rogue | 401 Assassination, 402 Combat, 403 Subtlety |
| Priest | 501 Discipline, 502 Holy, 503 Shadow |
| Death Knight | 601 Blood, 602 Frost, 603 Unholy |
| Shaman | 701 Elemental, 702 Enhancement, 703 Restoration |
| Mage | 801 Arcane, 802 Fire, 803 Frost |
| Warlock | 901 Affliction, 902 Demonology, 903 Destruction |
| Druid | 1101 Balance, 1102 Feral, 1103 Restoration |

## Build data

Prebuilt definitions live in the world database. Personal definitions live in the characters database. Both use ordered talent-name/rank steps and are resolved against AzerothCore's loaded 3.3.5 talent/spell data before use.

The server requires personal data to contain the correct class, exactly 71 contiguous steps, valid talent names/ranks, and sequential ranks before accepting a save. Talent application itself uses AzerothCore's native `LearnTalent()` path, so the core remains authoritative when a saved sequence is applied.

## SQL policy

Distributed SQL files contain complete current `CREATE TABLE` definitions and seed data implemented with `DELETE` / `INSERT`. Permanent upgrade chains containing `ALTER TABLE` are intentionally not shipped.