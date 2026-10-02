-- Result-oriented dispatch and minimal faction/war state.
-- No task/day simulation, faction feelings, treaty-law, internal politics or reputation.

CREATE TABLE IF NOT EXISTS dispatches (
    dispatch_entity_id      TEXT PRIMARY KEY,
    owner_entity_id         TEXT NOT NULL,
    target_entity_id        TEXT,
    dispatch_type           INTEGER NOT NULL,
    status                  INTEGER NOT NULL,
    start_world_tick        INTEGER NOT NULL,
    resolve_world_tick      INTEGER NOT NULL,
    risk_bps                INTEGER NOT NULL DEFAULT 0 CHECK (risk_bps BETWEEN 0 AND 10000),
    resolution_seed         INTEGER NOT NULL DEFAULT 0,
    result_json             TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(dispatch_entity_id) REFERENCES entities(id),
    FOREIGN KEY(owner_entity_id) REFERENCES entities(id),
    FOREIGN KEY(target_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS dispatch_participants (
    dispatch_entity_id      TEXT NOT NULL,
    participant_entity_id   TEXT NOT NULL,
    ordinal                 INTEGER NOT NULL,
    PRIMARY KEY(dispatch_entity_id, participant_entity_id),
    FOREIGN KEY(dispatch_entity_id) REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(participant_entity_id) REFERENCES entities(id)
);

CREATE INDEX IF NOT EXISTS idx_dispatch_owner_status
    ON dispatches(owner_entity_id, status);

CREATE TABLE IF NOT EXISTS factions (
    faction_entity_id       TEXT PRIMARY KEY,
    leader_ruler_entity_id  TEXT,
    kind                    TEXT NOT NULL,
    population              INTEGER NOT NULL DEFAULT 0 CHECK (population >= 0),
    FOREIGN KEY(faction_entity_id) REFERENCES entities(id),
    FOREIGN KEY(leader_ruler_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS faction_links (
    source_faction_entity_id  TEXT NOT NULL,
    target_faction_entity_id  TEXT NOT NULL,
    link_type                 INTEGER NOT NULL,
    active                    INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    updated_world_tick        INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(source_faction_entity_id, target_faction_entity_id, link_type),
    FOREIGN KEY(source_faction_entity_id) REFERENCES factions(faction_entity_id),
    FOREIGN KEY(target_faction_entity_id) REFERENCES factions(faction_entity_id)
);

CREATE TABLE IF NOT EXISTS armies (
    army_entity_id       TEXT PRIMARY KEY,
    faction_entity_id    TEXT NOT NULL,
    location_entity_id   TEXT NOT NULL,
    headcount            INTEGER NOT NULL DEFAULT 0 CHECK (headcount >= 0),
    power_sig            INTEGER NOT NULL DEFAULT 0,
    power_exp            INTEGER NOT NULL DEFAULT 0,
    state                TEXT NOT NULL DEFAULT 'ready',
    FOREIGN KEY(army_entity_id) REFERENCES entities(id),
    FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id),
    FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id)
);

CREATE TABLE IF NOT EXISTS army_commanders (
    army_entity_id       TEXT NOT NULL,
    commander_entity_id  TEXT NOT NULL,
    ordinal              INTEGER NOT NULL,
    PRIMARY KEY(army_entity_id, commander_entity_id),
    FOREIGN KEY(army_entity_id) REFERENCES armies(army_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(commander_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS wars (
    war_entity_id              TEXT PRIMARY KEY,
    status                     INTEGER NOT NULL,
    objective_type             TEXT NOT NULL,
    objective_target_entity_id TEXT,
    start_world_tick           INTEGER NOT NULL,
    end_world_tick             INTEGER NOT NULL DEFAULT 0,
    resolution_json            TEXT NOT NULL DEFAULT '{}',
    FOREIGN KEY(war_entity_id) REFERENCES entities(id),
    FOREIGN KEY(objective_target_entity_id) REFERENCES entities(id)
);

CREATE TABLE IF NOT EXISTS war_participants (
    war_entity_id       TEXT NOT NULL,
    faction_entity_id   TEXT NOT NULL,
    side_index          INTEGER NOT NULL,
    primary_participant INTEGER NOT NULL DEFAULT 1 CHECK (primary_participant IN (0,1)),
    PRIMARY KEY(war_entity_id, faction_entity_id),
    FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,
    FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id)
);

CREATE INDEX IF NOT EXISTS idx_wars_status
    ON wars(status);
