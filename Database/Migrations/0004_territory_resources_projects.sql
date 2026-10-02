-- Minimal territory / Domain Core / resource / project state.
-- No workers, shifts, stockpile objects, taxes or settlement simulation.

CREATE TABLE IF NOT EXISTS territories (
    territory_entity_id      TEXT PRIMARY KEY,
    ruler_entity_id          TEXT,
    root_location_entity_id  TEXT NOT NULL,
    is_main                  INTEGER NOT NULL DEFAULT 0 CHECK (is_main IN (0,1)),
    population               INTEGER NOT NULL DEFAULT 0 CHECK (population >= 0),
    control_state            TEXT NOT NULL DEFAULT 'controlled',
    FOREIGN KEY(territory_entity_id) REFERENCES entities(id),
    FOREIGN KEY(ruler_entity_id) REFERENCES entities(id),
    FOREIGN KEY(root_location_entity_id) REFERENCES locations(location_entity_id)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_main_territory_per_ruler
    ON territories(ruler_entity_id)
    WHERE is_main = 1 AND ruler_entity_id IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_territories_location
    ON territories(root_location_entity_id);

CREATE TABLE IF NOT EXISTS domain_cores (
    core_entity_id              TEXT PRIMARY KEY,
    territory_entity_id         TEXT NOT NULL UNIQUE,
    controller_ruler_entity_id  TEXT,
    lifecycle                   INTEGER NOT NULL,
    durability_sig              INTEGER NOT NULL DEFAULT 0,
    durability_exp              INTEGER NOT NULL DEFAULT 0,
    max_durability_sig          INTEGER NOT NULL DEFAULT 0,
    max_durability_exp          INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(core_entity_id) REFERENCES entities(id),
    FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id),
    FOREIGN KEY(controller_ruler_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS domain_core_aspects (
    core_entity_id      TEXT NOT NULL,
    aspect_content_id   TEXT NOT NULL,
    grade               INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(core_entity_id, aspect_content_id),
    FOREIGN KEY(core_entity_id) REFERENCES domain_cores(core_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS resource_balances (
    owner_entity_id      TEXT NOT NULL,
    resource_content_id  TEXT NOT NULL,
    amount               INTEGER NOT NULL DEFAULT 0 CHECK (amount >= 0),
    PRIMARY KEY(owner_entity_id, resource_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS projects (
    project_entity_id       TEXT PRIMARY KEY,
    owner_entity_id         TEXT NOT NULL,
    location_entity_id      TEXT NOT NULL,
    project_type_content_id TEXT NOT NULL,
    status                  INTEGER NOT NULL,
    start_world_tick        INTEGER NOT NULL,
    resolve_world_tick      INTEGER NOT NULL,
    progress_bps            INTEGER NOT NULL DEFAULT 0 CHECK (progress_bps BETWEEN 0 AND 10000),
    payload_json            TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(project_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_projects_owner_status
    ON projects(owner_entity_id, status);

CREATE INDEX IF NOT EXISTS idx_projects_location
    ON projects(location_entity_id);
