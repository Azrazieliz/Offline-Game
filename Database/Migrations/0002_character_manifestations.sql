-- Ruler-specific ownership state for collectible characters.
-- Immutable character definitions remain in validated content packages.

CREATE TABLE IF NOT EXISTS character_manifestations (
    manifestation_entity_id  TEXT PRIMARY KEY,
    owning_ruler_entity_id    TEXT NOT NULL,
    identity_content_id       TEXT NOT NULL,
    active_version_content_id TEXT NOT NULL,
    level                     INTEGER NOT NULL DEFAULT 1 CHECK (level >= 1),
    current_rarity            TEXT NOT NULL DEFAULT '',
    progression_state_json    TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(manifestation_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owning_ruler_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_character_manifestations_owner
    ON character_manifestations(owning_ruler_entity_id);

CREATE INDEX IF NOT EXISTS idx_character_manifestations_identity
    ON character_manifestations(identity_content_id);
