-- Migration 0011: reality graph, deterministic local time and bounded World Director.

CREATE TABLE IF NOT EXISTS time_domains (
    time_domain_entity_id TEXT PRIMARY KEY,
    parent_time_domain_entity_id TEXT,
    rate_numerator INTEGER NOT NULL DEFAULT 1 CHECK(rate_numerator > 0),
    rate_denominator INTEGER NOT NULL DEFAULT 1 CHECK(rate_denominator > 0),
    parent_epoch_tick INTEGER NOT NULL DEFAULT 0,
    local_epoch_tick INTEGER NOT NULL DEFAULT 0,
    calendar_content_id TEXT,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(time_domain_entity_id) REFERENCES entities(id),
    FOREIGN KEY(parent_time_domain_entity_id) REFERENCES time_domains(time_domain_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_time_domains_parent
    ON time_domains(parent_time_domain_entity_id);

CREATE TABLE IF NOT EXISTS reality_nodes (
    reality_entity_id TEXT PRIMARY KEY,
    parent_reality_entity_id TEXT,
    kind TEXT NOT NULL,
    world_rank_content_id TEXT,
    time_domain_entity_id TEXT,
    law_profile_content_id TEXT,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(reality_entity_id) REFERENCES entities(id),
    FOREIGN KEY(parent_reality_entity_id) REFERENCES reality_nodes(reality_entity_id),
    FOREIGN KEY(time_domain_entity_id) REFERENCES time_domains(time_domain_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_reality_nodes_parent
    ON reality_nodes(parent_reality_entity_id);
CREATE INDEX IF NOT EXISTS idx_reality_nodes_time
    ON reality_nodes(time_domain_entity_id);

CREATE TABLE IF NOT EXISTS junctions (
    junction_entity_id TEXT PRIMARY KEY,
    from_reality_entity_id TEXT NOT NULL,
    to_reality_entity_id TEXT NOT NULL,
    state TEXT NOT NULL,
    stability_bps INTEGER NOT NULL DEFAULT 10000 CHECK(stability_bps BETWEEN 0 AND 10000),
    opened_world_tick INTEGER,
    closed_world_tick INTEGER,
    requirements_json TEXT NOT NULL DEFAULT '{}',
    CHECK(from_reality_entity_id <> to_reality_entity_id),
    FOREIGN KEY(junction_entity_id) REFERENCES entities(id),
    FOREIGN KEY(from_reality_entity_id) REFERENCES reality_nodes(reality_entity_id),
    FOREIGN KEY(to_reality_entity_id) REFERENCES reality_nodes(reality_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_junctions_from
    ON junctions(from_reality_entity_id, state);
CREATE INDEX IF NOT EXISTS idx_junctions_to
    ON junctions(to_reality_entity_id, state);

CREATE TABLE IF NOT EXISTS world_director_schedule (
    schedule_entity_id TEXT PRIMARY KEY,
    content_id TEXT NOT NULL,
    template_content_id TEXT,
    status TEXT NOT NULL,
    eligible_since_world_tick INTEGER,
    scheduled_start_world_tick INTEGER,
    latest_start_world_tick INTEGER,
    resolution_seed INTEGER NOT NULL,
    decision_provenance_json TEXT NOT NULL DEFAULT '{}',
    CHECK(
        scheduled_start_world_tick IS NULL OR
        latest_start_world_tick IS NULL OR
        scheduled_start_world_tick <= latest_start_world_tick),
    FOREIGN KEY(schedule_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_world_director_schedule_status
    ON world_director_schedule(status, scheduled_start_world_tick);
CREATE INDEX IF NOT EXISTS idx_world_director_schedule_content
    ON world_director_schedule(content_id, scheduled_start_world_tick);

CREATE TABLE IF NOT EXISTS content_unlock_state (
    content_id TEXT PRIMARY KEY,
    state TEXT NOT NULL,
    eligible_world_tick INTEGER,
    released_world_tick INTEGER,
    state_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS offline_simulation_state (
    scope_entity_id TEXT PRIMARY KEY,
    last_active_world_tick INTEGER NOT NULL,
    last_catchup_world_tick INTEGER NOT NULL,
    governor_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(scope_entity_id) REFERENCES entities(id)
);

-- Existing worlds/locations are intentionally not assigned fabricated Reality
-- nodes, World Ranks, calendars or Director schedules during migration.
