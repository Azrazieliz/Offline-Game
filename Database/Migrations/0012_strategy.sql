-- Migration 0012: objective-faithful Dispatch, continuous War, Army
-- capability vectors, optional Project phases, civilization dimensions and
-- capability-grounded logistics.

ALTER TABLE dispatches
    ADD COLUMN risk_tolerance_bps INTEGER NOT NULL DEFAULT 5000
    CHECK(risk_tolerance_bps BETWEEN 0 AND 10000);
ALTER TABLE dispatches
    ADD COLUMN abort_policy_json TEXT NOT NULL DEFAULT '{}';
ALTER TABLE dispatches
    ADD COLUMN outcome_state TEXT NOT NULL DEFAULT '';
ALTER TABLE dispatches
    ADD COLUMN delay_until_world_tick INTEGER;

CREATE TABLE IF NOT EXISTS dispatch_objectives (
    dispatch_entity_id TEXT NOT NULL,
    objective_content_id TEXT NOT NULL,
    priority INTEGER NOT NULL DEFAULT 0,
    mandatory INTEGER NOT NULL DEFAULT 0 CHECK(mandatory IN (0,1)),
    target_entity_id TEXT,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(dispatch_entity_id, objective_content_id),
    FOREIGN KEY(dispatch_entity_id)
        REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(target_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_dispatch_objectives_priority
    ON dispatch_objectives(dispatch_entity_id, mandatory DESC, priority DESC);

CREATE TABLE IF NOT EXISTS dispatch_constraints (
    dispatch_entity_id TEXT NOT NULL,
    constraint_content_id TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(dispatch_entity_id, constraint_content_id),
    FOREIGN KEY(dispatch_entity_id)
        REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS war_fronts (
    front_entity_id TEXT PRIMARY KEY,
    war_entity_id TEXT NOT NULL,
    location_entity_id TEXT,
    reality_entity_id TEXT,
    state TEXT NOT NULL,
    start_world_tick INTEGER NOT NULL,
    end_world_tick INTEGER,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(front_entity_id) REFERENCES entities(id),
    FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id),
    FOREIGN KEY(reality_entity_id) REFERENCES reality_nodes(reality_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_war_fronts_parent
    ON war_fronts(war_entity_id, state, start_world_tick);

CREATE TABLE IF NOT EXISTS war_objectives (
    war_entity_id TEXT NOT NULL,
    front_entity_id TEXT NOT NULL DEFAULT '',
    objective_content_id TEXT NOT NULL,
    target_entity_id TEXT,
    priority INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(war_entity_id, front_entity_id, objective_content_id),
    FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(target_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_war_objectives_status
    ON war_objectives(war_entity_id, status, priority DESC);

CREATE TABLE IF NOT EXISTS war_orders (
    order_entity_id TEXT PRIMARY KEY,
    war_entity_id TEXT NOT NULL,
    front_entity_id TEXT,
    issuer_entity_id TEXT NOT NULL,
    recipient_entity_id TEXT NOT NULL,
    intent_content_id TEXT NOT NULL,
    constraints_json TEXT NOT NULL DEFAULT '{}',
    issued_world_tick INTEGER NOT NULL,
    outcome_state TEXT NOT NULL DEFAULT 'pending',
    outcome_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(order_entity_id) REFERENCES entities(id),
    FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(front_entity_id) REFERENCES war_fronts(front_entity_id),
    FOREIGN KEY(issuer_entity_id) REFERENCES entities(id),
    FOREIGN KEY(recipient_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_war_orders_parent
    ON war_orders(war_entity_id, issued_world_tick);

CREATE TABLE IF NOT EXISTS war_participant_history (
    war_entity_id TEXT NOT NULL,
    faction_entity_id TEXT NOT NULL,
    side_index INTEGER NOT NULL,
    joined_world_tick INTEGER NOT NULL,
    left_world_tick INTEGER,
    reason TEXT,
    PRIMARY KEY(war_entity_id, faction_entity_id, joined_world_tick),
    FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_war_participant_history_faction
    ON war_participant_history(faction_entity_id, joined_world_tick);

CREATE TABLE IF NOT EXISTS army_capabilities (
    army_entity_id TEXT NOT NULL,
    capability_content_id TEXT NOT NULL,
    magnitude_sig INTEGER NOT NULL,
    magnitude_exp INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(army_entity_id, capability_content_id),
    FOREIGN KEY(army_entity_id) REFERENCES armies(army_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS project_phases (
    project_entity_id TEXT NOT NULL,
    phase_content_id TEXT NOT NULL,
    sequence INTEGER NOT NULL,
    status TEXT NOT NULL,
    start_world_tick INTEGER NOT NULL,
    resolve_world_tick INTEGER NOT NULL,
    progress_bps INTEGER NOT NULL DEFAULT 0 CHECK(progress_bps BETWEEN 0 AND 10000),
    payload_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(project_entity_id, phase_content_id),
    FOREIGN KEY(project_entity_id) REFERENCES projects(project_entity_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_project_phases_sequence
    ON project_phases(project_entity_id, sequence);

CREATE TABLE IF NOT EXISTS project_assignments (
    project_entity_id TEXT NOT NULL,
    assignee_entity_id TEXT NOT NULL,
    role_content_id TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(project_entity_id, assignee_entity_id, role_content_id),
    FOREIGN KEY(project_entity_id) REFERENCES projects(project_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(assignee_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS civilization_state (
    civilization_entity_id TEXT PRIMARY KEY,
    genre_profile_content_id TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(civilization_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS civilization_dimensions (
    civilization_entity_id TEXT NOT NULL,
    dimension_content_id TEXT NOT NULL,
    grade TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(civilization_entity_id, dimension_content_id),
    FOREIGN KEY(civilization_entity_id)
        REFERENCES civilization_state(civilization_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS logistics_routes (
    route_entity_id TEXT PRIMARY KEY,
    owner_entity_id TEXT NOT NULL,
    origin_location_entity_id TEXT,
    origin_reality_entity_id TEXT,
    destination_location_entity_id TEXT,
    destination_reality_entity_id TEXT,
    transport_capability_content_id TEXT NOT NULL,
    capacity_sig INTEGER NOT NULL,
    capacity_exp INTEGER NOT NULL,
    risk_bps INTEGER NOT NULL DEFAULT 0 CHECK(risk_bps BETWEEN 0 AND 10000),
    status TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    CHECK(origin_location_entity_id IS NOT NULL OR origin_reality_entity_id IS NOT NULL),
    CHECK(destination_location_entity_id IS NOT NULL OR destination_reality_entity_id IS NOT NULL),
    FOREIGN KEY(route_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(origin_location_entity_id) REFERENCES locations(location_entity_id),
    FOREIGN KEY(origin_reality_entity_id) REFERENCES reality_nodes(reality_entity_id),
    FOREIGN KEY(destination_location_entity_id) REFERENCES locations(location_entity_id),
    FOREIGN KEY(destination_reality_entity_id) REFERENCES reality_nodes(reality_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_logistics_routes_owner
    ON logistics_routes(owner_entity_id, status);
