#include "Persistence/OGSQLiteWorldStore.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OfflineGame.h"
#include "sqlite/sqlite3.h"

namespace
{
struct FOGMigrationDefinition
{
    int32 Version;
    const TCHAR* Name;
    const TCHAR* Sql;
    TFunction<bool(FOGSQLiteWorldStore&, FString&)> DataTransform;
    TFunction<bool(FOGSQLiteWorldStore&, FString&)> Validate;
};

static const TCHAR* Migration0001Sql =
    TEXT("CREATE TABLE IF NOT EXISTS entities (")
    TEXT("id TEXT PRIMARY KEY,")
    TEXT("kind TEXT NOT NULL,")
    TEXT("created_world_tick INTEGER NOT NULL DEFAULT 0,")
    TEXT("retired_world_tick INTEGER,")
    TEXT("revision INTEGER NOT NULL DEFAULT 0,")
    TEXT("state_json TEXT NOT NULL DEFAULT '{}');")
    TEXT("CREATE INDEX IF NOT EXISTS idx_entities_kind ON entities(kind);")
    TEXT("CREATE TABLE IF NOT EXISTS world_events (")
    TEXT("event_id TEXT PRIMARY KEY,")
    TEXT("event_type TEXT NOT NULL,")
    TEXT("world_tick INTEGER NOT NULL,")
    TEXT("primary_entity_id TEXT,")
    TEXT("related_entities_json TEXT NOT NULL DEFAULT '[]',")
    TEXT("payload_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("chronicle_eligible INTEGER NOT NULL DEFAULT 0 CHECK (chronicle_eligible IN (0,1)),")
    TEXT("FOREIGN KEY(primary_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_world_events_tick ON world_events(world_tick);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_world_events_primary ON world_events(primary_entity_id, world_tick);")
    TEXT("CREATE TABLE IF NOT EXISTS knowledge_facts (")
    TEXT("owner_entity_id TEXT NOT NULL,")
    TEXT("fact_key TEXT NOT NULL,")
    TEXT("subject_entity_id TEXT NOT NULL DEFAULT '',")
    TEXT("value_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("learned_world_tick INTEGER NOT NULL,")
    TEXT("updated_world_tick INTEGER NOT NULL,")
    TEXT("PRIMARY KEY(owner_entity_id, fact_key, subject_entity_id),")
    TEXT("FOREIGN KEY(owner_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS content_packages (")
    TEXT("package_id TEXT PRIMARY KEY,")
    TEXT("version INTEGER NOT NULL,")
    TEXT("content_hash TEXT NOT NULL,")
    TEXT("installed INTEGER NOT NULL DEFAULT 0 CHECK (installed IN (0,1)),")
    TEXT("validated INTEGER NOT NULL DEFAULT 0 CHECK (validated IN (0,1)),")
    TEXT("activated INTEGER NOT NULL DEFAULT 0 CHECK (activated IN (0,1)),")
    TEXT("manifest_json TEXT NOT NULL DEFAULT '{}');");

static const TCHAR* Migration0002Sql =
    TEXT("CREATE TABLE IF NOT EXISTS character_manifestations (")
    TEXT("manifestation_entity_id TEXT PRIMARY KEY,")
    TEXT("owning_ruler_entity_id TEXT NOT NULL,")
    TEXT("identity_content_id TEXT NOT NULL,")
    TEXT("active_version_content_id TEXT NOT NULL,")
    TEXT("level INTEGER NOT NULL DEFAULT 1 CHECK (level >= 1),")
    TEXT("current_rarity TEXT NOT NULL DEFAULT '',")
    TEXT("progression_state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(manifestation_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(owning_ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_character_manifestations_owner ")
    TEXT("ON character_manifestations(owning_ruler_entity_id);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_character_manifestations_identity ")
    TEXT("ON character_manifestations(identity_content_id);");

static const TCHAR* Migration0003Sql =
    TEXT("CREATE TABLE IF NOT EXISTS locations (")
    TEXT("location_entity_id TEXT PRIMARY KEY,")
    TEXT("parent_location_entity_id TEXT,")
    TEXT("kind TEXT NOT NULL,")
    TEXT("territory_entity_id TEXT,")
    TEXT("physically_accessible INTEGER NOT NULL DEFAULT 1 CHECK (physically_accessible IN (0,1)),")
    TEXT("FOREIGN KEY(location_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(parent_location_entity_id) REFERENCES locations(location_entity_id),")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_locations_parent ON locations(parent_location_entity_id);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_locations_territory ON locations(territory_entity_id);")
    TEXT("CREATE TABLE IF NOT EXISTS world_presence (")
    TEXT("entity_id TEXT PRIMARY KEY,")
    TEXT("location_entity_id TEXT NOT NULL,")
    TEXT("local_x REAL NOT NULL DEFAULT 0,")
    TEXT("local_y REAL NOT NULL DEFAULT 0,")
    TEXT("local_z REAL NOT NULL DEFAULT 0,")
    TEXT("movement_context TEXT NOT NULL DEFAULT '',")
    TEXT("updated_world_tick INTEGER NOT NULL DEFAULT 0,")
    TEXT("FOREIGN KEY(entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_world_presence_location ON world_presence(location_entity_id);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_knowledge_subject ON knowledge_facts(subject_entity_id, fact_key);");

static const TCHAR* Migration0004Sql =
    TEXT("CREATE TABLE IF NOT EXISTS territories (")
    TEXT("territory_entity_id TEXT PRIMARY KEY,")
    TEXT("ruler_entity_id TEXT,")
    TEXT("root_location_entity_id TEXT NOT NULL,")
    TEXT("is_main INTEGER NOT NULL DEFAULT 0 CHECK (is_main IN (0,1)),")
    TEXT("population INTEGER NOT NULL DEFAULT 0 CHECK (population >= 0),")
    TEXT("control_state TEXT NOT NULL DEFAULT 'controlled',")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(ruler_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(root_location_entity_id) REFERENCES locations(location_entity_id));")
    TEXT("CREATE UNIQUE INDEX IF NOT EXISTS idx_main_territory_per_ruler ")
    TEXT("ON territories(ruler_entity_id) WHERE is_main = 1 AND ruler_entity_id IS NOT NULL;")
    TEXT("CREATE INDEX IF NOT EXISTS idx_territories_location ON territories(root_location_entity_id);")
    TEXT("CREATE TABLE IF NOT EXISTS domain_cores (")
    TEXT("core_entity_id TEXT PRIMARY KEY,")
    TEXT("territory_entity_id TEXT NOT NULL UNIQUE,")
    TEXT("controller_ruler_entity_id TEXT,")
    TEXT("lifecycle INTEGER NOT NULL,")
    TEXT("durability_sig INTEGER NOT NULL DEFAULT 0,")
    TEXT("durability_exp INTEGER NOT NULL DEFAULT 0,")
    TEXT("max_durability_sig INTEGER NOT NULL DEFAULT 0,")
    TEXT("max_durability_exp INTEGER NOT NULL DEFAULT 0,")
    TEXT("FOREIGN KEY(core_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id),")
    TEXT("FOREIGN KEY(controller_ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS domain_core_aspects (")
    TEXT("core_entity_id TEXT NOT NULL,")
    TEXT("aspect_content_id TEXT NOT NULL,")
    TEXT("grade INTEGER NOT NULL DEFAULT 0,")
    TEXT("PRIMARY KEY(core_entity_id, aspect_content_id),")
    TEXT("FOREIGN KEY(core_entity_id) REFERENCES domain_cores(core_entity_id) ON DELETE CASCADE);")
    TEXT("CREATE TABLE IF NOT EXISTS resource_balances (")
    TEXT("owner_entity_id TEXT NOT NULL,")
    TEXT("resource_content_id TEXT NOT NULL,")
    TEXT("amount INTEGER NOT NULL DEFAULT 0 CHECK (amount >= 0),")
    TEXT("PRIMARY KEY(owner_entity_id, resource_content_id),")
    TEXT("FOREIGN KEY(owner_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS projects (")
    TEXT("project_entity_id TEXT PRIMARY KEY,")
    TEXT("owner_entity_id TEXT NOT NULL,")
    TEXT("location_entity_id TEXT NOT NULL,")
    TEXT("project_type_content_id TEXT NOT NULL,")
    TEXT("status INTEGER NOT NULL,")
    TEXT("start_world_tick INTEGER NOT NULL,")
    TEXT("resolve_world_tick INTEGER NOT NULL,")
    TEXT("progress_bps INTEGER NOT NULL DEFAULT 0 CHECK (progress_bps BETWEEN 0 AND 10000),")
    TEXT("payload_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(project_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(owner_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_projects_owner_status ON projects(owner_entity_id, status);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_projects_location ON projects(location_entity_id);");

static const TCHAR* Migration0005Sql =
    TEXT("CREATE TABLE IF NOT EXISTS dispatches (")
    TEXT("dispatch_entity_id TEXT PRIMARY KEY,")
    TEXT("owner_entity_id TEXT NOT NULL,")
    TEXT("target_entity_id TEXT,")
    TEXT("dispatch_type INTEGER NOT NULL,")
    TEXT("status INTEGER NOT NULL,")
    TEXT("start_world_tick INTEGER NOT NULL,")
    TEXT("resolve_world_tick INTEGER NOT NULL,")
    TEXT("risk_bps INTEGER NOT NULL DEFAULT 0 CHECK (risk_bps BETWEEN 0 AND 10000),")
    TEXT("resolution_seed INTEGER NOT NULL DEFAULT 0,")
    TEXT("result_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(dispatch_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(owner_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(target_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS dispatch_participants (")
    TEXT("dispatch_entity_id TEXT NOT NULL,")
    TEXT("participant_entity_id TEXT NOT NULL,")
    TEXT("ordinal INTEGER NOT NULL,")
    TEXT("PRIMARY KEY(dispatch_entity_id, participant_entity_id),")
    TEXT("FOREIGN KEY(dispatch_entity_id) REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE,")
    TEXT("FOREIGN KEY(participant_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_dispatch_owner_status ON dispatches(owner_entity_id, status);")
    TEXT("CREATE TABLE IF NOT EXISTS factions (")
    TEXT("faction_entity_id TEXT PRIMARY KEY,")
    TEXT("leader_ruler_entity_id TEXT,")
    TEXT("kind TEXT NOT NULL,")
    TEXT("population INTEGER NOT NULL DEFAULT 0 CHECK (population >= 0),")
    TEXT("FOREIGN KEY(faction_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(leader_ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS faction_links (")
    TEXT("source_faction_entity_id TEXT NOT NULL,")
    TEXT("target_faction_entity_id TEXT NOT NULL,")
    TEXT("link_type INTEGER NOT NULL,")
    TEXT("active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),")
    TEXT("updated_world_tick INTEGER NOT NULL DEFAULT 0,")
    TEXT("PRIMARY KEY(source_faction_entity_id, target_faction_entity_id, link_type),")
    TEXT("FOREIGN KEY(source_faction_entity_id) REFERENCES factions(faction_entity_id),")
    TEXT("FOREIGN KEY(target_faction_entity_id) REFERENCES factions(faction_entity_id));")
    TEXT("CREATE TABLE IF NOT EXISTS armies (")
    TEXT("army_entity_id TEXT PRIMARY KEY,")
    TEXT("faction_entity_id TEXT NOT NULL,")
    TEXT("location_entity_id TEXT NOT NULL,")
    TEXT("headcount INTEGER NOT NULL DEFAULT 0 CHECK (headcount >= 0),")
    TEXT("power_sig INTEGER NOT NULL DEFAULT 0,")
    TEXT("power_exp INTEGER NOT NULL DEFAULT 0,")
    TEXT("state TEXT NOT NULL DEFAULT 'ready',")
    TEXT("FOREIGN KEY(army_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id),")
    TEXT("FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id));")
    TEXT("CREATE TABLE IF NOT EXISTS army_commanders (")
    TEXT("army_entity_id TEXT NOT NULL,")
    TEXT("commander_entity_id TEXT NOT NULL,")
    TEXT("ordinal INTEGER NOT NULL,")
    TEXT("PRIMARY KEY(army_entity_id, commander_entity_id),")
    TEXT("FOREIGN KEY(army_entity_id) REFERENCES armies(army_entity_id) ON DELETE CASCADE,")
    TEXT("FOREIGN KEY(commander_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS wars (")
    TEXT("war_entity_id TEXT PRIMARY KEY,")
    TEXT("status INTEGER NOT NULL,")
    TEXT("objective_type TEXT NOT NULL,")
    TEXT("objective_target_entity_id TEXT,")
    TEXT("start_world_tick INTEGER NOT NULL,")
    TEXT("end_world_tick INTEGER NOT NULL DEFAULT 0,")
    TEXT("resolution_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(war_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(objective_target_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS war_participants (")
    TEXT("war_entity_id TEXT NOT NULL,")
    TEXT("faction_entity_id TEXT NOT NULL,")
    TEXT("side_index INTEGER NOT NULL,")
    TEXT("primary_participant INTEGER NOT NULL DEFAULT 1 CHECK (primary_participant IN (0,1)),")
    TEXT("PRIMARY KEY(war_entity_id, faction_entity_id),")
    TEXT("FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE,")
    TEXT("FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_wars_status ON wars(status);");

static const TCHAR* Migration0006Sql =
    TEXT("ALTER TABLE character_manifestations ")
    TEXT("ADD COLUMN duplicate_acquisition_count INTEGER NOT NULL DEFAULT 0 ")
    TEXT("CHECK (duplicate_acquisition_count >= 0);")
    TEXT("CREATE TABLE IF NOT EXISTS gacha_states (")
    TEXT("ruler_entity_id TEXT NOT NULL,")
    TEXT("pity_category TEXT NOT NULL,")
    TEXT("pulls_since_top_rarity INTEGER NOT NULL DEFAULT 0 CHECK (pulls_since_top_rarity >= 0),")
    TEXT("featured_guaranteed INTEGER NOT NULL DEFAULT 0 CHECK (featured_guaranteed IN (0,1)),")
    TEXT("total_pulls INTEGER NOT NULL DEFAULT 0 CHECK (total_pulls >= 0),")
    TEXT("updated_world_tick INTEGER NOT NULL DEFAULT 0,")
    TEXT("PRIMARY KEY(ruler_entity_id, pity_category),")
    TEXT("FOREIGN KEY(ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_gacha_states_ruler ON gacha_states(ruler_entity_id);");

static const TCHAR* Migration0007Sql =
    TEXT("ALTER TABLE character_manifestations ADD COLUMN acquisition_world_tick INTEGER NOT NULL DEFAULT 0;")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN acquisition_ordinal INTEGER NOT NULL DEFAULT 0 CHECK (acquisition_ordinal >= 0);")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN origin_pull_event_id TEXT;")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN world_mode_anchor_territory_id TEXT;")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN world_mode_anchor_tick INTEGER;")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN lifecycle_state TEXT NOT NULL DEFAULT 'active';")
    TEXT("ALTER TABLE character_manifestations ADD COLUMN build_label TEXT NOT NULL DEFAULT '';")
    TEXT("CREATE INDEX IF NOT EXISTS idx_manifestations_owner_identity ")
    TEXT("ON character_manifestations(owning_ruler_entity_id, identity_content_id);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_manifestations_owner_identity_ordinal ")
    TEXT("ON character_manifestations(owning_ruler_entity_id, identity_content_id, acquisition_ordinal);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_manifestations_anchor ")
    TEXT("ON character_manifestations(world_mode_anchor_territory_id);");

static const TCHAR* Migration0008Sql =
    TEXT("CREATE TABLE IF NOT EXISTS location_territories (")
    TEXT("location_entity_id TEXT NOT NULL,")
    TEXT("territory_entity_id TEXT NOT NULL,")
    TEXT("relation_kind TEXT NOT NULL DEFAULT 'contained',")
    TEXT("coverage_bps INTEGER NOT NULL DEFAULT 10000 CHECK (coverage_bps BETWEEN 0 AND 10000),")
    TEXT("PRIMARY KEY(location_entity_id, territory_entity_id),")
    TEXT("FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id),")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_location_territories_territory ")
    TEXT("ON location_territories(territory_entity_id, location_entity_id);")
    TEXT("CREATE TABLE IF NOT EXISTS territory_claims (")
    TEXT("claim_entity_id TEXT PRIMARY KEY,")
    TEXT("territory_entity_id TEXT NOT NULL,")
    TEXT("ruler_entity_id TEXT NOT NULL,")
    TEXT("claim_kind TEXT NOT NULL,")
    TEXT("control_state TEXT NOT NULL,")
    TEXT("control_strength_bps INTEGER NOT NULL DEFAULT 0 CHECK (control_strength_bps BETWEEN 0 AND 10000),")
    TEXT("claim_start_world_tick INTEGER NOT NULL,")
    TEXT("effective_control_start_world_tick INTEGER,")
    TEXT("displaced_world_tick INTEGER,")
    TEXT("reclaim_deadline_world_tick INTEGER,")
    TEXT("updated_world_tick INTEGER NOT NULL,")
    TEXT("state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("UNIQUE(territory_entity_id, ruler_entity_id),")
    TEXT("FOREIGN KEY(claim_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id),")
    TEXT("FOREIGN KEY(ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_territory_claims_territory ")
    TEXT("ON territory_claims(territory_entity_id, control_state, updated_world_tick);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_territory_claims_ruler ")
    TEXT("ON territory_claims(ruler_entity_id, control_state, updated_world_tick);")
    TEXT("CREATE TABLE IF NOT EXISTS ruler_sovereignty_state (")
    TEXT("ruler_entity_id TEXT PRIMARY KEY,")
    TEXT("current_title TEXT NOT NULL DEFAULT 'ruler',")
    TEXT("historical_peak_title TEXT NOT NULL DEFAULT 'ruler',")
    TEXT("continuous_control_start_world_tick INTEGER,")
    TEXT("last_effective_control_world_tick INTEGER,")
    TEXT("scope_state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("updated_world_tick INTEGER NOT NULL,")
    TEXT("FOREIGN KEY(ruler_entity_id) REFERENCES entities(id));")
    TEXT("CREATE TABLE IF NOT EXISTS ruler_gacha_access (")
    TEXT("ruler_entity_id TEXT PRIMARY KEY,")
    TEXT("qualification_start_world_tick INTEGER,")
    TEXT("qualification_suspended_world_tick INTEGER,")
    TEXT("unlocked_world_tick INTEGER,")
    TEXT("permanently_unlocked INTEGER NOT NULL DEFAULT 0 CHECK (permanently_unlocked IN (0,1)),")
    TEXT("updated_world_tick INTEGER NOT NULL,")
    TEXT("FOREIGN KEY(ruler_entity_id) REFERENCES entities(id));");

static const TCHAR* Migration0009Sql =
    TEXT("CREATE TABLE IF NOT EXISTS territory_domain_state (")
    TEXT("territory_entity_id TEXT PRIMARY KEY,")
    TEXT("active_core_entity_id TEXT,")
    TEXT("domain_state TEXT NOT NULL DEFAULT 'none',")
    TEXT("heart_lost_world_tick INTEGER,")
    TEXT("ruin_started_world_tick INTEGER,")
    TEXT("reconstitution_project_entity_id TEXT,")
    TEXT("state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(territory_entity_id) REFERENCES territories(territory_entity_id),")
    TEXT("FOREIGN KEY(active_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("FOREIGN KEY(reconstitution_project_entity_id) REFERENCES projects(project_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_territory_domain_active_core ")
    TEXT("ON territory_domain_state(active_core_entity_id);")
    TEXT("CREATE TABLE IF NOT EXISTS domain_core_concepts (")
    TEXT("core_entity_id TEXT NOT NULL,")
    TEXT("concept_content_id TEXT NOT NULL,")
    TEXT("grade INTEGER NOT NULL DEFAULT 0,")
    TEXT("origin_source_core_entity_id TEXT,")
    TEXT("synthesis_rule_content_id TEXT,")
    TEXT("state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("PRIMARY KEY(core_entity_id, concept_content_id),")
    TEXT("FOREIGN KEY(core_entity_id) REFERENCES domain_cores(core_entity_id) ON DELETE CASCADE,")
    TEXT("FOREIGN KEY(origin_source_core_entity_id) REFERENCES domain_cores(core_entity_id));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_domain_core_concepts_origin ")
    TEXT("ON domain_core_concepts(origin_source_core_entity_id);")
    TEXT("CREATE TABLE IF NOT EXISTS domain_core_fusions (")
    TEXT("fusion_entity_id TEXT PRIMARY KEY,")
    TEXT("result_core_entity_id TEXT NOT NULL,")
    TEXT("absorber_core_entity_id TEXT NOT NULL,")
    TEXT("absorbed_core_entity_id TEXT NOT NULL,")
    TEXT("fusion_world_tick INTEGER NOT NULL,")
    TEXT("sequence_ordinal INTEGER NOT NULL CHECK (sequence_ordinal >= 0),")
    TEXT("synthesis_rule_content_id TEXT,")
    TEXT("outcome_kind TEXT NOT NULL,")
    TEXT("resolution_seed INTEGER NOT NULL DEFAULT 0,")
    TEXT("instability_state_json TEXT NOT NULL DEFAULT '{}',")
    TEXT("FOREIGN KEY(fusion_entity_id) REFERENCES entities(id),")
    TEXT("FOREIGN KEY(result_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("FOREIGN KEY(absorber_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("FOREIGN KEY(absorbed_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("UNIQUE(result_core_entity_id, sequence_ordinal));")
    TEXT("CREATE INDEX IF NOT EXISTS idx_domain_core_fusions_absorber ")
    TEXT("ON domain_core_fusions(absorber_core_entity_id, fusion_world_tick);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_domain_core_fusions_absorbed ")
    TEXT("ON domain_core_fusions(absorbed_core_entity_id, fusion_world_tick);")
    TEXT("CREATE TABLE IF NOT EXISTS domain_core_lineage (")
    TEXT("result_core_entity_id TEXT NOT NULL,")
    TEXT("source_core_entity_id TEXT NOT NULL,")
    TEXT("fusion_entity_id TEXT NOT NULL,")
    TEXT("lineage_role TEXT NOT NULL,")
    TEXT("PRIMARY KEY(result_core_entity_id, source_core_entity_id, fusion_entity_id),")
    TEXT("FOREIGN KEY(result_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("FOREIGN KEY(source_core_entity_id) REFERENCES domain_cores(core_entity_id),")
    TEXT("FOREIGN KEY(fusion_entity_id) REFERENCES domain_core_fusions(fusion_entity_id) ON DELETE CASCADE);")
    TEXT("CREATE INDEX IF NOT EXISTS idx_domain_core_lineage_source ")
    TEXT("ON domain_core_lineage(source_core_entity_id);");

static const TCHAR* Migration0010Sql =
    TEXT("CREATE TABLE IF NOT EXISTS entity_rank_state ( entity_id TEXT PRIMARY KEY, attained_rank_content_id TEXT NOT NULL, attained_level INTEGER NOT NULL CHECK(attained_level BETWEEN 1 AND 100), effective_rank_content_id TEXT, effective_level INTEGER, peak_rank_content_id TEXT NOT NULL, peak_level INTEGER NOT NULL CHECK(peak_level BETWEEN 1 AND 100), updated_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', CHECK(effective_level IS NULL OR effective_level BETWEEN 1 AND 100), CHECK((effective_rank_content_id IS NULL) = (effective_level IS NULL)), FOREIGN KEY(entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS factor_instances ( factor_instance_id TEXT PRIMARY KEY, owner_entity_id TEXT NOT NULL, factor_content_id TEXT NOT NULL, source_entity_id TEXT, acquired_world_tick INTEGER NOT NULL, purity_bps INTEGER NOT NULL DEFAULT 10000 CHECK(purity_bps BETWEEN 0 AND 10000), maturity_bps INTEGER NOT NULL DEFAULT 0 CHECK(maturity_bps BETWEEN 0 AND 10000), completeness_bps INTEGER NOT NULL DEFAULT 10000 CHECK(completeness_bps BETWEEN 0 AND 10000), expression_weight_bps INTEGER NOT NULL DEFAULT 0 CHECK(expression_weight_bps BETWEEN 0 AND 10000), state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(factor_instance_id) REFERENCES entities(id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id), FOREIGN KEY(source_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_factor_instances_owner ON factor_instances(owner_entity_id, factor_content_id); CREATE TABLE IF NOT EXISTS factor_lineage ( child_factor_instance_id TEXT NOT NULL, parent_factor_instance_id TEXT NOT NULL, relation_kind TEXT NOT NULL, ordinal INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(child_factor_instance_id, parent_factor_instance_id), FOREIGN KEY(child_factor_instance_id) REFERENCES factor_instances(factor_instance_id) ON DELETE CASCADE, FOREIGN KEY(parent_factor_instance_id) REFERENCES factor_instances(factor_instance_id) ); CREATE TABLE IF NOT EXISTS entity_classes ( owner_entity_id TEXT NOT NULL, class_content_id TEXT NOT NULL, attained_tier TEXT NOT NULL, current_expression_state TEXT NOT NULL DEFAULT 'expressible', recognized_world_tick INTEGER NOT NULL, updated_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(owner_entity_id, class_content_id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_entity_classes_class ON entity_classes(class_content_id, attained_tier); CREATE TABLE IF NOT EXISTS grand_class_seats ( class_content_id TEXT PRIMARY KEY, bearer_entity_id TEXT NOT NULL, appointed_world_tick INTEGER NOT NULL, seat_state TEXT NOT NULL DEFAULT 'active', mandate_state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(bearer_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_grand_class_seats_bearer ON grand_class_seats(bearer_entity_id, seat_state); CREATE TABLE IF NOT EXISTS entity_skills ( owner_entity_id TEXT NOT NULL, skill_content_id TEXT NOT NULL, learned_world_tick INTEGER NOT NULL, current_state TEXT NOT NULL DEFAULT 'learned', development_state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(owner_entity_id, skill_content_id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS skill_provenance ( owner_entity_id TEXT NOT NULL, skill_content_id TEXT NOT NULL, source_kind TEXT NOT NULL, source_content_or_entity_id TEXT NOT NULL, source_world_tick INTEGER NOT NULL, PRIMARY KEY( owner_entity_id, skill_content_id, source_kind, source_content_or_entity_id), FOREIGN KEY(owner_entity_id, skill_content_id) REFERENCES entity_skills(owner_entity_id, skill_content_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS manifestation_route_nodes ( manifestation_entity_id TEXT NOT NULL, route_content_id TEXT NOT NULL, node_content_id TEXT NOT NULL, state TEXT NOT NULL, entered_world_tick INTEGER, completed_world_tick INTEGER, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(manifestation_entity_id, route_content_id, node_content_id), FOREIGN KEY(manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE ); CREATE INDEX IF NOT EXISTS idx_manifestation_route_nodes_state ON manifestation_route_nodes(manifestation_entity_id, state); CREATE TABLE IF NOT EXISTS manifestation_forms ( manifestation_entity_id TEXT NOT NULL, form_content_id TEXT NOT NULL, state TEXT NOT NULL, unlocked_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(manifestation_entity_id, form_content_id), FOREIGN KEY(manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS manifestation_reinforcement ( manifestation_entity_id TEXT PRIMARY KEY, reinforcement_state TEXT NOT NULL, max_reinforced INTEGER NOT NULL DEFAULT 0 CHECK(max_reinforced IN (0,1)), updated_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS entity_transcendence_state ( entity_id TEXT PRIMARY KEY, grade_content_id TEXT NOT NULL, breakthrough_world_tick INTEGER NOT NULL, qualification_snapshot_json TEXT NOT NULL DEFAULT '{}', proof_provenance_json TEXT NOT NULL DEFAULT '{}', state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS manifestation_world_fantasm_state ( manifestation_entity_id TEXT PRIMARY KEY, grade_content_id TEXT NOT NULL, unlocked_world_tick INTEGER NOT NULL, expression_profile_content_id TEXT, evolution_state_json TEXT NOT NULL DEFAULT '{}', updated_world_tick INTEGER NOT NULL, FOREIGN KEY(manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS protagonist_world_manifestation_state ( owner_entity_id TEXT PRIMARY KEY, unlocked_world_tick INTEGER NOT NULL, expression_profile_content_id TEXT, provenance_state_json TEXT NOT NULL DEFAULT '{}', evolution_state_json TEXT NOT NULL DEFAULT '{}', updated_world_tick INTEGER NOT NULL, FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS character_convergences ( convergence_entity_id TEXT PRIMARY KEY, identity_content_id TEXT NOT NULL, result_manifestation_entity_id TEXT NOT NULL, rule_content_id TEXT NOT NULL, world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(convergence_entity_id) REFERENCES entities(id), FOREIGN KEY(result_manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ); CREATE INDEX IF NOT EXISTS idx_character_convergences_identity ON character_convergences(identity_content_id, world_tick); CREATE TABLE IF NOT EXISTS character_convergence_sources ( convergence_entity_id TEXT NOT NULL, source_manifestation_entity_id TEXT NOT NULL, lineage_content_id TEXT, ordinal INTEGER NOT NULL, PRIMARY KEY(convergence_entity_id, source_manifestation_entity_id), FOREIGN KEY(convergence_entity_id) REFERENCES character_convergences(convergence_entity_id) ON DELETE CASCADE, FOREIGN KEY(source_manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) );");

static const TCHAR* Migration0011Sql =
    TEXT("CREATE TABLE IF NOT EXISTS time_domains ( time_domain_entity_id TEXT PRIMARY KEY, parent_time_domain_entity_id TEXT, rate_numerator INTEGER NOT NULL DEFAULT 1 CHECK(rate_numerator > 0), rate_denominator INTEGER NOT NULL DEFAULT 1 CHECK(rate_denominator > 0), parent_epoch_tick INTEGER NOT NULL DEFAULT 0, local_epoch_tick INTEGER NOT NULL DEFAULT 0, calendar_content_id TEXT, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(time_domain_entity_id) REFERENCES entities(id), FOREIGN KEY(parent_time_domain_entity_id) REFERENCES time_domains(time_domain_entity_id) ); CREATE INDEX IF NOT EXISTS idx_time_domains_parent ON time_domains(parent_time_domain_entity_id); CREATE TABLE IF NOT EXISTS reality_nodes ( reality_entity_id TEXT PRIMARY KEY, parent_reality_entity_id TEXT, kind TEXT NOT NULL, world_rank_content_id TEXT, time_domain_entity_id TEXT, law_profile_content_id TEXT, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(reality_entity_id) REFERENCES entities(id), FOREIGN KEY(parent_reality_entity_id) REFERENCES reality_nodes(reality_entity_id), FOREIGN KEY(time_domain_entity_id) REFERENCES time_domains(time_domain_entity_id) ); CREATE INDEX IF NOT EXISTS idx_reality_nodes_parent ON reality_nodes(parent_reality_entity_id); CREATE INDEX IF NOT EXISTS idx_reality_nodes_time ON reality_nodes(time_domain_entity_id); CREATE TABLE IF NOT EXISTS junctions ( junction_entity_id TEXT PRIMARY KEY, from_reality_entity_id TEXT NOT NULL, to_reality_entity_id TEXT NOT NULL, state TEXT NOT NULL, stability_bps INTEGER NOT NULL DEFAULT 10000 CHECK(stability_bps BETWEEN 0 AND 10000), opened_world_tick INTEGER, closed_world_tick INTEGER, requirements_json TEXT NOT NULL DEFAULT '{}', CHECK(from_reality_entity_id <> to_reality_entity_id), FOREIGN KEY(junction_entity_id) REFERENCES entities(id), FOREIGN KEY(from_reality_entity_id) REFERENCES reality_nodes(reality_entity_id), FOREIGN KEY(to_reality_entity_id) REFERENCES reality_nodes(reality_entity_id) ); CREATE INDEX IF NOT EXISTS idx_junctions_from ON junctions(from_reality_entity_id, state); CREATE INDEX IF NOT EXISTS idx_junctions_to ON junctions(to_reality_entity_id, state); CREATE TABLE IF NOT EXISTS world_director_schedule ( schedule_entity_id TEXT PRIMARY KEY, content_id TEXT NOT NULL, template_content_id TEXT, status TEXT NOT NULL, eligible_since_world_tick INTEGER, scheduled_start_world_tick INTEGER, latest_start_world_tick INTEGER, resolution_seed INTEGER NOT NULL, decision_provenance_json TEXT NOT NULL DEFAULT '{}', CHECK( scheduled_start_world_tick IS NULL OR latest_start_world_tick IS NULL OR scheduled_start_world_tick <= latest_start_world_tick), FOREIGN KEY(schedule_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_world_director_schedule_status ON world_director_schedule(status, scheduled_start_world_tick); CREATE INDEX IF NOT EXISTS idx_world_director_schedule_content ON world_director_schedule(content_id, scheduled_start_world_tick); CREATE TABLE IF NOT EXISTS content_unlock_state ( content_id TEXT PRIMARY KEY, state TEXT NOT NULL, eligible_world_tick INTEGER, released_world_tick INTEGER, state_json TEXT NOT NULL DEFAULT '{}' ); CREATE TABLE IF NOT EXISTS offline_simulation_state ( scope_entity_id TEXT PRIMARY KEY, last_active_world_tick INTEGER NOT NULL, last_catchup_world_tick INTEGER NOT NULL, governor_state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(scope_entity_id) REFERENCES entities(id) );");

static const TCHAR* Migration0012Sql =
    TEXT("ALTER TABLE dispatches ADD COLUMN risk_tolerance_bps INTEGER NOT NULL DEFAULT 5000 CHECK(risk_tolerance_bps BETWEEN 0 AND 10000); ALTER TABLE dispatches ADD COLUMN abort_policy_json TEXT NOT NULL DEFAULT '{}'; ALTER TABLE dispatches ADD COLUMN outcome_state TEXT NOT NULL DEFAULT ''; ALTER TABLE dispatches ADD COLUMN delay_until_world_tick INTEGER; CREATE TABLE IF NOT EXISTS dispatch_objectives ( dispatch_entity_id TEXT NOT NULL, objective_content_id TEXT NOT NULL, priority INTEGER NOT NULL DEFAULT 0, mandatory INTEGER NOT NULL DEFAULT 0 CHECK(mandatory IN (0,1)), target_entity_id TEXT, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(dispatch_entity_id, objective_content_id), FOREIGN KEY(dispatch_entity_id) REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE, FOREIGN KEY(target_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_dispatch_objectives_priority ON dispatch_objectives(dispatch_entity_id, mandatory DESC, priority DESC); CREATE TABLE IF NOT EXISTS dispatch_constraints ( dispatch_entity_id TEXT NOT NULL, constraint_content_id TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(dispatch_entity_id, constraint_content_id), FOREIGN KEY(dispatch_entity_id) REFERENCES dispatches(dispatch_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS war_fronts ( front_entity_id TEXT PRIMARY KEY, war_entity_id TEXT NOT NULL, location_entity_id TEXT, reality_entity_id TEXT, state TEXT NOT NULL, start_world_tick INTEGER NOT NULL, end_world_tick INTEGER, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(front_entity_id) REFERENCES entities(id), FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE, FOREIGN KEY(location_entity_id) REFERENCES locations(location_entity_id), FOREIGN KEY(reality_entity_id) REFERENCES reality_nodes(reality_entity_id) ); CREATE INDEX IF NOT EXISTS idx_war_fronts_parent ON war_fronts(war_entity_id, state, start_world_tick); CREATE TABLE IF NOT EXISTS war_objectives ( war_entity_id TEXT NOT NULL, front_entity_id TEXT NOT NULL DEFAULT '', objective_content_id TEXT NOT NULL, target_entity_id TEXT, priority INTEGER NOT NULL DEFAULT 0, status TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(war_entity_id, front_entity_id, objective_content_id), FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE, FOREIGN KEY(target_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_war_objectives_status ON war_objectives(war_entity_id, status, priority DESC); CREATE TABLE IF NOT EXISTS war_orders ( order_entity_id TEXT PRIMARY KEY, war_entity_id TEXT NOT NULL, front_entity_id TEXT, issuer_entity_id TEXT NOT NULL, recipient_entity_id TEXT NOT NULL, intent_content_id TEXT NOT NULL, constraints_json TEXT NOT NULL DEFAULT '{}', issued_world_tick INTEGER NOT NULL, outcome_state TEXT NOT NULL DEFAULT 'pending', outcome_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(order_entity_id) REFERENCES entities(id), FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE, FOREIGN KEY(front_entity_id) REFERENCES war_fronts(front_entity_id), FOREIGN KEY(issuer_entity_id) REFERENCES entities(id), FOREIGN KEY(recipient_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_war_orders_parent ON war_orders(war_entity_id, issued_world_tick); CREATE TABLE IF NOT EXISTS war_participant_history ( war_entity_id TEXT NOT NULL, faction_entity_id TEXT NOT NULL, side_index INTEGER NOT NULL, joined_world_tick INTEGER NOT NULL, left_world_tick INTEGER, reason TEXT, PRIMARY KEY(war_entity_id, faction_entity_id, joined_world_tick), FOREIGN KEY(war_entity_id) REFERENCES wars(war_entity_id) ON DELETE CASCADE, FOREIGN KEY(faction_entity_id) REFERENCES factions(faction_entity_id) ); CREATE INDEX IF NOT EXISTS idx_war_participant_history_faction ON war_participant_history(faction_entity_id, joined_world_tick); CREATE TABLE IF NOT EXISTS army_capabilities ( army_entity_id TEXT NOT NULL, capability_content_id TEXT NOT NULL, magnitude_sig INTEGER NOT NULL, magnitude_exp INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(army_entity_id, capability_content_id), FOREIGN KEY(army_entity_id) REFERENCES armies(army_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS project_phases ( project_entity_id TEXT NOT NULL, phase_content_id TEXT NOT NULL, sequence INTEGER NOT NULL, status TEXT NOT NULL, start_world_tick INTEGER NOT NULL, resolve_world_tick INTEGER NOT NULL, progress_bps INTEGER NOT NULL DEFAULT 0 CHECK(progress_bps BETWEEN 0 AND 10000), payload_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(project_entity_id, phase_content_id), FOREIGN KEY(project_entity_id) REFERENCES projects(project_entity_id) ON DELETE CASCADE ); CREATE INDEX IF NOT EXISTS idx_project_phases_sequence ON project_phases(project_entity_id, sequence); CREATE TABLE IF NOT EXISTS project_assignments ( project_entity_id TEXT NOT NULL, assignee_entity_id TEXT NOT NULL, role_content_id TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(project_entity_id, assignee_entity_id, role_content_id), FOREIGN KEY(project_entity_id) REFERENCES projects(project_entity_id) ON DELETE CASCADE, FOREIGN KEY(assignee_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS civilization_state ( civilization_entity_id TEXT PRIMARY KEY, genre_profile_content_id TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(civilization_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS civilization_dimensions ( civilization_entity_id TEXT NOT NULL, dimension_content_id TEXT NOT NULL, grade TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(civilization_entity_id, dimension_content_id), FOREIGN KEY(civilization_entity_id) REFERENCES civilization_state(civilization_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS logistics_routes ( route_entity_id TEXT PRIMARY KEY, owner_entity_id TEXT NOT NULL, origin_location_entity_id TEXT, origin_reality_entity_id TEXT, destination_location_entity_id TEXT, destination_reality_entity_id TEXT, transport_capability_content_id TEXT NOT NULL, capacity_sig INTEGER NOT NULL, capacity_exp INTEGER NOT NULL, risk_bps INTEGER NOT NULL DEFAULT 0 CHECK(risk_bps BETWEEN 0 AND 10000), status TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', CHECK(origin_location_entity_id IS NOT NULL OR origin_reality_entity_id IS NOT NULL), CHECK(destination_location_entity_id IS NOT NULL OR destination_reality_entity_id IS NOT NULL), FOREIGN KEY(route_entity_id) REFERENCES entities(id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id), FOREIGN KEY(origin_location_entity_id) REFERENCES locations(location_entity_id), FOREIGN KEY(origin_reality_entity_id) REFERENCES reality_nodes(reality_entity_id), FOREIGN KEY(destination_location_entity_id) REFERENCES locations(location_entity_id), FOREIGN KEY(destination_reality_entity_id) REFERENCES reality_nodes(reality_entity_id) ); CREATE INDEX IF NOT EXISTS idx_logistics_routes_owner ON logistics_routes(owner_entity_id, status);");

static const TCHAR* Migration0013Sql =
    TEXT("CREATE TABLE IF NOT EXISTS item_instances ( item_entity_id TEXT PRIMARY KEY, definition_content_id TEXT NOT NULL, owner_entity_id TEXT, current_rank_content_id TEXT, quality_content_id TEXT, durability_state_json TEXT NOT NULL DEFAULT '{}', evolution_state_json TEXT NOT NULL DEFAULT '{}', history_state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(item_entity_id) REFERENCES entities(id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_item_instances_owner ON item_instances(owner_entity_id); CREATE TABLE IF NOT EXISTS item_modifiers ( item_entity_id TEXT NOT NULL, modifier_content_id TEXT NOT NULL, ordinal INTEGER NOT NULL DEFAULT 0 CHECK(ordinal >= 0), state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(item_entity_id, modifier_content_id, ordinal), FOREIGN KEY(item_entity_id) REFERENCES item_instances(item_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS equipment_bindings ( wearer_entity_id TEXT NOT NULL, slot_content_id TEXT NOT NULL, item_entity_id TEXT NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(wearer_entity_id, slot_content_id), FOREIGN KEY(wearer_entity_id) REFERENCES entities(id), FOREIGN KEY(item_entity_id) REFERENCES item_instances(item_entity_id) ); CREATE INDEX IF NOT EXISTS idx_equipment_bindings_item ON equipment_bindings(item_entity_id); CREATE TABLE IF NOT EXISTS inventory_containers ( container_entity_id TEXT PRIMARY KEY, owner_entity_id TEXT NOT NULL, container_type_content_id TEXT NOT NULL, capacity_state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(container_entity_id) REFERENCES entities(id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE INDEX IF NOT EXISTS idx_inventory_containers_owner ON inventory_containers(owner_entity_id); CREATE TABLE IF NOT EXISTS container_contents ( container_entity_id TEXT NOT NULL, item_entity_id TEXT NOT NULL, amount INTEGER NOT NULL DEFAULT 1 CHECK(amount > 0), PRIMARY KEY(container_entity_id, item_entity_id), FOREIGN KEY(container_entity_id) REFERENCES inventory_containers(container_entity_id) ON DELETE CASCADE, FOREIGN KEY(item_entity_id) REFERENCES item_instances(item_entity_id) ); CREATE TABLE IF NOT EXISTS item_owner_affinity ( item_entity_id TEXT NOT NULL, owner_entity_id TEXT NOT NULL, affinity_value INTEGER NOT NULL DEFAULT 0, milestone_content_id TEXT, updated_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(item_entity_id, owner_entity_id), FOREIGN KEY(item_entity_id) REFERENCES item_instances(item_entity_id) ON DELETE CASCADE, FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS entity_equipment_proficiency ( owner_entity_id TEXT NOT NULL, proficiency_content_id TEXT NOT NULL, proficiency_value INTEGER NOT NULL DEFAULT 0, grade_content_id TEXT, updated_world_tick INTEGER NOT NULL, state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(owner_entity_id, proficiency_content_id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS manifestation_presentation_state ( manifestation_entity_id TEXT PRIMARY KEY, selected_skin_content_id TEXT, outfit_state_json TEXT NOT NULL DEFAULT '{}', presentation_variant_state_json TEXT NOT NULL DEFAULT '{}', updated_world_tick INTEGER NOT NULL, FOREIGN KEY(manifestation_entity_id) REFERENCES character_manifestations(manifestation_entity_id) ON DELETE CASCADE ); CREATE TABLE IF NOT EXISTS owned_presentation_unlocks ( owner_entity_id TEXT NOT NULL, presentation_content_id TEXT NOT NULL, acquired_world_tick INTEGER NOT NULL, state TEXT NOT NULL DEFAULT 'owned', PRIMARY KEY(owner_entity_id, presentation_content_id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id) ); ALTER TABLE knowledge_facts ADD COLUMN belief_state TEXT NOT NULL DEFAULT 'believed'; ALTER TABLE knowledge_facts ADD COLUMN confidence_bps INTEGER NOT NULL DEFAULT 10000 CHECK(confidence_bps BETWEEN 0 AND 10000); ALTER TABLE knowledge_facts ADD COLUMN source_entity_id TEXT; ALTER TABLE knowledge_facts ADD COLUMN source_event_id TEXT; ALTER TABLE knowledge_facts ADD COLUMN evidence_world_tick INTEGER; ALTER TABLE knowledge_facts ADD COLUMN language_context_content_id TEXT; CREATE TABLE IF NOT EXISTS entity_languages ( entity_id TEXT NOT NULL, language_content_id TEXT NOT NULL, spoken_proficiency_bps INTEGER NOT NULL DEFAULT 0 CHECK(spoken_proficiency_bps BETWEEN 0 AND 10000), written_proficiency_bps INTEGER NOT NULL DEFAULT 0 CHECK(written_proficiency_bps BETWEEN 0 AND 10000), state_json TEXT NOT NULL DEFAULT '{}', PRIMARY KEY(entity_id, language_content_id), FOREIGN KEY(entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS semantic_memories ( memory_entity_id TEXT PRIMARY KEY, owner_entity_id TEXT NOT NULL, subject_entity_id TEXT, source_event_id TEXT, memory_type_content_id TEXT NOT NULL, salience_bps INTEGER NOT NULL DEFAULT 0 CHECK(salience_bps BETWEEN 0 AND 10000), state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(memory_entity_id) REFERENCES entities(id), FOREIGN KEY(owner_entity_id) REFERENCES entities(id), FOREIGN KEY(subject_entity_id) REFERENCES entities(id), FOREIGN KEY(source_event_id) REFERENCES world_events(event_id) ); CREATE INDEX IF NOT EXISTS idx_semantic_memories_owner ON semantic_memories(owner_entity_id, salience_bps DESC); CREATE TABLE IF NOT EXISTS npc_promotion_state ( entity_id TEXT PRIMARY KEY, simulation_tier_content_id TEXT NOT NULL, promoted_world_tick INTEGER NOT NULL, reason_event_id TEXT, presentation_package_state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(entity_id) REFERENCES entities(id), FOREIGN KEY(reason_event_id) REFERENCES world_events(event_id) ); CREATE TABLE IF NOT EXISTS character_adult_runtime_state ( character_entity_id TEXT PRIMARY KEY, current_profile_variant_content_id TEXT, mutable_context_state_json TEXT NOT NULL DEFAULT '{}', updated_world_tick INTEGER NOT NULL, FOREIGN KEY(character_entity_id) REFERENCES entities(id) ); CREATE TABLE IF NOT EXISTS heroic_records ( record_entity_id TEXT PRIMARY KEY, source_world_entity_id TEXT NOT NULL, identity_content_id TEXT NOT NULL, death_event_id TEXT NOT NULL, created_world_tick INTEGER NOT NULL, pattern_content_id TEXT, gacha_access_state TEXT NOT NULL DEFAULT 'locked', state_json TEXT NOT NULL DEFAULT '{}', FOREIGN KEY(record_entity_id) REFERENCES entities(id), FOREIGN KEY(source_world_entity_id) REFERENCES entities(id), FOREIGN KEY(death_event_id) REFERENCES world_events(event_id) ); CREATE INDEX IF NOT EXISTS idx_heroic_records_identity ON heroic_records(identity_content_id, created_world_tick);");

static const FOGMigrationDefinition Migrations[] =
{
    {1, TEXT("bootstrap"), Migration0001Sql, {}, {}},
    {2, TEXT("character_manifestations"), Migration0002Sql, {}, {}},
    {3, TEXT("world_location_state"), Migration0003Sql, {}, {}},
    {4, TEXT("territory_resources_projects"), Migration0004Sql, {}, {}},
    {5, TEXT("dispatch_faction_war"), Migration0005Sql, {}, {}},
    {6, TEXT("gacha_state"), Migration0006Sql, {}, {}},
    {
        7,
        TEXT("manifestation_instances"),
        Migration0007Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateLegacyDuplicateManifestations0007(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateManifestationMigration0007(Error);
        }
    },
    {
        8,
        TEXT("territory_sovereignty_gacha_access"),
        Migration0008Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateTerritorySovereignty0008(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateTerritorySovereigntyMigration0008(Error);
        }
    },
    {
        9,
        TEXT("domain_heart_core_fusion"),
        Migration0009Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateDomainHeartAndConcepts0009(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateDomainHeartMigration0009(Error);
        }
    },
    {
        10,
        TEXT("normalized_progression"),
        Migration0010Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateProgression0010(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateProgressionMigration0010(Error);
        }
    },
    {
        11,
        TEXT("reality_time_world_director"),
        Migration0011Sql,
        {},
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateRealityTimeDirectorMigration0011(Error);
        }
    },
    {
        12,
        TEXT("strategy_civilization_logistics"),
        Migration0012Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateStrategy0012(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateStrategyMigration0012(Error);
        }
    },
    {
        13,
        TEXT("items_knowledge_character_runtime"),
        Migration0013Sql,
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.MigrateItemsKnowledgeCharacters0013(Error);
        },
        [](FOGSQLiteWorldStore& Store, FString& Error)
        {
            return Store.ValidateItemsKnowledgeCharactersMigration0013(Error);
        }
    },
};

FString RelatedEntitiesToJson(const TArray<FOGEntityId>& EntityIds)
{
    TArray<FString> Values;
    Values.Reserve(EntityIds.Num());
    for (const FOGEntityId& EntityId : EntityIds)
    {
        Values.Add(FString::Printf(TEXT("\"%s\""), *EntityId.ToString()));
    }

    return FString::Printf(TEXT("[%s]"), *FString::Join(Values, TEXT(",")));
}

bool BindText(sqlite3_stmt* Statement, int32 Index, const FString& Value)
{
    FTCHARToUTF8 Utf8(*Value);
    return sqlite3_bind_text(
        Statement,
        Index,
        Utf8.Get(),
        Utf8.Length(),
        SQLITE_TRANSIENT) == SQLITE_OK;
}

FString ColumnText(sqlite3_stmt* Statement, int32 Column)
{
    const unsigned char* Text = sqlite3_column_text(Statement, Column);
    return Text ? UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text)) : FString();
}
}

FOGSQLiteWorldStore::~FOGSQLiteWorldStore()
{
    Close();
}

bool FOGSQLiteWorldStore::Open(const FString& AbsoluteDatabasePath, FString& OutError)
{
    Close();
    OutError.Reset();

    if (AbsoluteDatabasePath.IsEmpty())
    {
        OutError = TEXT("Database path is empty.");
        return false;
    }

    const FString ParentDirectory = FPaths::GetPath(AbsoluteDatabasePath);
    if (!ParentDirectory.IsEmpty() &&
        !IFileManager::Get().MakeDirectory(*ParentDirectory, true) &&
        !IFileManager::Get().DirectoryExists(*ParentDirectory))
    {
        OutError = FString::Printf(TEXT("Failed to create database directory: %s"), *ParentDirectory);
        return false;
    }

    FTCHARToUTF8 PathUtf8(*AbsoluteDatabasePath);
    const int32 OpenResult = sqlite3_open_v2(
        PathUtf8.Get(),
        &Database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (OpenResult != SQLITE_OK || Database == nullptr)
    {
        OutError = LastError(TEXT("sqlite3_open_v2"));
        Close();
        return false;
    }

    DatabasePath = AbsoluteDatabasePath;
    sqlite3_extended_result_codes(Database, 1);
    sqlite3_busy_timeout(Database, 5000);

    if (!ExecuteSql(TEXT("PRAGMA foreign_keys = ON;"), OutError) ||
        !ExecuteSql(TEXT("PRAGMA journal_mode = WAL;"), OutError) ||
        !ExecuteSql(TEXT("PRAGMA synchronous = NORMAL;"), OutError) ||
        !ExecuteSql(TEXT("PRAGMA temp_store = MEMORY;"), OutError) ||
        !EnsureMigrationTable(OutError) ||
        !ApplyMigrations(OutError))
    {
        Close();
        return false;
    }

    UE_LOG(LogOfflineGame, Log, TEXT("Opened authoritative world database: %s"), *DatabasePath);
    return true;
}

void FOGSQLiteWorldStore::Close()
{
    if (Database == nullptr)
    {
        DatabasePath.Reset();
        bTransactionActive = false;
        return;
    }

    if (bTransactionActive)
    {
        char* ErrorMessage = nullptr;
        sqlite3_exec(Database, "ROLLBACK;", nullptr, nullptr, &ErrorMessage);
        if (ErrorMessage)
        {
            sqlite3_free(ErrorMessage);
        }
        bTransactionActive = false;
    }

    char* ErrorMessage = nullptr;
    sqlite3_exec(Database, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, &ErrorMessage);
    if (ErrorMessage)
    {
        sqlite3_free(ErrorMessage);
    }

    sqlite3_close_v2(Database);
    Database = nullptr;
    DatabasePath.Reset();
}

bool FOGSQLiteWorldStore::ExecuteSql(const FString& Sql, FString& OutError) const
{
    if (Database == nullptr)
    {
        OutError = TEXT("Database is not open.");
        return false;
    }

    FTCHARToUTF8 SqlUtf8(*Sql);
    char* ErrorMessage = nullptr;
    const int32 Result = sqlite3_exec(Database, SqlUtf8.Get(), nullptr, nullptr, &ErrorMessage);
    if (Result != SQLITE_OK)
    {
        const FString Detail = ErrorMessage
            ? UTF8_TO_TCHAR(ErrorMessage)
            : UTF8_TO_TCHAR(sqlite3_errmsg(Database));

        if (ErrorMessage)
        {
            sqlite3_free(ErrorMessage);
        }

        OutError = FString::Printf(TEXT("SQLite execution failed (%d): %s"), Result, *Detail);
        return false;
    }

    OutError.Reset();
    return true;
}

bool FOGSQLiteWorldStore::EnsureMigrationTable(FString& OutError)
{
    return ExecuteSql(
        TEXT("CREATE TABLE IF NOT EXISTS schema_migrations (")
        TEXT("version INTEGER PRIMARY KEY,")
        TEXT("name TEXT NOT NULL,")
        TEXT("applied_at_utc TEXT NOT NULL);"),
        OutError);
}

int32 FOGSQLiteWorldStore::GetSchemaVersion(FString& OutError) const
{
    OutError.Reset();
    if (Database == nullptr)
    {
        OutError = TEXT("Database is not open.");
        return INDEX_NONE;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql = "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;";
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare schema version query"));
        return INDEX_NONE;
    }

    int32 Version = INDEX_NONE;
    if (sqlite3_step(Statement) == SQLITE_ROW)
    {
        Version = sqlite3_column_int(Statement, 0);
    }
    else
    {
        OutError = LastError(TEXT("Read schema version"));
    }

    sqlite3_finalize(Statement);
    return Version;
}

int32 FOGSQLiteWorldStore::LatestSchemaVersion()
{
    return Migrations[UE_ARRAY_COUNT(Migrations) - 1].Version;
}

bool FOGSQLiteWorldStore::RunApplicationValidation(
    FString& OutReport,
    FString& OutError) const
{
    OutReport.Reset();
    OutError.Reset();

    if (Database == nullptr)
    {
        OutError = TEXT("Database is not open.");
        return false;
    }

    FString SchemaError;
    const int32 SchemaVersion = GetSchemaVersion(SchemaError);
    if (SchemaVersion == INDEX_NONE)
    {
        OutError = SchemaError;
        return false;
    }

    if (SchemaVersion != LatestSchemaVersion())
    {
        OutError = FString::Printf(
            TEXT("Schema version %d does not match runtime target %d."),
            SchemaVersion,
            LatestSchemaVersion());
        return false;
    }

    sqlite3_stmt* MigrationStatement = nullptr;
    const char* MigrationSql =
        "SELECT COUNT(*), COALESCE(MIN(version), 0), COALESCE(MAX(version), 0) "
        "FROM schema_migrations;";

    if (sqlite3_prepare_v2(
            Database,
            MigrationSql,
            -1,
            &MigrationStatement,
            nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare migration-continuity validation"));
        return false;
    }

    int32 MigrationCount = 0;
    int32 MinimumVersion = 0;
    int32 MaximumVersion = 0;

    if (sqlite3_step(MigrationStatement) == SQLITE_ROW)
    {
        MigrationCount = sqlite3_column_int(MigrationStatement, 0);
        MinimumVersion = sqlite3_column_int(MigrationStatement, 1);
        MaximumVersion = sqlite3_column_int(MigrationStatement, 2);
    }
    else
    {
        OutError = LastError(TEXT("Read migration-continuity validation"));
        sqlite3_finalize(MigrationStatement);
        return false;
    }

    sqlite3_finalize(MigrationStatement);

    if (MigrationCount != LatestSchemaVersion() ||
        MinimumVersion != 1 ||
        MaximumVersion != LatestSchemaVersion())
    {
        OutError = FString::Printf(
            TEXT("Migration ledger is not contiguous (count=%d, min=%d, max=%d, expected=1..%d)."),
            MigrationCount,
            MinimumVersion,
            MaximumVersion,
            LatestSchemaVersion());
        return false;
    }

    sqlite3_stmt* ForeignKeyStatement = nullptr;
    if (sqlite3_prepare_v2(
            Database,
            "PRAGMA foreign_key_check;",
            -1,
            &ForeignKeyStatement,
            nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare foreign-key validation"));
        return false;
    }

    const int32 ForeignKeyStep =
        sqlite3_step(ForeignKeyStatement);
    if (ForeignKeyStep == SQLITE_ROW)
    {
        const FString TableName =
            ColumnText(ForeignKeyStatement, 0);
        const int64 RowId =
            sqlite3_column_int64(ForeignKeyStatement, 1);
        sqlite3_finalize(ForeignKeyStatement);

        OutError = FString::Printf(
            TEXT("Foreign-key validation failed in table %s at row %lld."),
            *TableName,
            static_cast<long long>(RowId));
        return false;
    }

    if (ForeignKeyStep != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Run foreign-key validation"));
        sqlite3_finalize(ForeignKeyStatement);
        return false;
    }

    sqlite3_finalize(ForeignKeyStatement);

    FString IntegrityReport;
    if (!RunIntegrityCheck(
            IntegrityReport,
            OutError))
    {
        return false;
    }

    OutReport = FString::Printf(
        TEXT("schema=%d; migrations=%d; foreign_keys=ok; integrity=%s"),
        SchemaVersion,
        MigrationCount,
        *IntegrityReport);
    return true;
}

bool FOGSQLiteWorldStore::RecordMigration(int32 Version, const TCHAR* Name, FString& OutError)
{
    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO schema_migrations(version, name, applied_at_utc) "
        "VALUES(?, ?, strftime('%Y-%m-%dT%H:%M:%fZ','now'));";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare migration record"));
        return false;
    }

    const FString NameString(Name);
    const bool bBound =
        sqlite3_bind_int(Statement, 1, Version) == SQLITE_OK &&
        BindText(Statement, 2, NameString);

    const bool bSucceeded = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Record migration"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::ApplyMigrations(FString& OutError)
{
    int32 CurrentVersion = GetSchemaVersion(OutError);
    if (CurrentVersion == INDEX_NONE)
    {
        return false;
    }

    for (const FOGMigrationDefinition& Migration : Migrations)
    {
        if (Migration.Version <= CurrentVersion)
        {
            continue;
        }

        if (!BeginTransaction(OutError))
        {
            return false;
        }

        bool bMigrationSucceeded =
            ExecuteSql(Migration.Sql, OutError);

        if (bMigrationSucceeded &&
            Migration.DataTransform)
        {
            bMigrationSucceeded =
                Migration.DataTransform(*this, OutError);
        }

        if (bMigrationSucceeded)
        {
            bMigrationSucceeded =
                RecordMigration(
                    Migration.Version,
                    Migration.Name,
                    OutError);
        }

        if (bMigrationSucceeded &&
            Migration.Validate)
        {
            bMigrationSucceeded =
                Migration.Validate(*this, OutError);
        }

        if (bMigrationSucceeded)
        {
            bMigrationSucceeded =
                CommitTransaction(OutError);
        }

        if (!bMigrationSucceeded)
        {
            FString RollbackError;
            RollbackTransaction(RollbackError);
            if (!RollbackError.IsEmpty())
            {
                OutError += FString::Printf(TEXT(" | Rollback error: %s"), *RollbackError);
            }
            return false;
        }

        CurrentVersion = Migration.Version;
        UE_LOG(LogOfflineGame, Log, TEXT("Applied database migration %d (%s)."), Migration.Version, Migration.Name);
    }

    return true;
}

bool FOGSQLiteWorldStore::BeginTransaction(FString& OutError)
{
    if (bTransactionActive)
    {
        OutError = TEXT("Nested world-store transactions are not supported.");
        return false;
    }

    if (!ExecuteSql(TEXT("BEGIN IMMEDIATE;"), OutError))
    {
        return false;
    }

    bTransactionActive = true;
    return true;
}

bool FOGSQLiteWorldStore::CommitTransaction(FString& OutError)
{
    if (!bTransactionActive)
    {
        OutError = TEXT("No active world-store transaction to commit.");
        return false;
    }

    if (!ExecuteSql(TEXT("COMMIT;"), OutError))
    {
        return false;
    }

    bTransactionActive = false;
    return true;
}

bool FOGSQLiteWorldStore::RollbackTransaction(FString& OutError)
{
    if (!bTransactionActive)
    {
        OutError.Reset();
        return true;
    }

    const bool bSucceeded = ExecuteSql(TEXT("ROLLBACK;"), OutError);
    if (bSucceeded)
    {
        bTransactionActive = false;
    }
    return bSucceeded;
}

bool FOGSQLiteWorldStore::UpsertEntity(
    const FOGEntityId& EntityId,
    FName Kind,
    int64 CreatedWorldTick,
    const FString& StateJson,
    FString& OutError)
{
    OutError.Reset();
    if (!EntityId.IsValid() || Kind.IsNone())
    {
        OutError = TEXT("Cannot persist an entity without a valid ID and kind.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO entities(id, kind, created_world_tick, revision, state_json) "
        "VALUES(?, ?, ?, 0, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "kind = excluded.kind, "
        "revision = entities.revision + 1, "
        "state_json = excluded.state_json;";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare entity upsert"));
        return false;
    }

    const bool bBound =
        BindText(Statement, 1, EntityId.ToString()) &&
        BindText(Statement, 2, Kind.ToString()) &&
        sqlite3_bind_int64(Statement, 3, CreatedWorldTick) == SQLITE_OK &&
        BindText(Statement, 4, StateJson);

    const bool bSucceeded = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Upsert entity"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::TryReadEntity(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FName& OutKind,
    FString& OutStateJson,
    int64& OutRevision,
    FString& OutError) const
{
    bOutFound = false;
    OutKind = NAME_None;
    OutStateJson.Reset();
    OutRevision = 0;
    OutError.Reset();

    sqlite3_stmt* Statement = nullptr;
    const char* Sql = "SELECT kind, state_json, revision FROM entities WHERE id = ?;";
    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare entity read"));
        return false;
    }

    if (!BindText(Statement, 1, EntityId.ToString()))
    {
        OutError = LastError(TEXT("Bind entity read"));
        sqlite3_finalize(Statement);
        return false;
    }

    const int32 StepResult = sqlite3_step(Statement);
    if (StepResult == SQLITE_ROW)
    {
        bOutFound = true;
        OutKind = FName(*ColumnText(Statement, 0));
        OutStateJson = ColumnText(Statement, 1);
        OutRevision = sqlite3_column_int64(Statement, 2);
    }
    else if (StepResult != SQLITE_DONE)
    {
        OutError = LastError(TEXT("Read entity"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    return true;
}

bool FOGSQLiteWorldStore::AppendWorldEvent(const FOGWorldEvent& Event, FString& OutError)
{
    if (!Event.EventId.IsValid() || Event.EventType.IsNone())
    {
        OutError = TEXT("World event requires a valid event ID and event type.");
        return false;
    }

    sqlite3_stmt* Statement = nullptr;
    const char* Sql =
        "INSERT INTO world_events("
        "event_id, event_type, world_tick, primary_entity_id, related_entities_json, payload_json, chronicle_eligible"
        ") VALUES(?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(Database, Sql, -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare world event insert"));
        return false;
    }

    const FString PrimaryEntity = Event.PrimaryEntity.IsValid()
        ? Event.PrimaryEntity.ToString()
        : FString();

    const bool bBound =
        BindText(Statement, 1, Event.EventId.ToString()) &&
        BindText(Statement, 2, Event.EventType.ToString()) &&
        sqlite3_bind_int64(Statement, 3, Event.WorldTick) == SQLITE_OK &&
        (PrimaryEntity.IsEmpty()
            ? sqlite3_bind_null(Statement, 4) == SQLITE_OK
            : BindText(Statement, 4, PrimaryEntity)) &&
        BindText(Statement, 5, RelatedEntitiesToJson(Event.RelatedEntities)) &&
        BindText(Statement, 6, Event.PayloadJson.IsEmpty() ? TEXT("{}") : Event.PayloadJson) &&
        sqlite3_bind_int(Statement, 7, Event.bChronicleEligible ? 1 : 0) == SQLITE_OK;

    const bool bSucceeded = bBound && sqlite3_step(Statement) == SQLITE_DONE;
    if (!bSucceeded)
    {
        OutError = LastError(TEXT("Append world event"));
    }

    sqlite3_finalize(Statement);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::BackupTo(const FString& AbsoluteBackupPath, FString& OutError)
{
    OutError.Reset();
    if (Database == nullptr)
    {
        OutError = TEXT("Database is not open.");
        return false;
    }

    const FString ParentDirectory = FPaths::GetPath(AbsoluteBackupPath);
    if (!ParentDirectory.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*ParentDirectory, true);
    }

    IFileManager::Get().Delete(*AbsoluteBackupPath, false, true, true);

    sqlite3* BackupDatabase = nullptr;
    FTCHARToUTF8 BackupPathUtf8(*AbsoluteBackupPath);
    if (sqlite3_open_v2(
            BackupPathUtf8.Get(),
            &BackupDatabase,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK)
    {
        OutError = TEXT("Failed to open backup database.");
        if (BackupDatabase)
        {
            sqlite3_close_v2(BackupDatabase);
        }
        return false;
    }

    sqlite3_backup* Backup = sqlite3_backup_init(BackupDatabase, "main", Database, "main");
    if (!Backup)
    {
        OutError = UTF8_TO_TCHAR(sqlite3_errmsg(BackupDatabase));
        sqlite3_close_v2(BackupDatabase);
        return false;
    }

    const int32 StepResult = sqlite3_backup_step(Backup, -1);
    const int32 FinishResult = sqlite3_backup_finish(Backup);
    const bool bSucceeded =
        (StepResult == SQLITE_DONE || StepResult == SQLITE_OK) &&
        FinishResult == SQLITE_OK;

    if (!bSucceeded)
    {
        OutError = UTF8_TO_TCHAR(sqlite3_errmsg(BackupDatabase));
    }

    sqlite3_close_v2(BackupDatabase);
    return bSucceeded;
}

bool FOGSQLiteWorldStore::RestoreFrom(const FString& AbsoluteBackupPath, FString& OutError)
{
    OutError.Reset();
    if (Database == nullptr)
    {
        OutError = TEXT("Database is not open.");
        return false;
    }

    if (bTransactionActive)
    {
        OutError = TEXT("Cannot restore while a world-store transaction is active.");
        return false;
    }

    sqlite3* BackupDatabase = nullptr;
    FTCHARToUTF8 BackupPathUtf8(*AbsoluteBackupPath);
    if (sqlite3_open_v2(
            BackupPathUtf8.Get(),
            &BackupDatabase,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
            nullptr) != SQLITE_OK)
    {
        OutError = TEXT("Failed to open restore source.");
        if (BackupDatabase)
        {
            sqlite3_close_v2(BackupDatabase);
        }
        return false;
    }

    sqlite3_backup* Restore = sqlite3_backup_init(Database, "main", BackupDatabase, "main");
    if (!Restore)
    {
        OutError = LastError(TEXT("Initialize restore"));
        sqlite3_close_v2(BackupDatabase);
        return false;
    }

    const int32 StepResult = sqlite3_backup_step(Restore, -1);
    const int32 FinishResult = sqlite3_backup_finish(Restore);
    sqlite3_close_v2(BackupDatabase);

    if (!((StepResult == SQLITE_DONE || StepResult == SQLITE_OK) && FinishResult == SQLITE_OK))
    {
        OutError = LastError(TEXT("Restore database"));
        return false;
    }

    if (!EnsureMigrationTable(OutError) || !ApplyMigrations(OutError))
    {
        return false;
    }

    return Checkpoint(OutError);
}

bool FOGSQLiteWorldStore::RunIntegrityCheck(FString& OutReport, FString& OutError) const
{
    OutReport.Reset();
    OutError.Reset();

    sqlite3_stmt* Statement = nullptr;
    if (sqlite3_prepare_v2(Database, "PRAGMA integrity_check;", -1, &Statement, nullptr) != SQLITE_OK)
    {
        OutError = LastError(TEXT("Prepare integrity check"));
        return false;
    }

    TArray<FString> Rows;
    bool bHealthy = true;

    while (true)
    {
        const int32 StepResult = sqlite3_step(Statement);
        if (StepResult == SQLITE_ROW)
        {
            const FString Row = ColumnText(Statement, 0);
            Rows.Add(Row);
            if (!Row.Equals(TEXT("ok"), ESearchCase::IgnoreCase))
            {
                bHealthy = false;
            }
            continue;
        }

        if (StepResult == SQLITE_DONE)
        {
            break;
        }

        OutError = LastError(TEXT("Run integrity check"));
        sqlite3_finalize(Statement);
        return false;
    }

    sqlite3_finalize(Statement);
    OutReport = FString::Join(Rows, TEXT("\n"));

    if (!bHealthy)
    {
        OutError = TEXT("SQLite integrity_check reported corruption.");
        return false;
    }

    return true;
}

bool FOGSQLiteWorldStore::Checkpoint(FString& OutError)
{
    return ExecuteSql(TEXT("PRAGMA wal_checkpoint(TRUNCATE);"), OutError);
}

FString FOGSQLiteWorldStore::LastError(const TCHAR* Context) const
{
    const FString Detail = Database
        ? UTF8_TO_TCHAR(sqlite3_errmsg(Database))
        : TEXT("database handle is null");

    return FString::Printf(TEXT("%s: %s"), Context, *Detail);
}
