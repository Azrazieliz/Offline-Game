-- Offline-Game authoritative-state bootstrap.
-- Keep migration files immutable after release. Corrections require a new migration.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version             INTEGER PRIMARY KEY,
    name                TEXT NOT NULL,
    applied_at_utc      TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS entities (
    id                  TEXT PRIMARY KEY,
    kind                TEXT NOT NULL,
    created_world_tick  INTEGER NOT NULL DEFAULT 0,
    retired_world_tick  INTEGER,
    revision            INTEGER NOT NULL DEFAULT 0,
    state_json          TEXT NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_entities_kind
    ON entities(kind);

CREATE TABLE IF NOT EXISTS world_events (
    event_id             TEXT PRIMARY KEY,
    event_type           TEXT NOT NULL,
    world_tick           INTEGER NOT NULL,
    primary_entity_id    TEXT,
    related_entities_json TEXT NOT NULL DEFAULT '[]',
    payload_json         TEXT NOT NULL DEFAULT '{}',
    chronicle_eligible   INTEGER NOT NULL DEFAULT 0 CHECK (chronicle_eligible IN (0, 1)),
    FOREIGN KEY(primary_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_world_events_tick
    ON world_events(world_tick);

CREATE INDEX IF NOT EXISTS idx_world_events_primary
    ON world_events(primary_entity_id, world_tick);

CREATE TABLE IF NOT EXISTS knowledge_facts (
    owner_entity_id      TEXT NOT NULL,
    fact_key             TEXT NOT NULL,
    subject_entity_id    TEXT NOT NULL DEFAULT '',
    value_json           TEXT NOT NULL DEFAULT '{}',
    learned_world_tick   INTEGER NOT NULL,
    updated_world_tick   INTEGER NOT NULL,
    PRIMARY KEY(owner_entity_id, fact_key, subject_entity_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS content_packages (
    package_id           TEXT PRIMARY KEY,
    version              INTEGER NOT NULL,
    content_hash         TEXT NOT NULL,
    installed            INTEGER NOT NULL DEFAULT 0 CHECK (installed IN (0, 1)),
    validated            INTEGER NOT NULL DEFAULT 0 CHECK (validated IN (0, 1)),
    activated            INTEGER NOT NULL DEFAULT 0 CHECK (activated IN (0, 1)),
    manifest_json        TEXT NOT NULL DEFAULT '{}'
);

INSERT OR IGNORE INTO schema_migrations(version, name, applied_at_utc)
VALUES (1, 'bootstrap', strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));
