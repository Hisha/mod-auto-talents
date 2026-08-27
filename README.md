# mod-auto-talents

Automatic talent leveling and personal talent loadouts for AzerothCore WotLK 3.3.5a.

`mod-auto-talents` lets players assign an ordered talent build to either native talent specialization. As the character levels, the module automatically spends newly available talent points in the saved order. When the player changes dual-spec slots, logs in, or otherwise needs reconciliation, the module applies the build appropriate to the active slot and current level.

The module ships with server-controlled prebuilt leveling builds for every class/spec and also supports one personal 71-point build per character/spec slot.

## Companion addon

For the full in-game graphical workflow, install the optional **AutoTalentsUI** companion addon:

https://github.com/Hisha/AutoTalentsUI

The addon is optional. Players without it can still use the server-provided prebuilt builds through `.autotalent` commands.

With AutoTalentsUI installed, a class trainer's normal gossip menu gains the native-looking option:

> I wish to adjust my personal talent build(s).

Selecting it opens the Auto Talents manager directly. Players do not need to enter the trainer's skill list first.

The manager allows the player to:

- view the current Auto Talents assignment for each available native talent spec;
- select one of the server-provided prebuilt leveling builds;
- create or edit a personal talent build with the graphical talent planner;
- switch back to an already-saved personal build without paying another save fee;
- disable Auto Talents for an individual spec slot.

Talent Spec 2 is exposed by the addon after Dual Talent Specialization is unlocked.

## Features

- Automatic talent spending while leveling.
- Independent assignments for native Talent Spec 1 and Talent Spec 2.
- Automatic reconciliation on login, level-up, spec change, and assignment change.
- Server-controlled prebuilt leveling builds for every WotLK class/spec.
- One personal/custom ordered 71-point build per character/spec slot.
- Configurable personal-build pricing.
- Server-side validation of all prebuilt and personal talent sequences.
- Optional graphical 3.3.5a companion addon.
- No client patch required.

## Installation

Clone the module into the AzerothCore `modules` directory:

```bash
cd ~/azerothcore-wotlk/modules
git clone https://github.com/Hisha/mod-auto-talents.git
```

Reconfigure/rebuild AzerothCore as required by your installation, install the rebuilt server, and restart `worldserver`.

The module uses AzerothCore's module CMake hooks to install:

```text
conf/mod_auto_talents.conf.dist
```

into the normal module configuration location.

### SQL

The module ships complete base SQL definitions under:

```text
data/sql/db-world/base/auto_talent_builds.sql
data/sql/db-characters/base/auto_talent_characters.sql
```

These contain the current table definitions and seed data used by the module.

Distributed SQL intentionally uses complete `CREATE TABLE` definitions and seed operations rather than maintaining a permanent chain of `ALTER TABLE` migrations.

## Player commands

### Prebuilt builds

```text
.autotalent list
.autotalent status
.autotalent set <1|2> <buildId>
.autotalent clear <1|2>
```

### Personal-build utility commands

```text
.autotalent custom price <1|2>
.autotalent custom clone <1|2> <prebuiltBuildId> [name]
.autotalent custom use <1|2>
```

The `.autotalent ui ...` command family is an internal transport used by AutoTalentsUI. The server remains authoritative for available builds, assignments, pricing, validation, storage, and talent application.

## Personal-build pricing

Prebuilt build selection is always free.

Creating or updating a personal build uses these settings in `mod_auto_talents.conf`:

```text
AutoTalents.CustomBuilds.Enable
AutoTalents.CustomBuilds.CostMode
AutoTalents.CustomBuilds.BaseCost
AutoTalents.CustomBuilds.CostIncrease
AutoTalents.CustomBuilds.CostMultiplier
AutoTalents.CustomBuilds.MaxCost
```

`AutoTalents.CustomBuilds.CostMode` supports:

- `0` - flat cost;
- `1` - linear increase per successful save;
- `2` - multiplier increase per successful save.

The successful-save counter is maintained per character/spec slot. Switching between an existing personal build and a prebuilt build does not increase the counter and does not cost money. Only a successful personal-build create/update is charged.

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

## Build data and validation

Prebuilt definitions live in the world database. Personal definitions live in the characters database.

Both use ordered talent-name/rank steps and are resolved against AzerothCore's loaded WotLK 3.3.5a talent/spell data before use.

For personal builds, the server validates the character class, exactly 71 contiguous ordered steps, valid talent names/ranks, sequential ranks, tree requirements, and prerequisites before accepting the save.

Talent application uses AzerothCore's native talent-learning path so the core remains authoritative when a sequence is applied.

## AutoTalentsUI protocol

AutoTalentsUI communicates with the module using player commands and tagged system-message responses. Protocol messages are filtered from the player's normal chat display by the addon.

The personal planner stages the ordered build in server memory while it is transmitted. The saved personal build is committed only after the complete sequence has arrived and passed server validation, preventing partial saves.

## Configuration

Main settings:

```text
AutoTalents.Enable
AutoTalents.Debug
AutoTalents.LoginMessage
AutoTalents.CustomBuilds.Enable
AutoTalents.CustomBuilds.CostMode
AutoTalents.CustomBuilds.BaseCost
AutoTalents.CustomBuilds.CostIncrease
AutoTalents.CustomBuilds.CostMultiplier
AutoTalents.CustomBuilds.MaxCost
```

See `conf/mod_auto_talents.conf.dist` for descriptions and defaults.

## AzerothCore Catalogue

This repository is intended to be discoverable through the AzerothCore module catalogue.

The current AzerothCore catalogue indexes GitHub repositories using the `azerothcore-module` topic. After publishing the repository, add this GitHub repository topic:

```text
azerothcore-module
```

On GitHub, open the repository page, choose **Manage topics**, add `azerothcore-module`, and save.

AzerothCore catalogue:

https://www.azerothcore.org/catalogue.html

Catalogue submission/help:

https://www.azerothcore.org/catalogue.html#/how-to

Adding an `icon.png` to the repository root is also recommended by AzerothCore so the module has an image in catalogue listings/details.

## Related project

**AutoTalentsUI** - optional World of Warcraft 3.3.5a graphical companion addon:

https://github.com/Hisha/AutoTalentsUI

## License

See [LICENSE](LICENSE).