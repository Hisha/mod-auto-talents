# mod-auto-talents

Server-side automatic talent leveling for AzerothCore WotLK 3.3.5a.

Players select a predefined build for either native dual-talent slot. The active slot is reconciled automatically on login, level-up, spec change, or when assigning a build to the currently active slot. No client addon is required.

## Commands

- `.autotalent list`
- `.autotalent status`
- `.autotalent set <1|2> <buildId>`
- `.autotalent clear <1|2>`

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

The v0.3 library uses leveling-oriented WotLK Classic progression, with Icy Veins leveling guides used as the primary reference for leveling priorities and the 3.3.5 talent-tree structure used to keep the data server-valid.

## Build data

Build definitions live in the world database. `auto_talent_build_step` stores a talent name and human-readable rank for each ordered point. At startup the module resolves each name against AzerothCore's loaded 3.3.5 talent/spell data and disables a build if a referenced talent is unknown or belongs to the wrong class.

This keeps future server/admin editing readable without requiring hard-coded Talent.dbc or spell IDs.

## SQL policy

Distributed SQL files contain complete current `CREATE TABLE` definitions and seed data implemented with `DELETE` / `INSERT`. Permanent upgrade chains containing `ALTER TABLE` are intentionally not shipped.
