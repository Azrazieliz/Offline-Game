# Persistence, Content Packages and Android Delivery

STATUS: ARCHITECTURE FROZEN FOR RECONCILIATION.

## 1. Content distribution

Use a hybrid structure:
- core application/runtime;
- independently installable/updatable region packages;
- character/media packages;
- event/sealed packages;
- shared asset/dependency packages.

Once installed, gameplay content works offline indefinitely without login/authentication/server dependency.

## 2. Updating

The app may check for updates when internet is available.

Small metadata/package-index checks may be automatic.

Large content downloads default to **automatic download on Wi-Fi/unmetered connections**, with a user setting to disable or further restrict that behavior. Metered/mobile-data downloads require explicit user permission unless the user changes the preference.

Update availability never blocks already-installed offline content merely because the device is offline.

## 3. Storage

Installed-content planning follows `docs/STORAGE_QUALITY_BUDGET.md`.

The standard-quality full-install target is **25-35 GB**. A **35-50 GB** full install is acceptable only when measured production content warrants it after duplication/reuse/compression review. **50 GB** is the soft warning threshold. **60 GB** is the hard installed-content ceiling unless the Creative Director explicitly approves an exception backed by physical Galaxy S26 Ultra profiling and a documented quality/content justification.

The former ~100 GB planning ceiling is superseded.

These installed-size limits apply to shipped/cooked runtime content, not source-production masters, DCC files, lossless audio masters, layered textures, local build intermediates, caches, saves, or optional extra-language/ultra-resolution packs that are not part of the standard-quality full install.

Large/rarely used packages may be placed in Android-permitted user-selected external/shared storage where practical.

Hot/latency-sensitive assets remain in the optimal local location.

The package manager tracks:
- stable package ID/version;
- dependencies;
- hashes;
- installation location;
- activation state;
- sealed/visible state;
- compatibility.

## 4. Mods

General user mod support is outside the initial architecture.

The package format remains clean/validated enough that custom-content support could be added later without redesigning persistence.

## 5. Save continuity

Retain:
- one canonical persistent world/history;
- automatic rotating snapshots;
- manual Export;
- user-visible external backup location;
- optional secondary archive/cloud copy;
- no cloud dependency.

On migration failure:
1. preserve the untouched pre-migration save;
2. attempt migration/recovery on a copy;
3. validate integrity;
4. refuse normal continuation rather than silently corrupting the sole history.

Clear World/new-world deletion does not automatically erase external backup archives. Backup deletion is a separate explicit action.

## 6. Orientation and Android UX

Portrait Ruler Mode / landscape World Mode switch automatically by default.

The player can lock orientation when desired.

Android notifications integrate with the Reports system as specified in docs/UI_PRESENTATION.md.


## Player-facing opening / recovery UX

Use a real title/opening presentation before the main world.

Primary actions:
- Continue;
- Recover Existing World;
- Import Backup;
- Settings.

Clear World is destructive/secondary and must not read like a routine New Game action.

Backup UI exposes automatic/manual snapshots, date/time, world/schema/build information, integrity state and explicit Export/Import/Restore.

Package manager exposes installed package size/version/location/update state plus move/archive controls where supported.

Metadata/update checks may occur automatically online.

Large package downloads default to **automatic Wi-Fi/unmetered download**; user settings may disable or further constrain it.
