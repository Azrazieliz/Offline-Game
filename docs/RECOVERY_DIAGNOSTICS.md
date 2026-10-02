# Recovery and Diagnostics

Phase G keeps save recovery deliberately small and explicit.

## Rotating snapshots

At application startup, an existing authoritative SQLite save receives a full
online backup. The runtime retains the three newest snapshots.

Snapshots are SQLite databases created through the SQLite backup API. They are
not Unreal SaveGame blobs and they do not fork canonical state.

A fresh first-run database is not snapshotted before it has meaningful state.

## Diagnostics bundle

The game core can write a compact JSON diagnostics bundle containing:

- UTC generation time;
- Unreal Engine version;
- OS version;
- database schema version;
- SQLite integrity-check result;
- database file name and byte size.

The bundle intentionally excludes absolute save paths and gameplay/save payloads
so it is suitable for sharing during debugging without exposing world content.

The diagnostic generator does not silently repair state. Repair remains a
separate explicit developer/player-support action.
