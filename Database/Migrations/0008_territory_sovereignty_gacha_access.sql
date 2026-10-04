-- Migration 0008: overlapping Territory membership/claims, sovereignty state,
-- five-day reclamation persistence, and permanent first gacha unlock.

CREATE TABLE IF NOT EXISTS location_territories (
    location_entity_id TEXT NOT NULL,
    territory_entity_id TEXT NOT NULL,
    relation_kind TEXT NOT NULL DEFAULT 'contained',
    coverage_bps INTEGER NOT NULL DEFAULT 10000
        CHECK (coverage_bps BETWEEN 0 AND 10000),
    PRIMARY KEY(location_entity_id, territory_entity_id),
    FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id),
    FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_location_territories_territory
    ON location_territories(territory_entity_id, location_entity_id);

CREATE TABLE IF NOT EXISTS territory_claims (
    claim_entity_id TEXT PRIMARY KEY,
    territory_entity_id TEXT NOT NULL,
    ruler_entity_id TEXT NOT NULL,
    claim_kind TEXT NOT NULL,
    control_state TEXT NOT NULL,
    control_strength_bps INTEGER NOT NULL DEFAULT 0
        CHECK (control_strength_bps BETWEEN 0 AND 10000),
    claim_start_world_tick INTEGER NOT NULL,
    effective_control_start_world_tick INTEGER,
    displaced_world_tick INTEGER,
    reclaim_deadline_world_tick INTEGER,
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    UNIQUE(territory_entity_id, ruler_entity_id),
    FOREIGN KEY(claim_entity_id) REFERENCES entities(id),
    FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id),
    FOREIGN KEY(ruler_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_territory_claims_territory
    ON territory_claims(territory_entity_id, control_state, updated_world_tick);

CREATE INDEX IF NOT EXISTS idx_territory_claims_ruler
    ON territory_claims(ruler_entity_id, control_state, updated_world_tick);

CREATE TABLE IF NOT EXISTS ruler_sovereignty_state (
    ruler_entity_id TEXT PRIMARY KEY,
    current_title TEXT NOT NULL DEFAULT 'ruler',
    historical_peak_title TEXT NOT NULL DEFAULT 'ruler',
    continuous_control_start_world_tick INTEGER,
    last_effective_control_world_tick INTEGER,
    scope_state_json TEXT NOT NULL DEFAULT '{}',
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(ruler_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS ruler_gacha_access (
    ruler_entity_id TEXT PRIMARY KEY,
    qualification_start_world_tick INTEGER,
    qualification_suspended_world_tick INTEGER,
    unlocked_world_tick INTEGER,
    permanently_unlocked INTEGER NOT NULL DEFAULT 0
        CHECK (permanently_unlocked IN (0,1)),
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(ruler_entity_id) REFERENCES entities(id)
);

-- Deterministic legacy Territory projection, prior-gacha access preservation
-- and schema/application validation are performed by migration callbacks.
