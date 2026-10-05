-- Migration 0013: normalized items/inventory/equipment, relationship affinity and
-- proficiency, presentation state, knowledge/belief/language/memory, NPC
-- promotion, mutable adult runtime context and Heroic Records.

CREATE TABLE IF NOT EXISTS item_instances (
    item_entity_id TEXT PRIMARY KEY,
    definition_content_id TEXT NOT NULL,
    owner_entity_id TEXT,
    current_rank_content_id TEXT,
    quality_content_id TEXT,
    durability_state_json TEXT NOT NULL DEFAULT '{}',
    evolution_state_json TEXT NOT NULL DEFAULT '{}',
    history_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(item_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_item_instances_owner
    ON item_instances(owner_entity_id);

CREATE TABLE IF NOT EXISTS item_modifiers (
    item_entity_id TEXT NOT NULL,
    modifier_content_id TEXT NOT NULL,
    ordinal INTEGER NOT NULL DEFAULT 0 CHECK(ordinal >= 0),
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(item_entity_id, modifier_content_id, ordinal),
    FOREIGN KEY(item_entity_id)
        REFERENCES item_instances(item_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS equipment_bindings (
    wearer_entity_id TEXT NOT NULL,
    slot_content_id TEXT NOT NULL,
    item_entity_id TEXT NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(wearer_entity_id, slot_content_id),
    FOREIGN KEY(wearer_entity_id) REFERENCES entities(id),
    FOREIGN KEY(item_entity_id) REFERENCES item_instances(item_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_equipment_bindings_item
    ON equipment_bindings(item_entity_id);

CREATE TABLE IF NOT EXISTS inventory_containers (
    container_entity_id TEXT PRIMARY KEY,
    owner_entity_id TEXT NOT NULL,
    container_type_content_id TEXT NOT NULL,
    capacity_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(container_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_inventory_containers_owner
    ON inventory_containers(owner_entity_id);

CREATE TABLE IF NOT EXISTS container_contents (
    container_entity_id TEXT NOT NULL,
    item_entity_id TEXT NOT NULL,
    amount INTEGER NOT NULL DEFAULT 1 CHECK(amount > 0),
    PRIMARY KEY(container_entity_id, item_entity_id),
    FOREIGN KEY(container_entity_id)
        REFERENCES inventory_containers(container_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(item_entity_id)
        REFERENCES item_instances(item_entity_id)
);

CREATE TABLE IF NOT EXISTS item_owner_affinity (
    item_entity_id TEXT NOT NULL,
    owner_entity_id TEXT NOT NULL,
    affinity_value INTEGER NOT NULL DEFAULT 0,
    milestone_content_id TEXT,
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(item_entity_id, owner_entity_id),
    FOREIGN KEY(item_entity_id)
        REFERENCES item_instances(item_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS entity_equipment_proficiency (
    owner_entity_id TEXT NOT NULL,
    proficiency_content_id TEXT NOT NULL,
    proficiency_value INTEGER NOT NULL DEFAULT 0,
    grade_content_id TEXT,
    updated_world_tick INTEGER NOT NULL,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(owner_entity_id, proficiency_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS manifestation_presentation_state (
    manifestation_entity_id TEXT PRIMARY KEY,
    selected_skin_content_id TEXT,
    outfit_state_json TEXT NOT NULL DEFAULT '{}',
    presentation_variant_state_json TEXT NOT NULL DEFAULT '{}',
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS owned_presentation_unlocks (
    owner_entity_id TEXT NOT NULL,
    presentation_content_id TEXT NOT NULL,
    acquired_world_tick INTEGER NOT NULL,
    state TEXT NOT NULL DEFAULT 'owned',
    PRIMARY KEY(owner_entity_id, presentation_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id)
);

ALTER TABLE knowledge_facts
    ADD COLUMN belief_state TEXT NOT NULL DEFAULT 'believed';
ALTER TABLE knowledge_facts
    ADD COLUMN confidence_bps INTEGER NOT NULL DEFAULT 10000
    CHECK(confidence_bps BETWEEN 0 AND 10000);
ALTER TABLE knowledge_facts
    ADD COLUMN source_entity_id TEXT;
ALTER TABLE knowledge_facts
    ADD COLUMN source_event_id TEXT;
ALTER TABLE knowledge_facts
    ADD COLUMN evidence_world_tick INTEGER;
ALTER TABLE knowledge_facts
    ADD COLUMN language_context_content_id TEXT;

CREATE TABLE IF NOT EXISTS entity_languages (
    entity_id TEXT NOT NULL,
    language_content_id TEXT NOT NULL,
    spoken_proficiency_bps INTEGER NOT NULL DEFAULT 0
        CHECK(spoken_proficiency_bps BETWEEN 0 AND 10000),
    written_proficiency_bps INTEGER NOT NULL DEFAULT 0
        CHECK(written_proficiency_bps BETWEEN 0 AND 10000),
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(entity_id, language_content_id),
    FOREIGN KEY(entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS semantic_memories (
    memory_entity_id TEXT PRIMARY KEY,
    owner_entity_id TEXT NOT NULL,
    subject_entity_id TEXT,
    source_event_id TEXT,
    memory_type_content_id TEXT NOT NULL,
    salience_bps INTEGER NOT NULL DEFAULT 0
        CHECK(salience_bps BETWEEN 0 AND 10000),
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(memory_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(subject_entity_id) REFERENCES entities(id),
    FOREIGN KEY(source_event_id) REFERENCES world_events(event_id)
);

CREATE INDEX IF NOT EXISTS idx_semantic_memories_owner
    ON semantic_memories(owner_entity_id, salience_bps DESC);

CREATE TABLE IF NOT EXISTS npc_promotion_state (
    entity_id TEXT PRIMARY KEY,
    simulation_tier_content_id TEXT NOT NULL,
    promoted_world_tick INTEGER NOT NULL,
    reason_event_id TEXT,
    presentation_package_state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(entity_id) REFERENCES entities(id),
    FOREIGN KEY(reason_event_id) REFERENCES world_events(event_id)
);

CREATE TABLE IF NOT EXISTS character_adult_runtime_state (
    character_entity_id TEXT PRIMARY KEY,
    current_profile_variant_content_id TEXT,
    mutable_context_state_json TEXT NOT NULL DEFAULT '{}',
    updated_world_tick INTEGER NOT NULL,
    FOREIGN KEY(character_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS heroic_records (
    record_entity_id TEXT PRIMARY KEY,
    source_world_entity_id TEXT NOT NULL,
    identity_content_id TEXT NOT NULL,
    death_event_id TEXT NOT NULL,
    created_world_tick INTEGER NOT NULL,
    pattern_content_id TEXT,
    gacha_access_state TEXT NOT NULL DEFAULT 'locked',
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(record_entity_id) REFERENCES entities(id),
    FOREIGN KEY(source_world_entity_id) REFERENCES entities(id),
    FOREIGN KEY(death_event_id) REFERENCES world_events(event_id)
);

CREATE INDEX IF NOT EXISTS idx_heroic_records_identity
    ON heroic_records(identity_content_id, created_world_tick);
