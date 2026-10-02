-- Persistent gacha pity/guarantee state and duplicate acquisition accounting.
-- Pull outcomes remain meaningful world events rather than a second history log.

ALTER TABLE character_manifestations
    ADD COLUMN duplicate_acquisition_count INTEGER NOT NULL DEFAULT 0
    CHECK (duplicate_acquisition_count >= 0);

CREATE TABLE IF NOT EXISTS gacha_states (
    ruler_entity_id          TEXT NOT NULL,
    pity_category            TEXT NOT NULL,
    pulls_since_top_rarity   INTEGER NOT NULL DEFAULT 0 CHECK (pulls_since_top_rarity >= 0),
    featured_guaranteed      INTEGER NOT NULL DEFAULT 0 CHECK (featured_guaranteed IN (0, 1)),
    total_pulls              INTEGER NOT NULL DEFAULT 0 CHECK (total_pulls >= 0),
    updated_world_tick       INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(ruler_entity_id, pity_category),
    FOREIGN KEY(ruler_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_gacha_states_ruler
    ON gacha_states(ruler_entity_id);
