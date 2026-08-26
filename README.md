# mod-auto-talents

Server-side automatic talent leveling for AzerothCore 3.3.5a.

## Current scope

Players choose a server-provided build for each native talent spec slot. When the player logs in, gains a level, changes active dual-spec slot, or changes the assignment for the active slot, the module reconciles the active talent tree to the selected build through the number of talent points currently available.

The module does not require a client addon. Build definitions and their exact one-point-at-a-time order live in the world database.

## Built-in test builds

The first complete data set is Paladin:

- `201` - Protection (`0/53/18` at level 80)
- `202` - Retribution (`0/16/55` at level 80)

These are intended as useful leveling builds and, more importantly, to prove the complete automatic progression engine before adding the remaining classes/specs.

## Commands

```text
.autotalent list
.autotalent status
.autotalent set <1|2> <buildId>
.autotalent clear <1|2>
```

Examples:

```text
.autotalent set 1 202
.autotalent set 2 201
```

If the assigned slot is active, `set` reconciles immediately. An inactive dual-spec slot is left untouched until it becomes active.

## Reconciliation behavior

The same reconciliation operation is used for login, level-up, active spec change, and active assignment change.

- If the current active tree exactly matches the scripted prefix, only newly available points are learned.
- If the active tree does not match the selected scripted prefix, that active spec is reset for free and rebuilt through the current available talent-point count.
- Only the active native dual-spec slot is ever modified.
- AzerothCore's normal `LearnTalent()` path applies each point, so normal class, prerequisite, tier, rank, and free-point checks remain authoritative.

## SQL layout

World database:

- `auto_talent_build`
- `auto_talent_build_step`

Characters database:

- `auto_talent_character`

The shipped SQL files are complete current-state base files. Permanent module SQL uses complete `CREATE` definitions plus `DELETE`/`INSERT` seed maintenance; shipped schema evolution is not represented as a chain of `ALTER` statements.

For build authoring, `auto_talent_build_step.talent_id` accepts either a real `Talent.dbc` TalentID or any spell ID belonging to that talent. Spell references are normalized to the proper TalentID when the server loads the definitions. `rank` is human-readable and 1-based.

## Configuration

```ini
AutoTalents.Enable = 1
AutoTalents.Debug = 0
AutoTalents.LoginMessage = 0
```

`AutoTalents.Debug = 1` logs validation and actual reconciliation activity. Characters with no assigned build are intentionally not logged, preventing playerbot startup spam.

## Installation / test update

Apply the complete world base SQL after updating this milestone so the two Paladin builds receive their ordered steps:

```bash
mysql -u root -p acore_world < modules/mod-auto-talents/data/sql/db-world/base/auto_talent_builds.sql
```

The character schema did not change in this milestone.
