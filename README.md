# mod-auto-talents

Server-side automatic talent leveling for AzerothCore WotLK 3.3.5a.

Players can select a predefined server build for either native dual-talent slot. The active slot is reconciled automatically on login, level-up, spec change, or when assigning a build to the currently active slot. Prebuilt builds are always free.

v0.4 adds the server-side foundation for one personal/custom ordered build per character/spec slot. Personal builds are stored in the characters database, can be assigned independently to Spec 1 or Spec 2, use the same reconciliation engine as prebuilt builds, and can have a server-configurable save price. The future optional client addon will submit the player's ordered 71-point plan through this same manager API.

## Commands

Prebuilt builds:

- `.autotalent list`
- `.autotalent status`
- `.autotalent set <1|2> <buildId>`
- `.autotalent clear <1|2>`

Personal-build test commands:

- `.autotalent custom price <1|2>` - show the price of the next successful personal-build save for that slot.
- `.autotalent custom clone <1|2> <prebuiltBuildId> [name]` - copy a valid prebuilt 71-point sequence into the slot's personal build, charge the configured save price, select it, and reconcile immediately if the slot is active.
- `.autotalent custom use <1|2>` - select the already-saved personal build for that slot without charging a save fee.

`custom clone` is primarily a v0.4 server-infrastructure test command. The planned addon will replace cloning with a native-style talent planner and call `SavePersonalBuild()` with the player's ordered choices.

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

The server checks personal data for the correct class, exactly 71 contiguous steps, valid talent names/ranks, and sequential ranks before accepting a save. Talent application itself continues to use AzerothCore's native `LearnTalent()` path, so the core remains authoritative when a saved sequence is applied.

## SQL policy

Distributed SQL files contain complete current `CREATE TABLE` definitions and seed data implemented with `DELETE` / `INSERT`. Permanent upgrade chains containing `ALTER TABLE` are intentionally not shipped.
