-- Migration 0010: queryable entity/Manifestation progression.
-- Rank identities, Class identities, route families, Transcendence grades and
-- World Fantasm grades remain stable content IDs rather than C++ enums.

CREATE TABLE IF NOT EXISTS entity_rank_state (
    entity_id TEXT PRIMARY KEY,
    attained_rank_content_id TEXT NOT NULL,
    attained_level INTEGER NOT NULL CHECK(attained_level BETWEEN 1 AND 100),
    effective_rank_content_id TEXT,
    effective_level INTEGER,
    peak_rank_content_id TEXT NOT NULL,
    peak_level INTEGER NOT NULL CHECK(peak_level BETWEEN 1 AND 100),
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    CHECK(effective_level IS NULL OR effective_level BETWEEN 1 AND 100),
    CHECK((effective_rank_content_id IS NULL) = (effective_level IS NULL)),
    FOREIGN KEY(entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS factor_instances (
    factor_instance_id TEXT PRIMARY KEY,
    owner_entity_id TEXT NOT NULL,
    factor_content_id TEXT NOT NULL,
    source_entity_id TEXT,
    acquired_world_tick INTEGER NOT NULL,
    purity_bps INTEGER NOT NULL DEFAULT 10000 CHECK(purity_bps BETWEEN 0 AND 10000),
    maturity_bps INTEGER NOT NULL DEFAULT 0 CHECK(maturity_bps BETWEEN 0 AND 10000),
    completeness_bps INTEGER NOT NULL DEFAULT 10000 CHECK(completeness_bps BETWEEN 0 AND 10000),
    expression_weight_bps INTEGER NOT NULL DEFAULT 0 CHECK(expression_weight_bps BETWEEN 0 AND 10000),
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(factor_instance_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(source_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_factor_instances_owner
    ON factor_instances(owner_entity_id, factor_content_id);

CREATE TABLE IF NOT EXISTS factor_lineage (
    child_factor_instance_id TEXT NOT NULL,
    parent_factor_instance_id TEXT NOT NULL,
    relation_kind TEXT NOT NULL,
    ordinal INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(child_factor_instance_id, parent_factor_instance_id),
    FOREIGN KEY(child_factor_instance_id)
        REFERENCES factor_instances(factor_instance_id) ON DELETE CASCADE,
    FOREIGN KEY(parent_factor_instance_id)
        REFERENCES factor_instances(factor_instance_id)
);

CREATE TABLE IF NOT EXISTS entity_classes (
    owner_entity_id TEXT NOT NULL,
    class_content_id TEXT NOT NULL,
    attained_tier TEXT NOT NULL,
    current_expression_state TEXT NOT NULL DEFAULT 'expressible',
    recognized_world_tick INTEGER NOT NULL,
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(owner_entity_id, class_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_entity_classes_class
    ON entity_classes(class_content_id, attained_tier);

CREATE TABLE IF NOT EXISTS grand_class_seats (
    class_content_id TEXT PRIMARY KEY,
    bearer_entity_id TEXT NOT NULL,
    appointed_world_tick INTEGER NOT NULL,
    seat_state TEXT NOT NULL DEFAULT 'active',
    mandate_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(bearer_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_grand_class_seats_bearer
    ON grand_class_seats(bearer_entity_id, seat_state);

CREATE TABLE IF NOT EXISTS entity_skills (
    owner_entity_id TEXT NOT NULL,
    skill_content_id TEXT NOT NULL,
    learned_world_tick INTEGER NOT NULL,
    current_state TEXT NOT NULL DEFAULT 'learned',
    development_state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(owner_entity_id, skill_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS skill_provenance (
    owner_entity_id TEXT NOT NULL,
    skill_content_id TEXT NOT NULL,
    source_kind TEXT NOT NULL,
    source_content_or_entity_id TEXT NOT NULL,
    source_world_tick INTEGER NOT NULL,
    PRIMARY KEY(
        owner_entity_id,
        skill_content_id,
        source_kind,
        source_content_or_entity_id),
    FOREIGN KEY(owner_entity_id, skill_content_id)
        REFERENCES entity_skills(owner_entity_id, skill_content_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS manifestation_route_nodes (
    manifestation_entity_id TEXT NOT NULL,
    route_content_id TEXT NOT NULL,
    node_content_id TEXT NOT NULL,
    state TEXT NOT NULL,
    entered_world_tick INTEGER,
    completed_world_tick INTEGER,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(manifestation_entity_id, route_content_id, node_content_id),
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_manifestation_route_nodes_state
    ON manifestation_route_nodes(manifestation_entity_id, state);

CREATE TABLE IF NOT EXISTS manifestation_forms (
    manifestation_entity_id TEXT NOT NULL,
    form_content_id TEXT NOT NULL,
    state TEXT NOT NULL,
    unlocked_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(manifestation_entity_id, form_content_id),
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS manifestation_reinforcement (
    manifestation_entity_id TEXT PRIMARY KEY,
    reinforcement_state TEXT NOT NULL,
    max_reinforced INTEGER NOT NULL DEFAULT 0 CHECK(max_reinforced IN (0,1)),
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS entity_transcendence_state (
    entity_id TEXT PRIMARY KEY,
    grade_content_id TEXT NOT NULL,
    breakthrough_world_tick INTEGER NOT NULL,
    qualification_snapshot_json TEXT NOT NULL DEFAULT '{}',
    proof_provenance_json TEXT NOT NULL DEFAULT '{}',
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS manifestation_world_fantasm_state (
    manifestation_entity_id TEXT PRIMARY KEY,
    grade_content_id TEXT NOT NULL,
    unlocked_world_tick INTEGER NOT NULL,
    expression_profile_content_id TEXT,
    evolution_state_json TEXT NOT NULL DEFAULT '{}',
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS protagonist_world_manifestation_state (
    owner_entity_id TEXT PRIMARY KEY,
    unlocked_world_tick INTEGER NOT NULL,
    expression_profile_content_id TEXT,
    provenance_state_json TEXT NOT NULL DEFAULT '{}',
    evolution_state_json TEXT NOT NULL DEFAULT '{}',
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS character_convergences (
    convergence_entity_id TEXT PRIMARY KEY,
    identity_content_id TEXT NOT NULL,
    result_manifestation_entity_id TEXT NOT NULL,
    rule_content_id TEXT NOT NULL,
    world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(convergence_entity_id) REFERENCES entities(id),
    FOREIGN KEY(result_manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_character_convergences_identity
    ON character_convergences(identity_content_id, world_tick);

CREATE TABLE IF NOT EXISTS character_convergence_sources (
    convergence_entity_id TEXT NOT NULL,
    source_manifestation_entity_id TEXT NOT NULL,
    lineage_content_id TEXT,
    ordinal INTEGER NOT NULL,
    PRIMARY KEY(convergence_entity_id, source_manifestation_entity_id),
    FOREIGN KEY(convergence_entity_id)
        REFERENCES character_convergences(convergence_entity_id)
        ON DELETE CASCADE,
    FOREIGN KEY(source_manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
);

-- Legacy ProgressionStateJson remains intact for migration provenance only.
-- No Rank identity, Factor, Class or route is fabricated from opaque legacy JSON.
-- Existing Manifestations receive only an explicit "legacy_unassessed"
-- reinforcement row so max-reinforcement is never inferred.
