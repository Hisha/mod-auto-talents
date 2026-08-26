# mod-auto-talents

Server-side automatic talent leveling for AzerothCore 3.3.5a.

## Current milestone

This first development milestone provides the framework only:

- Server-owned predefined build records.
- One build assignment for each native dual-talent slot.
- `.autotalent list`, `.autotalent status`, `.autotalent set`, and `.autotalent clear`.
- Reconciliation triggers on login, level-up, and dual-spec change.
- Persistent character assignments.
- Paladin Protection and Retribution placeholder build definitions.

It intentionally does **not** spend or reset talent points yet. The next milestone adds the ordered talent application/reconciliation engine after this framework is verified to compile and load correctly.

## SQL policy

Shipped base SQL files represent the complete current schema. Long-term module SQL should use complete `CREATE TABLE` definitions and seed `DELETE`/`INSERT` statements. Temporary `ALTER TABLE` statements may be supplied during development when useful, but they are not retained as the permanent install schema.

## Installation

Clone or copy the module under AzerothCore's `modules` directory, apply the world and characters SQL files, re-run CMake/build, install, and restart worldserver.

World DB:

```bash
mysql -u <user> -p acore_world < modules/mod-auto-talents/data/sql/db-world/base/auto_talent_builds.sql
```

Characters DB:

```bash
mysql -u <user> -p acore_characters < modules/mod-auto-talents/data/sql/db-characters/base/auto_talent_characters.sql
```

Copy the installed `mod_auto_talents.conf.dist` to `mod_auto_talents.conf` if you want local overrides.

## Commands

```text
.autotalent list
.autotalent status
.autotalent set <1|2> <buildId>
.autotalent clear <1|2>
```

For the initial Paladin test data:

```text
201 = Protection
202 = Retribution
```

Example:

```text
.autotalent set 1 202
.autotalent set 2 201
.autotalent status
```

Set `AutoTalents.Debug = 1` while testing to log login, level-up, spec-change, and assignment-change reconciliation triggers.
