#include "Runtime/OGVerticalSliceScenario.h"

#include "Combat/OGActionCombatAdapter.h"
#include "Combat/OGBattleReplay.h"
#include "Dom/JsonObject.h"
#include "Gacha/OGGachaService.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/OGSharedWorldStateService.h"
#include "World/OGTerritoryProjectService.h"

namespace
{
FOGEntityId FixedId(uint32 Tail)
{
    return FOGEntityId(
        FGuid(
            0x0A57E210,
            0x0FF11E6A,
            0x4D450000,
            Tail));
}

FOGContentId AcquiredIdentityId()
{
    return FOGContentId(
        TEXT("slice:character.acquired"));
}

FOGContentId AcquiredVersionId()
{
    return FOGContentId(
        TEXT("slice:character.acquired.base"));
}

FOGContentId PullCurrencyId()
{
    return FOGContentId(
        TEXT("slice:currency.pull"));
}

FOGContentId ProjectResourceId()
{
    return FOGContentId(
        TEXT("slice:resource.build"));
}

FOGCombatUnitState MakeUnit(
    const FOGEntityId& EntityId,
    const FOGContentId& IdentityId,
    int32 TeamIndex,
    int64 NextActionValue)
{
    FOGCombatUnitState Unit;
    Unit.UnitEntityId = EntityId;
    Unit.IdentityId = IdentityId;
    Unit.TeamIndex = TeamIndex;
    Unit.Presence = EOGCombatPresence::Active;
    Unit.CurrentHp =
        FOGLargeNumber::FromInt64(10000);
    Unit.Stats.MaxHp = Unit.CurrentHp;
    Unit.Stats.Attack =
        FOGLargeNumber::FromInt64(100);
    Unit.Stats.Defense =
        FOGLargeNumber::FromInt64(10);
    Unit.Stats.HitBps = 10000;
    Unit.NextActionValue = NextActionValue;
    Unit.DefaultActionDelay = 200;
    return Unit;
}

FOGTurnTeamState MakeTeam(
    int32 TeamIndex,
    const FOGEntityId& UnitId)
{
    FOGTurnTeamState Team;
    Team.TeamIndex = TeamIndex;

    FOGTurnSuccessionLane Lane;
    Lane.LaneIndex = 0;
    Lane.OrderedUnitIds.Add(UnitId);
    Team.Lanes.Add(MoveTemp(Lane));
    return Team;
}

bool ParseEntityId(
    const FString& Value,
    FOGEntityId& OutId)
{
    FGuid Guid;
    if (!FGuid::Parse(Value, Guid))
    {
        OutId = FOGEntityId();
        return false;
    }

    OutId = FOGEntityId(Guid);
    return true;
}

bool ReadCheckpoint(
    IOGWorldStore& Store,
    FOGVerticalSliceScenarioResult& OutResult,
    FString& OutError)
{
    bool bFound = false;
    FName Kind = NAME_None;
    FString StateJson;
    int64 Revision = 0;

    if (!Store.TryReadEntity(
            FOGVerticalSliceScenarioHarness::ScenarioCheckpointId(),
            bFound,
            Kind,
            StateJson,
            Revision,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Kind != FName(TEXT("vertical_slice_checkpoint")))
    {
        OutError =
            TEXT("Vertical-slice checkpoint is missing.");
        return false;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(StateJson);

    if (!FJsonSerializer::Deserialize(Reader, Json) ||
        !Json.IsValid())
    {
        OutError =
            TEXT("Vertical-slice checkpoint JSON is invalid.");
        return false;
    }

    FString ManifestationText;
    FString ProjectText;
    if (!Json->TryGetStringField(
            TEXT("manifestation_id"),
            ManifestationText) ||
        !Json->TryGetStringField(
            TEXT("project_id"),
            ProjectText) ||
        !Json->TryGetStringField(
            TEXT("battle_fingerprint"),
            OutResult.BattleFingerprint) ||
        !ParseEntityId(
            ManifestationText,
            OutResult.ManifestationId) ||
        !ParseEntityId(
            ProjectText,
            OutResult.ProjectId))
    {
        OutError =
            TEXT("Vertical-slice checkpoint is incomplete.");
        return false;
    }

    return true;
}

bool WriteCheckpoint(
    IOGWorldStore& Store,
    const FOGVerticalSliceScenarioResult& Result,
    int64 WorldTick,
    FString& OutError)
{
    TSharedRef<FJsonObject> Json =
        MakeShared<FJsonObject>();
    Json->SetStringField(
        TEXT("manifestation_id"),
        Result.ManifestationId.ToString());
    Json->SetStringField(
        TEXT("project_id"),
        Result.ProjectId.ToString());
    Json->SetStringField(
        TEXT("battle_fingerprint"),
        Result.BattleFingerprint);
    Json->SetStringField(
        TEXT("stage"),
        TEXT("completed"));

    FString StateJson;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&StateJson);
    FJsonSerializer::Serialize(Json, Writer);

    return Store.UpsertEntity(
        FOGVerticalSliceScenarioHarness::ScenarioCheckpointId(),
        TEXT("vertical_slice_checkpoint"),
        WorldTick,
        StateJson,
        OutError);
}
}

FOGEntityId FOGVerticalSliceScenarioHarness::ScenarioRulerId()
{
    return FixedId(1);
}

FOGEntityId FOGVerticalSliceScenarioHarness::ScenarioLocationId()
{
    return FixedId(2);
}

FOGEntityId FOGVerticalSliceScenarioHarness::ScenarioTerritoryId()
{
    return FixedId(3);
}

FOGEntityId FOGVerticalSliceScenarioHarness::ScenarioCoreId()
{
    return FixedId(4);
}

FOGEntityId FOGVerticalSliceScenarioHarness::ScenarioCheckpointId()
{
    return FixedId(5);
}

bool FOGVerticalSliceScenarioHarness::RunFresh(
    IOGWorldStore& Store,
    FOGVerticalSliceScenarioResult& OutResult,
    FString& OutError)
{
    OutResult = FOGVerticalSliceScenarioResult();
    OutError.Reset();

    bool bExisting = false;
    FName ExistingKind = NAME_None;
    FString ExistingState;
    int64 ExistingRevision = 0;

    if (!Store.TryReadEntity(
            ScenarioCheckpointId(),
            bExisting,
            ExistingKind,
            ExistingState,
            ExistingRevision,
            OutError))
    {
        return false;
    }

    if (bExisting)
    {
        OutError =
            TEXT("Vertical-slice scenario already exists in this world store.");
        return false;
    }

    const FOGEntityId RulerId =
        ScenarioRulerId();
    const FOGEntityId LocationId =
        ScenarioLocationId();
    const FOGEntityId TerritoryId =
        ScenarioTerritoryId();
    const FOGEntityId CoreId =
        ScenarioCoreId();

    if (!Store.UpsertEntity(
            RulerId,
            TEXT("ruler"),
            0,
            TEXT("{}"),
            OutError))
    {
        return false;
    }

    FOGLocationRecord Location;
    Location.LocationId = LocationId;
    Location.Kind = TEXT("wilderness");
    if (!Store.UpsertLocation(
            Location,
            0,
            OutError))
    {
        return false;
    }

    if (!Store.SetResourceBalance(
            RulerId,
            PullCurrencyId(),
            500,
            OutError) ||
        !Store.SetResourceBalance(
            RulerId,
            ProjectResourceId(),
            100,
            OutError))
    {
        return false;
    }

    FOGGachaBannerDefinition Banner;
    Banner.BannerId =
        FOGContentId(TEXT("slice:banner.proof"));
    Banner.PityCategory =
        FName(TEXT("slice_proof"));
    Banner.CurrencyId =
        PullCurrencyId();
    Banner.PullCost = 100;
    Banner.TopRarity =
        FName(TEXT("proof"));
    Banner.SoftPityStart = 0;
    Banner.HardPity = 1;

    FOGGachaPoolEntry Entry;
    Entry.IdentityId =
        AcquiredIdentityId();
    Entry.VersionId =
        AcquiredVersionId();
    Entry.Rarity =
        FName(TEXT("proof"));
    Entry.Weight = 1;
    Entry.bFeatured = true;
    Banner.Entries.Add(Entry);

    FOGGachaService Gacha(Store);
    FOGGachaPullResult Pull;
    if (!Gacha.Pull(
            Banner,
            RulerId,
            10,
            0x5A17,
            Pull,
            OutError))
    {
        return false;
    }

    OutResult.ManifestationId =
        Pull.ManifestationId;

    FOGCombatUnitState RulerUnit =
        MakeUnit(
            RulerId,
            FOGContentId(
                TEXT("slice:ruler.protagonist")),
            0,
            0);

    FOGCombatUnitState AcquiredUnit =
        MakeUnit(
            Pull.ManifestationId,
            Pull.IdentityId,
            0,
            100);

    if (!FOGActionCombatAdapter::ValidateSwitchParty(
            {RulerUnit, AcquiredUnit},
            OutError))
    {
        return false;
    }

    FOGSharedWorldStateService SharedWorld(
        Store);

    FOGWorldPresenceRecord Presence;
    Presence.EntityId = RulerId;
    Presence.LocationId = LocationId;
    Presence.LocalPosition =
        FVector3d(125.0, 250.0, 12.0);
    Presence.MovementContext =
        FName(TEXT("ground"));
    Presence.UpdatedWorldTick = 20;

    if (!SharedWorld.UpdatePhysicalPresence(
            Presence,
            OutError) ||
        !SharedWorld.RecordLocationDiscovery(
            RulerId,
            LocationId,
            EOGLocationKnowledgeLevel::Explored,
            25,
            OutError))
    {
        return false;
    }

    FOGTerritoryRecord Territory;
    Territory.TerritoryId = TerritoryId;
    Territory.RulerId = RulerId;
    Territory.RootLocationId = LocationId;
    Territory.bMainTerritory = true;
    Territory.Population = 1;
    Territory.ControlState =
        FName(TEXT("controlled"));

    if (!Store.UpsertTerritory(
            Territory,
            30,
            OutError))
    {
        return false;
    }

    FOGDomainCoreRecord Core;
    Core.CoreId = CoreId;
    Core.TerritoryId = TerritoryId;
    Core.ControllerRulerId = RulerId;
    Core.Lifecycle =
        EOGDomainCoreLifecycle::Awakened;
    Core.CurrentDurability =
        FOGLargeNumber::FromInt64(1000);
    Core.MaxDurability =
        FOGLargeNumber::FromInt64(1000);

    if (!Store.UpsertDomainCore(
            Core,
            30,
            OutError))
    {
        return false;
    }

    FOGTerritoryProjectService TerritoryService(
        Store);

    if (!TerritoryService.ApplyCoreDurabilityDamage(
            CoreId,
            FOGLargeNumber::FromInt64(100),
            40,
            OutError))
    {
        return false;
    }

    FOGProjectResourceCost Cost;
    Cost.ResourceId = ProjectResourceId();
    Cost.Amount = 25;

    if (!TerritoryService.StartProject(
            RulerId,
            LocationId,
            FOGContentId(
                TEXT("slice:project.repair")),
            {Cost},
            50,
            80,
            TEXT("{\"proof\":true}"),
            OutResult.ProjectId,
            OutError))
    {
        return false;
    }

    FOGProjectRecord CompletedProject;
    if (!TerritoryService.RefreshProject(
            OutResult.ProjectId,
            80,
            CompletedProject,
            OutError) ||
        CompletedProject.Status !=
            EOGProjectStatus::Completed)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Vertical-slice project did not complete.");
        }
        return false;
    }

    const FOGEntityId EnemyId =
        FixedId(6);
    FOGCombatUnitState EnemyUnit =
        MakeUnit(
            EnemyId,
            FOGContentId(
                TEXT("slice:enemy.proof")),
            1,
            100);

    FOGTurnBattleState BattleState;
    BattleState.BattleId =
        FixedId(7);
    BattleState.Teams =
    {
        MakeTeam(0, RulerUnit.UnitEntityId),
        MakeTeam(1, EnemyUnit.UnitEntityId)
    };
    BattleState.Units =
    {
        RulerUnit,
        EnemyUnit
    };

    TArray<FOGReplayActionCommand> Commands;
    for (int32 Round = 0; Round < 3; ++Round)
    {
        FOGReplayActionCommand PlayerAction;
        PlayerAction.SourceUnitId =
            RulerUnit.UnitEntityId;
        PlayerAction.TargetUnitId =
            EnemyUnit.UnitEntityId;
        PlayerAction.SkillId =
            FOGContentId(
                TEXT("slice:skill.proof_player"));
        PlayerAction.SkillMultiplierBps = 5000;
        PlayerAction.bCanCrit = false;
        PlayerAction.bCanBeBlocked = false;
        PlayerAction.ActionDelay = 200;
        Commands.Add(PlayerAction);

        FOGReplayActionCommand EnemyAction;
        EnemyAction.SourceUnitId =
            EnemyUnit.UnitEntityId;
        EnemyAction.TargetUnitId =
            RulerUnit.UnitEntityId;
        EnemyAction.SkillId =
            FOGContentId(
                TEXT("slice:skill.proof_enemy"));
        EnemyAction.SkillMultiplierBps = 5000;
        EnemyAction.bCanCrit = false;
        EnemyAction.bCanBeBlocked = false;
        EnemyAction.ActionDelay = 200;
        Commands.Add(EnemyAction);
    }

    const FOGReplayResult Replay =
        FOGBattleReplay::Run(
            BattleState,
            0xA57E210ull,
            Commands);

    if (!Replay.bSucceeded)
    {
        OutError =
            FString::Printf(
                TEXT("Turn-battle replay failed: %s"),
                *Replay.Error);
        return false;
    }

    OutResult.BattleFingerprint =
        Replay.DeterministicFingerprint;

    if (!WriteCheckpoint(
            Store,
            OutResult,
            100,
            OutError))
    {
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("vertical_slice.completed"));
    Event.WorldTick = 100;
    Event.PrimaryEntity = RulerId;
    Event.RelatedEntities.Add(LocationId);
    Event.RelatedEntities.Add(CoreId);
    Event.RelatedEntities.Add(
        OutResult.ManifestationId);
    Event.bChronicleEligible = false;
    Event.PayloadJson =
        TEXT("{\"checkpoint\":\"g2_runtime_harness\"}");

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        return false;
    }

    OutResult.bSucceeded = true;
    return true;
}

bool FOGVerticalSliceScenarioHarness::VerifyAfterRestart(
    IOGWorldStore& Store,
    FOGVerticalSliceScenarioResult& OutResult,
    FString& OutError)
{
    OutResult =
        FOGVerticalSliceScenarioResult();
    OutError.Reset();

    if (!ReadCheckpoint(
            Store,
            OutResult,
            OutError))
    {
        return false;
    }

    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryFindCharacterManifestationByOwnerAndIdentity(
            ScenarioRulerId(),
            AcquiredIdentityId(),
            bFound,
            Manifestation,
            OutError) ||
        !bFound ||
        Manifestation.ManifestationId !=
            OutResult.ManifestationId)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Acquired Manifestation did not survive restart.");
        }
        return false;
    }

    FOGGachaStateRecord GachaState;
    if (!Store.TryReadGachaState(
            ScenarioRulerId(),
            FName(TEXT("slice_proof")),
            bFound,
            GachaState,
            OutError) ||
        !bFound ||
        GachaState.TotalPulls != 1)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Persistent gacha state did not survive restart.");
        }
        return false;
    }

    FOGSharedWorldStateService SharedWorld(
        Store);

    EOGLocationKnowledgeLevel Knowledge =
        EOGLocationKnowledgeLevel::Rumored;
    if (!SharedWorld.TryReadLocationKnowledge(
            ScenarioRulerId(),
            ScenarioLocationId(),
            bFound,
            Knowledge,
            OutError) ||
        !bFound ||
        Knowledge !=
            EOGLocationKnowledgeLevel::Explored)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("World discovery did not survive restart.");
        }
        return false;
    }

    FOGWorldPresenceRecord Presence;
    if (!SharedWorld.TryReadPhysicalPresence(
            ScenarioRulerId(),
            bFound,
            Presence,
            OutError) ||
        !bFound ||
        Presence.LocationId !=
            ScenarioLocationId())
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Physical presence did not survive restart.");
        }
        return false;
    }

    FOGDomainCoreRecord Core;
    if (!Store.TryReadDomainCore(
            ScenarioCoreId(),
            bFound,
            Core,
            OutError) ||
        !bFound ||
        FOGLargeNumber::Compare(
            Core.CurrentDurability,
            FOGLargeNumber::FromInt64(900)) != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Persistent World Mode consequence did not survive restart.");
        }
        return false;
    }

    FOGProjectRecord Project;
    if (!Store.TryReadProject(
            OutResult.ProjectId,
            bFound,
            Project,
            OutError) ||
        !bFound ||
        Project.Status !=
            EOGProjectStatus::Completed ||
        Project.ProgressBps != 10000)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Project completion did not survive restart.");
        }
        return false;
    }

    bool bResourceKnown = false;
    int64 ResourceBalance = 0;
    if (!Store.TryReadResourceBalance(
            ScenarioRulerId(),
            ProjectResourceId(),
            bResourceKnown,
            ResourceBalance,
            OutError) ||
        !bResourceKnown ||
        ResourceBalance != 75)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Project resource consequence did not survive restart.");
        }
        return false;
    }

    if (OutResult.BattleFingerprint.IsEmpty())
    {
        OutError =
            TEXT("Turn-battle proof fingerprint was not persisted.");
        return false;
    }

    OutResult.bSucceeded = true;
    return true;
}
