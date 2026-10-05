-- Migration 0009: Domain-heart state, normalized Core Concepts,
-- asymmetric Core fusion history and persistent lineage.

CREATE TABLE IF NOT EXISTS territory_domain_state (
    territory_entity_id TEXT PRIMARY KEY,
    active_core_entity_id TEXT,
    domain_state TEXT NOT NULL DEFAULT 'none',
    heart_lost_world_tick INTEGER,
    ruin_started_world_tick INTEGER,
    reconstitution_project_entity_id TEXT,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(territory_entity_id)
        REFERENCES territories(territory_entity_id),
    FOREIGN KEY(active_core_entity_id)
        REFERENCES domain_cores(core_entity_id),
    FOREIGN KEY(reconstitution_project_entity_id)
        REFERENCES projects(project_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_territory_domain_active_core
    ON territory_domain_state(active_core_entity_id);

CREATE TABLE IF NOT EXISTS domain_core_concepts (
    core_entity_id TEXT NOT NULL,
    concept_content_id TEXT NOT NULL,
    grade INTEGER NOT NULL DEFAULT 0,
    origin_source_core_entity_id TEXT,
    synthesis_rule_content_id TEXT,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(core_entity_id, concept_content_id),
    FOREIGN KEY(core_entity_id)
        REFERENCES domain_cores(core_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(origin_source_core_entity_id)
        REFERENCES domain_cores(core_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_domain_core_concepts_origin
    ON domain_core_concepts(origin_source_core_entity_id);

CREATE TABLE IF NOT EXISTS domain_core_fusions (
    fusion_entity_id TEXT PRIMARY KEY,
    result_core_entity_id TEXT NOT NULL,
    absorber_core_entity_id TEXT NOT NULL,
    absorbed_core_entity_id TEXT NOT NULL,
    fusion_world_tick INTEGER NOT NULL,
    sequence_ordinal INTEGER NOT NULL CHECK (sequence_ordinal >= 0),
    synthesis_rule_content_id TEXT,
    outcome_kind TEXT NOT NULL,
    resolution_seed INTEGER NOT NULL DEFAULT 0,
    instability_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(fusion_entity_id) REFERENCES entities(id),
    FOREIGN KEY(result_core_entity_id) REFERENCES domain_cores(core_entity_id),
    FOREIGN KEY(absorber_core_entity_id) REFERENCES domain_cores(core_entity_id),
    FOREIGN KEY(absorbed_core_entity_id) REFERENCES domain_cores(core_entity_id),
    UNIQUE(result_core_entity_id, sequence_ordinal)
);

CREATE INDEX IF NOT EXISTS idx_domain_core_fusions_absorber
    ON domain_core_fusions(absorber_core_entity_id, fusion_world_tick);

CREATE INDEX IF NOT EXISTS idx_domain_core_fusions_absorbed
    ON domain_core_fusions(absorbed_core_entity_id, fusion_world_tick);

CREATE TABLE IF NOT EXISTS domain_core_lineage (
    result_core_entity_id TEXT NOT NULL,
    source_core_entity_id TEXT NOT NULL,
    fusion_entity_id TEXT NOT NULL,
    lineage_role TEXT NOT NULL,
    PRIMARY KEY(
        result_core_entity_id,
        source_core_entity_id,
        fusion_entity_id),
    FOREIGN KEY(result_core_entity_id)
        REFERENCES domain_cores(core_entity_id),
    FOREIGN KEY(source_core_entity_id)
        REFERENCES domain_cores(core_entity_id),
    FOREIGN KEY(fusion_entity_id)
        REFERENCES domain_core_fusions(fusion_entity_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_domain_core_lineage_source
    ON domain_core_lineage(source_core_entity_id);

-- Legacy domain_core_aspects -> domain_core_concepts and initial Domain-heart
-- projection are deterministic migration callbacks in FOGSQLiteWorldStore.
