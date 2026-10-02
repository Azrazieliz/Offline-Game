-- Shared physical location/presence state.
-- Truth is separate from knowledge/discovery.

CREATE TABLE IF NOT EXISTS locations (
    location_entity_id          TEXT PRIMARY KEY,
    parent_location_entity_id   TEXT,
    kind                        TEXT NOT NULL,
    territory_entity_id         TEXT,
    physically_accessible       INTEGER NOT NULL DEFAULT 1 CHECK (physically_accessible IN (0,1)),
    FOREIGN KEY(location_entity_id) REFERENCES entities(id),
    FOREIGN KEY(parent_location_entity_id) REFERENCES locations(location_entity_id),
    FOREIGN KEY(territory_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_locations_parent
    ON locations(parent_location_entity_id);

CREATE INDEX IF NOT EXISTS idx_locations_territory
    ON locations(territory_entity_id);

CREATE TABLE IF NOT EXISTS world_presence (
    entity_id             TEXT PRIMARY KEY,
    location_entity_id    TEXT NOT NULL,
    local_x               REAL NOT NULL DEFAULT 0,
    local_y               REAL NOT NULL DEFAULT 0,
    local_z               REAL NOT NULL DEFAULT 0,
    movement_context      TEXT NOT NULL DEFAULT '',
    updated_world_tick    INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(entity_id) REFERENCES entities(id),
    FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_world_presence_location
    ON world_presence(location_entity_id);

CREATE INDEX IF NOT EXISTS idx_knowledge_subject
    ON knowledge_facts(subject_entity_id, fact_key);
