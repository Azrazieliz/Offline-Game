# Storage Quality Budget

STATUS: AUTHORITATIVE STORAGE POLICY.

This document supersedes the former 60-80 GB normal installed-content target and the former ~100 GB practical upper bound.

## 1. Installed-content targets

For the **standard-quality full install** on the primary Android target:

- preferred target: **25-35 GB**;
- acceptable expansion band: **35-50 GB**, only when measured production content justifies it after duplication, reuse, compression and streaming review;
- soft warning threshold: **50 GB**;
- hard ceiling: **60 GB**.

Crossing 50 GB requires an explicit storage review before further content growth.

Crossing 60 GB is not permitted by default. An exception requires:
1. explicit Creative Director approval;
2. physical Samsung Galaxy S26 Ultra profiling;
3. measured cooked-package evidence;
4. a written explanation of why equivalent visible/audible quality cannot reasonably be preserved below the ceiling;
5. a package/archive strategy for the excess content.

## 2. What counts

The installed-content budget counts shipped/cooked runtime content in the standard-quality full install, including:
- base runtime/application payload;
- standard shared packages;
- standard regions/world packages;
- standard character/enemy/item assets;
- standard animation/VFX/audio/UI/cinematic runtime assets.

It does **not** count:
- source-production masters;
- DCC project files;
- layered source textures;
- lossless audio masters/stems not shipped at runtime;
- local build intermediates and caches;
- user save/backup growth;
- optional extra-language packs;
- optional ultra-resolution packs not included in the standard-quality install.

Optional packages still require their own measured size and must never be used to hide mandatory core content from the standard-install accounting.

## 3. Planning envelopes by content family

These are planning envelopes, not quotas. Shared dependencies are counted once at integration level.

| Content family | Planning envelope |
|---|---:|
| Characters + skins | 6-8 GB |
| Environments/worlds | 8-12 GB |
| Enemies/bosses | 2-4 GB |
| Weapons/items/equipment | 1-2 GB |
| Animation | 2-3 GB |
| VFX | 1.5-3 GB |
| Music | 0.5-1.5 GB |
| SFX + voice | 1-2.5 GB |
| UI + cinematics | 0.5-1.5 GB |
| Engine/runtime/data/other | 1-2 GB |

The category maxima are not intended to be summed as a mandatory allocation. The cumulative standard install remains governed by the global thresholds above.

## 4. High-value per-asset planning targets

Use these as review triggers, not automatic rejection rules:

- full playable hero visual/runtime package: typically **80-180 MB cooked**;
- ordinary incremental skin reusing body/rig/common assets: typically **20-70 MB cooked**;
- radically different premium skin: normally **<=100-150 MB cooked** unless measured evidence justifies more;
- weapon: typically **5-30 MB cooked**;
- large unique boss/monster: typically **100-300 MB cooked**;
- ordinary skill VFX package: typically **1-10 MB cooked**;
- signature Ultimate/World Fantasm VFX package: typically **10-50 MB cooked**;
- 3-5 minute compressed stereo music track: typically **6-12 MB**;
- adaptive music with several runtime stems: typically **20-60 MB**.

Production masters may be much larger. They are not the shipped-size target.

## 5. Quality-first optimization order

Storage reduction must follow this order:
1. remove accidental duplication;
2. share dependencies and master materials;
3. use modular kits, instancing and deterministic/procedural variation;
4. pack compatible channels and strip source/editor-only payloads;
5. use salience-based texture/audio/mesh resolution;
6. compress with platform-appropriate settings;
7. stream/archive content that need not be resident;
8. only then reduce approved visible/audible quality.

Do not lower signature-asset quality merely to hit an arbitrary category envelope when the same bytes can be recovered from duplication elsewhere.

## 6. Required accounting

Every integrated content artifact records:
- source bytes;
- interchange bytes;
- estimated cooked Android bytes before integration;
- measured cooked Android bytes after integration;
- shared dependency bytes;
- resident memory estimate/measurement where relevant;
- streaming/package group;
- optimization notes.

Master Integration owns the non-double-counted cumulative forecast. Engineering owns measured cooked/package size.

## 7. Package strategy

Do not plan one monolithic immutable install.

Prefer:
- compact base application/runtime;
- shared dependency packages;
- region/world packages;
- character/media packages;
- language/media option packs where appropriate;
- patch granularity that avoids replacing unrelated multi-gigabyte payloads.

Installed packages must remain usable offline after installation, consistent with `PERSISTENCE_PACKAGING.md`.

## 8. Authority

Measured cooked output and physical-device profiling are authoritative over estimates.

Any older document that describes 60-80 GB as the normal installed target or ~100 GB as the practical installed ceiling is superseded by this policy.
