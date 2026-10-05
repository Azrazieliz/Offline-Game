-- Migration 0014: package lifecycle/dependencies, reports/delivery and
-- canonical Manifestation management metadata. Device/profile preferences and
-- backup catalog remain sidecars outside canonical world causality.

ALTER TABLE content_packages
    ADD COLUMN category TEXT NOT NULL DEFAULT 'generic';
ALTER TABLE content_packages
    ADD COLUMN install_uri TEXT NOT NULL DEFAULT '';
ALTER TABLE content_packages
    ADD COLUMN storage_class TEXT NOT NULL DEFAULT 'local_hot';
ALTER TABLE content_packages
    ADD COLUMN sealed_state TEXT NOT NULL DEFAULT 'visible';
ALTER TABLE content_packages
    ADD COLUMN download_state TEXT NOT NULL DEFAULT 'installed';
ALTER TABLE content_packages
    ADD COLUMN compatibility_json TEXT NOT NULL DEFAULT '{}';

CREATE TABLE IF NOT EXISTS package_dependencies (
    package_id TEXT NOT NULL,
    dependency_package_id TEXT NOT NULL,
    minimum_version INTEGER NOT NULL CHECK(minimum_version > 0),
    PRIMARY KEY(package_id, dependency_package_id),
    CHECK(package_id <> dependency_package_id),
    FOREIGN KEY(package_id)
        REFERENCES content_packages(package_id) ON DELETE CASCADE,
    FOREIGN KEY(dependency_package_id)
        REFERENCES content_packages(package_id)
);

CREATE TABLE IF NOT EXISTS reports (
    report_entity_id TEXT PRIMARY KEY,
    owner_entity_id TEXT NOT NULL,
    source_world_event_id TEXT,
    category TEXT NOT NULL,
    priority INTEGER NOT NULL DEFAULT 0,
    created_world_tick INTEGER NOT NULL,
    acknowledged_world_tick INTEGER,
    payload_json TEXT NOT NULL DEFAULT '{}',
    CHECK(acknowledged_world_tick IS NULL OR
          acknowledged_world_tick >= created_world_tick),
    FOREIGN KEY(report_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(source_world_event_id) REFERENCES world_events(event_id)
);

CREATE INDEX IF NOT EXISTS idx_reports_owner
    ON reports(owner_entity_id, acknowledged_world_tick, priority DESC, created_world_tick DESC);

CREATE TABLE IF NOT EXISTS report_delivery (
    report_entity_id TEXT NOT NULL,
    channel TEXT NOT NULL,
    state TEXT NOT NULL,
    scheduled_real_utc TEXT,
    delivered_real_utc TEXT,
    platform_notification_id TEXT,
    privacy_state TEXT NOT NULL DEFAULT 'default',
    PRIMARY KEY(report_entity_id, channel),
    FOREIGN KEY(report_entity_id)
        REFERENCES reports(report_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS manifestation_management_metadata (
    manifestation_entity_id TEXT PRIMARY KEY,
    favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)),
    protected_state INTEGER NOT NULL DEFAULT 0 CHECK(protected_state IN (0,1)),
    locked_state INTEGER NOT NULL DEFAULT 0 CHECK(locked_state IN (0,1)),
    updated_world_tick INTEGER NOT NULL DEFAULT 0,
    state_json TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS manifestation_context_selection (
    owner_entity_id TEXT NOT NULL,
    context_content_id TEXT NOT NULL,
    manifestation_entity_id TEXT NOT NULL,
    updated_world_tick INTEGER NOT NULL DEFAULT 0,
    state_json TEXT NOT NULL DEFAULT '{}',
    PRIMARY KEY(owner_entity_id, context_content_id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(manifestation_entity_id)
        REFERENCES character_manifestations(manifestation_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_manifestation_context_selection_manifestation
    ON manifestation_context_selection(manifestation_entity_id);
