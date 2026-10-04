-- Migration 0007: full persistent Manifestation instances and acquisition provenance.
-- The legacy duplicate_acquisition_count column is retained only as migration provenance.
-- Runtime code no longer reads/writes it after this migration.

ALTER TABLE character_manifestations
    ADD COLUMN acquisition_world_tick INTEGER NOT NULL DEFAULT 0;

ALTER TABLE character_manifestations
    ADD COLUMN acquisition_ordinal INTEGER NOT NULL DEFAULT 0
    CHECK (acquisition_ordinal >= 0);

ALTER TABLE character_manifestations
    ADD COLUMN origin_pull_event_id TEXT;

ALTER TABLE character_manifestations
    ADD COLUMN world_mode_anchor_territory_id TEXT;

ALTER TABLE character_manifestations
    ADD COLUMN world_mode_anchor_tick INTEGER;

ALTER TABLE character_manifestations
    ADD COLUMN lifecycle_state TEXT NOT NULL DEFAULT 'active';

ALTER TABLE character_manifestations
    ADD COLUMN build_label TEXT NOT NULL DEFAULT '';

CREATE INDEX IF NOT EXISTS idx_manifestations_owner_identity
    ON character_manifestations(owning_ruler_entity_id, identity_content_id);

CREATE INDEX IF NOT EXISTS idx_manifestations_owner_identity_ordinal
    ON character_manifestations(
        owning_ruler_entity_id,
        identity_content_id,
        acquisition_ordinal);

CREATE INDEX IF NOT EXISTS idx_manifestations_anchor
    ON character_manifestations(world_mode_anchor_territory_id);

-- Deterministic legacy counter fan-out and post-transform validation are
-- implemented by FOGSQLiteWorldStore migration callbacks.
