# Recovery and Diagnostics

STATUS: RECONCILIATION REQUIRED BEFORE REAL VALIDATION GATE.

## 1. Current pre-reconciliation implementation

The existing runtime can:
- open the authoritative SQLite database;
- apply numbered migrations;
- create SQLite online backups;
- retain three app-private rotating snapshots;
- restore from a backup;
- run `PRAGMA integrity_check`;
- emit a compact diagnostics JSON bundle.

This foundation is useful, but its startup order is **not yet compliant** with the frozen recovery contract: the current GameCore opens the DB (and therefore applies migrations) before it creates the startup snapshot.

## 2. Required migration-safe startup

Before any future schema migration can modify the only canonical history:

1. detect the existing authoritative DB without migrating it;
2. create/preserve an untouched pre-migration backup outside the working DB;
3. migrate a working copy;
4. run schema/integrity/application validation;
5. atomically promote the validated copy;
6. retain the untouched prior version for recovery;
7. refuse unsafe continuation if migration/validation fails.

Migration work never silently destroys the only recoverable history.

## 3. Rotating recovery

Automatic rotating snapshots remain, but they are only one layer.

The production design also requires:
- user-visible/external protected backups;
- manual Export;
- Recover Existing World / Import Backup;
- several recent plus less-frequent historical snapshots;
- backup metadata/hash/schema version;
- explicit backup deletion separate from Clear World;
- optional secondary/cloud archive without cloud dependency.

SQLite backup remains the preferred state-copy primitive.

## 4. Diagnostics bundle

Diagnostics can include:
- UTC generation time;
- Unreal Engine version;
- OS/device information;
- database schema version;
- SQLite integrity result;
- database size/name;
- migration state;
- package/runtime versions;
- optional aggregate performance telemetry.

Diagnostics must not silently repair state.

Repair/import/recovery are explicit operations with audit/provenance.

## 5. Next engineering source

Exact persistence/package changes are specified in `RECONCILIATION_MIGRATION_MATRIX.md` and `PERSISTENCE_PACKAGING.md`.
