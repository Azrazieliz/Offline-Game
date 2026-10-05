#include "Runtime/OGVerticalSliceScenario.h"

#include "Combat/OGActionCombatAdapter.h"
#include "Combat/OGBattleReplay.h"
#include "Combat/OGCombatMath.h"
#include "Content/OGContentManifest.h"
#include "Dom/JsonObject.h"
#include "Gacha/OGGachaService.h"
#include "Gacha/OGRulerGachaAccessService.h"
#include "Random/OGDeterministicRng.h"
#include "Runtime/OGPackageManagerService.h"
#include "Runtime/OGReportService.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UI/OGUiViewModelService.h"
#include "World/OGDispatchService.h"
#include "World/OGDomainCoreService.h"
#include "World/OGFactionWarService.h"
#include "World/OGProjectService.h"
#include "World/OGSharedWorldStateService.h"
#include "World/OGTerritoryControlService.h"
#include "World/OGWorldTimeService.h"

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

FOGContentId ProtagonistIdentityId()
{
    return FOGContentId(
        TEXT("slice:character.protagonist"));
}

FOGContentId WorldAllyIdentityId()
{
    return FOGContentId(
        TEXT("slice:character.world_ally"));
}

FOGContentId EnemyFamilyIdentityId()
{
    return FOGContentId(
        TEXT("slice:enemy.family"));
}

FOGContentId BossIdentityId()
{
    return FOGContentId(
        TEXT("slice:enemy.boss"));
}

FOGEntityId ScenarioClaimId()
{
    return FixedId(8);
}

FOGEntityId ScenarioTimeDomainId()
{
    return FixedId(9);
}

FOGEntityId ScenarioWorldAllyId()
{
    return FixedId(10);
}

FOGEntityId ScenarioBossId()
{
    return FixedId(11);
}

FOGEntityId ScenarioEnemyFamilyMemberId()
{
    return FixedId(12);
}

FOGEntityId ScenarioRulerFactionId()
{
    return FixedId(13);
}

FOGEntityId ScenarioEnemyFactionId()
{
    return FixedId(14);
}

FOGEntityId ScenarioTurnBattleId()
{
    return FixedId(15);
}

FOGContentId DispatchObjectiveId()
{
    return FOGContentId(
        TEXT("slice:dispatch_objective.protect_core"));
}

FOGContentId DispatchConstraintId()
{
    return FOGContentId(
        TEXT("slice:dispatch_constraint.preserve_heart"));
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
        FOGLargeNumber::FromInt64(400);
    Unit.Stats.Defense =
        FOGLargeNumber::FromInt64(100);
    Unit.Stats.HitBps = 10000;
    Unit.NextActionValue = NextActionValue;
    Unit.DefaultActionDelay = 200;
    return Unit;
}

FOGTurnTeamState MakeTeam(
    int32 TeamIndex,
    const TArray<FOGEntityId>& UnitIds)
{
    FOGTurnTeamState Team;
    Team.TeamIndex = TeamIndex;

    for (int32 Index = 0;
         Index < UnitIds.Num();
         ++Index)
    {
        FOGTurnSuccessionLane Lane;
        Lane.LaneIndex = Index;
        Lane.OrderedUnitIds.Add(
            UnitIds[Index]);
        Team.Lanes.Add(
            MoveTemp(Lane));
    }

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

void SetCheckpointEntity(
    const TSharedRef<FJsonObject>& Json,
    const TCHAR* Field,
    const FOGEntityId& Id)
{
    Json->SetStringField(
        Field,
        Id.ToString());
}

bool ReadCheckpointEntity(
    const TSharedPtr<FJsonObject>& Json,
    const TCHAR* Field,
    FOGEntityId& OutId)
{
    FString Value;
    return Json.IsValid() &&
        Json->TryGetStringField(
            Field,
            Value) &&
        ParseEntityId(
            Value,
            OutId);
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
        Kind !=
            FName(TEXT("vertical_slice_checkpoint")))
    {
        OutError =
            TEXT("Reconciled Vertical Slice 0 checkpoint is missing.");
        return false;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(
            StateJson);

    if (!FJsonSerializer::Deserialize(
            Reader,
            Json) ||
        !Json.IsValid())
    {
        OutError =
            TEXT("Vertical Slice 0 checkpoint JSON is invalid.");
        return false;
    }

    FString Stage;
    if (!Json->TryGetStringField(
            TEXT("stage"),
            Stage) ||
        Stage != TEXT("completed") ||
        !Json->TryGetStringField(
            TEXT("turn_battle_fingerprint"),
            OutResult.TurnBattleFingerprint) ||
        !Json->TryGetStringField(
            TEXT("action_damage_display"),
            OutResult.ActionDamageDisplay) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("first_manifestation_id"),
            OutResult.FirstManifestationId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("second_manifestation_id"),
            OutResult.SecondManifestationId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("project_id"),
            OutResult.ProjectId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("dispatch_id"),
            OutResult.DispatchId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("war_id"),
            OutResult.WarId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("war_front_id"),
            OutResult.WarFrontId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("action_combat_event_id"),
            OutResult.ActionCombatEventId) ||
        !ReadCheckpointEntity(
            Json,
            TEXT("report_id"),
            OutResult.ReportId))
    {
        OutError =
            TEXT("Vertical Slice 0 checkpoint is incomplete.");
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

    SetCheckpointEntity(
        Json,
        TEXT("first_manifestation_id"),
        Result.FirstManifestationId);
    SetCheckpointEntity(
        Json,
        TEXT("second_manifestation_id"),
        Result.SecondManifestationId);
    SetCheckpointEntity(
        Json,
        TEXT("project_id"),
        Result.ProjectId);
    SetCheckpointEntity(
        Json,
        TEXT("dispatch_id"),
        Result.DispatchId);
    SetCheckpointEntity(
        Json,
        TEXT("war_id"),
        Result.WarId);
    SetCheckpointEntity(
        Json,
        TEXT("war_front_id"),
        Result.WarFrontId);
    SetCheckpointEntity(
        Json,
        TEXT("action_combat_event_id"),
        Result.ActionCombatEventId);
    SetCheckpointEntity(
        Json,
        TEXT("report_id"),
        Result.ReportId);

    Json->SetStringField(
        TEXT("turn_battle_fingerprint"),
        Result.TurnBattleFingerprint);
    Json->SetStringField(
        TEXT("action_damage_display"),
        Result.ActionDamageDisplay);
    Json->SetStringField(
        TEXT("stage"),
        TEXT("completed"));
    Json->SetStringField(
        TEXT("contract"),
        TEXT("reconciled_vertical_slice_0"));

    FString StateJson;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &StateJson);
    if (!FJsonSerializer::Serialize(
            Json,
            Writer))
    {
        OutError =
            TEXT("Failed to serialize Vertical Slice 0 checkpoint.");
        return false;
    }

    return Store.UpsertEntity(
        FOGVerticalSliceScenarioHarness::ScenarioCheckpointId(),
        FName(TEXT("vertical_slice_checkpoint")),
        WorldTick,
        StateJson,
        OutError);
}

bool ValidateAndActivateSlicePackage(
    IOGWorldStore& Store,
    FString& OutError)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId =
        FOGVerticalSliceScenarioHarness::ScenarioPackageId();
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId =
        FOGVerticalSliceScenarioHarness::AcquiredIdentityId();
    Identity.DisplayNameKey =
        TEXT("slice.character.acquired");
    Identity.CanonicalMaturity =
        EOGCanonicalMaturity::Unknown;
    Identity.OriginRarity =
        FName(TEXT("proof"));
    Manifest.CharacterIdentities.Add(
        Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId =
        FOGVerticalSliceScenarioHarness::AcquiredVersionId();
    Version.IdentityId =
        Identity.IdentityId;
    Version.VersionKind =
        FName(TEXT("base"));
    Manifest.CharacterVersions.Add(
        Version);

    TMap<FOGContentId, EOGCanonicalMaturity> KnownExternalIdentities;
    TArray<FString> ValidationErrors;
    if (!FOGContentManifestValidator::Validate(
            Manifest,
            KnownExternalIdentities,
            ValidationErrors))
    {
        OutError =
            FString::Printf(
                TEXT("Vertical Slice 0 content package failed validation: %s"),
                *FString::Join(
                    ValidationErrors,
                    TEXT(" | ")));
        return false;
    }

    FOGContentPackageRecord Package;
    Package.PackageId =
        Manifest.PackageId;
    Package.Version =
        Manifest.Version;
    Package.ContentHash =
        TEXT("slice-proof-package-v1");
    Package.bInstalled = true;
    Package.bValidated = true;
    Package.Category =
        FName(TEXT("vertical_slice"));
    Package.StorageClass =
        FName(TEXT("local_hot"));
    Package.SealedState =
        FName(TEXT("visible"));
    Package.DownloadState =
        FName(TEXT("installed"));
    Package.CompatibilityJson =
        TEXT("{\"schema_min\":14}");
    Package.ManifestJson =
        TEXT("{\"purpose\":\"reconciled_vertical_slice_0\"}");

    FOGPackageManagerService Packages(
        Store);
    return Packages.RegisterPackage(
               Package,
               OutError) &&
        Packages.ActivatePackage(
            Package.PackageId,
            OutError);
}

bool RequireUnanchored(
    IOGWorldStore& Store,
    const FOGEntityId& ManifestationId,
    FString& OutError)
{
    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Expected acquired Manifestation is missing.");
        return false;
    }

    if (Manifestation.WorldModeAnchorTerritoryId.IsValid())
    {
        OutError =
            TEXT("Landless pull incorrectly received a World Mode anchor.");
        return false;
    }

    return true;
}

bool RequireRulerModeAcquisitionProjection(
    IOGWorldStore& Store,
    const FOGEntityId& RulerId,
    int32 ExpectedCopyCount,
    const FOGEntityId& ExpectedSelectedManifestationId,
    FString& OutError)
{
    FOGUiViewModelService Ui(
        Store);

    TArray<FOGRosterIdentityViewModel> Roster;
    if (!Ui.BuildRoster(
            RulerId,
            Roster,
            OutError))
    {
        return false;
    }

    const FOGRosterIdentityViewModel* Card =
        Roster.FindByPredicate(
            [](const FOGRosterIdentityViewModel& Candidate)
            {
                return Candidate.IdentityId ==
                    FOGVerticalSliceScenarioHarness::AcquiredIdentityId();
            });

    if (!Card ||
        Card->ManifestationCount !=
            ExpectedCopyCount ||
        (ExpectedSelectedManifestationId.IsValid() &&
         Card->SelectedManifestationId !=
            ExpectedSelectedManifestationId))
    {
        OutError =
            TEXT("Portrait Ruler Mode did not immediately project the acquired Manifestation state.");
        return false;
    }

    return true;
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

FOGContentId FOGVerticalSliceScenarioHarness::ScenarioPackageId()
{
    return FOGContentId(
        TEXT("slice:package.reconciled"));
}

FOGContentId FOGVerticalSliceScenarioHarness::AcquiredIdentityId()
{
    return FOGContentId(
        TEXT("slice:character.acquired"));
}

FOGContentId FOGVerticalSliceScenarioHarness::AcquiredVersionId()
{
    return FOGContentId(
        TEXT("slice:character.acquired.base"));
}

bool FOGVerticalSliceScenarioHarness::RunFresh(
    IOGWorldStore& Store,
    FOGVerticalSliceScenarioResult& OutResult,
    FString& OutError)
{
    OutResult =
        FOGVerticalSliceScenarioResult();
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
            TEXT("Vertical Slice 0 already exists in this world store.");
        return false;
    }

    if (!ValidateAndActivateSlicePackage(
            Store,
            OutError))
    {
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
    const FOGEntityId ClaimId =
        ScenarioClaimId();
    const FOGEntityId TimeDomainId =
        ScenarioTimeDomainId();
    const FOGEntityId WorldAllyId =
        ScenarioWorldAllyId();
    const FOGEntityId BossId =
        ScenarioBossId();
    const FOGEntityId EnemyFamilyMemberId =
        ScenarioEnemyFamilyMemberId();

    if (!Store.UpsertEntity(
            RulerId,
            FName(TEXT("ruler")),
            0,
            TEXT("{\"persistent_protagonist\":true}"),
            OutError) ||
        !Store.UpsertEntity(
            WorldAllyId,
            FName(TEXT("world_ally")),
            0,
            TEXT("{\"source\":\"world_born\"}"),
            OutError) ||
        !Store.UpsertEntity(
            BossId,
            FName(TEXT("boss")),
            0,
            TEXT("{\"condition\":\"healthy\"}"),
            OutError) ||
        !Store.UpsertEntity(
            EnemyFamilyMemberId,
            FName(TEXT("enemy")),
            0,
            TEXT("{\"family\":\"slice_raider\"}"),
            OutError))
    {
        return false;
    }

    FOGLocationRecord Location;
    Location.LocationId =
        LocationId;
    Location.Kind =
        FName(TEXT("starting_region"));
    if (!Store.UpsertLocation(
            Location,
            0,
            OutError))
    {
        return false;
    }

    FOGTerritoryRecord Territory;
    Territory.TerritoryId =
        TerritoryId;
    Territory.RulerId =
        RulerId;
    Territory.RootLocationId =
        LocationId;
    Territory.bMainTerritory =
        true;
    Territory.Population =
        1;
    Territory.ControlState =
        FName(TEXT("controlled"));
    if (!Store.UpsertTerritory(
            Territory,
            0,
            OutError))
    {
        return false;
    }

    FOGLocationTerritoryRecord LocationTerritory;
    LocationTerritory.LocationId =
        LocationId;
    LocationTerritory.TerritoryId =
        TerritoryId;
    LocationTerritory.RelationKind =
        FName(TEXT("root"));
    LocationTerritory.CoverageBps =
        10000;
    if (!Store.UpsertLocationTerritory(
            LocationTerritory,
            OutError))
    {
        return false;
    }

    FOGTerritoryClaimRecord Claim;
    Claim.ClaimId =
        ClaimId;
    Claim.TerritoryId =
        TerritoryId;
    Claim.RulerId =
        RulerId;
    Claim.ClaimKind =
        FName(TEXT("control"));
    Claim.ControlState =
        FName(TEXT("controlled"));
    Claim.ControlStrengthBps =
        10000;
    Claim.ClaimStartWorldTick =
        0;
    Claim.bHasEffectiveControlStart =
        true;
    Claim.EffectiveControlStartWorldTick =
        0;
    Claim.UpdatedWorldTick =
        0;
    if (!Store.UpsertTerritoryClaim(
            Claim,
            0,
            OutError))
    {
        return false;
    }

    FOGTimeDomainRecord TimeDomain;
    TimeDomain.TimeDomainId =
        TimeDomainId;
    TimeDomain.RateNumerator = 1;
    TimeDomain.RateDenominator = 1;
    TimeDomain.CalendarId =
        FOGContentId(
            TEXT("slice:calendar.starting_world"));

    FOGWorldTimeService WorldTime(
        Store);
    if (!WorldTime.SaveTimeDomain(
            TimeDomain,
            0,
            OutError))
    {
        return false;
    }

    // Harness-authored calendar semantics only. Production month length is
    // content-owned and never a runtime constant.
    const FOGCalendarElapsedResolver CalendarResolver =
        [](const FOGCalendarElapsedQuery& Query,
           bool& bOutElapsed,
           FString& Error)
        {
            Error.Reset();

            if (Query.DurationKind !=
                    FName(TEXT("month")) ||
                Query.DurationCount != 1)
            {
                Error =
                    TEXT("Vertical Slice 0 calendar received an unexpected duration query.");
                return false;
            }

            const int64 AuthoredSliceMonthTicks =
                8;
            const int64 Elapsed =
                Query.EndLocalTick -
                Query.StartLocalTick;
            bOutElapsed =
                Query.bStrictlyMoreThan
                    ? Elapsed >
                        AuthoredSliceMonthTicks
                    : Elapsed >=
                        AuthoredSliceMonthTicks;
            return true;
        };

    FOGRulerGachaAccessService GachaAccess(
        Store);
    FOGRulerGachaAccessRecord GachaAccessState;
    if (!GachaAccess.RefreshGachaQualification(
            RulerId,
            0,
            TimeDomainId,
            CalendarResolver,
            GachaAccessState,
            OutError) ||
        !GachaAccess.RefreshGachaQualification(
            RulerId,
            9,
            TimeDomainId,
            CalendarResolver,
            GachaAccessState,
            OutError) ||
        !GachaAccessState.bPermanentlyUnlocked)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Vertical Slice 0 failed the authored one-month sovereignty qualification.");
        }
        return false;
    }

    // Permanent access must survive later landlessness. Displace the only
    // holding before pulling so the acquisition is correctly Ruler-visible but
    // remains World-Mode-unanchored until control is reached again.
    FOGTerritoryControlService TerritoryControl(
        Store);
    if (!TerritoryControl.BeginDisplacement(
            ClaimId,
            10,
            16,
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
        FOGContentId(
            TEXT("slice:banner.proof"));
    Banner.PityCategory =
        FName(TEXT("slice_proof"));
    Banner.CurrencyId =
        PullCurrencyId();
    Banner.PullCost =
        100;
    Banner.TopRarity =
        FName(TEXT("proof"));
    Banner.SoftPityStart =
        0;
    Banner.HardPity =
        1;

    FOGGachaPoolEntry Entry;
    Entry.IdentityId =
        AcquiredIdentityId();
    Entry.VersionId =
        AcquiredVersionId();
    Entry.Rarity =
        FName(TEXT("proof"));
    Entry.Weight =
        1;
    Entry.bFeatured =
        true;
    Banner.Entries.Add(
        Entry);

    FOGGachaService Gacha(
        Store);

    FOGGachaPullResult FirstPull;
    if (!Gacha.Pull(
            Banner,
            RulerId,
            11,
            0x5A17,
            FirstPull,
            OutError))
    {
        return false;
    }
    OutResult.FirstManifestationId =
        FirstPull.ManifestationId;

    if (!RequireUnanchored(
            Store,
            OutResult.FirstManifestationId,
            OutError) ||
        !RequireRulerModeAcquisitionProjection(
            Store,
            RulerId,
            1,
            OutResult.FirstManifestationId,
            OutError))
    {
        return false;
    }

    FOGGachaPullResult SecondPull;
    if (!Gacha.Pull(
            Banner,
            RulerId,
            12,
            0x5A18,
            SecondPull,
            OutError))
    {
        return false;
    }
    OutResult.SecondManifestationId =
        SecondPull.ManifestationId;

    if (OutResult.FirstManifestationId ==
            OutResult.SecondManifestationId ||
        !SecondPull.bDuplicateIdentity ||
        !RequireUnanchored(
            Store,
            OutResult.SecondManifestationId,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Repeated Identity acquisition did not create a distinct unanchored Manifestation.");
        }
        return false;
    }

    FOGManifestationContextSelectionRecord LastUsed;
    LastUsed.OwnerEntityId =
        RulerId;
    LastUsed.ContextId =
        FOGUiViewModelService::CharacterLastUsedContextId(
            AcquiredIdentityId());
    LastUsed.ManifestationId =
        OutResult.SecondManifestationId;
    LastUsed.UpdatedWorldTick =
        12;
    if (!Store.UpsertManifestationContextSelection(
            LastUsed,
            OutError) ||
        !RequireRulerModeAcquisitionProjection(
            Store,
            RulerId,
            2,
            OutResult.SecondManifestationId,
            OutError))
    {
        return false;
    }

    // Reach controlled Territory again and anchor both independent
    // Manifestations. Reclamation continuity and anchoring are separate facts.
    if (!TerritoryControl.ReclaimTerritory(
            ClaimId,
            13,
            OutError))
    {
        return false;
    }

    FOGEntityId PreferredTerritory;
    if (!TerritoryControl.FindPreferredEffectiveTerritoryForRuler(
            RulerId,
            13,
            PreferredTerritory,
            OutError) ||
        PreferredTerritory !=
            TerritoryId)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Reclaimed Territory was not available for World Mode anchoring.");
        }
        return false;
    }

    FOGEntityId FirstAnchorTerritory;
    FOGEntityId SecondAnchorTerritory;
    if (!TerritoryControl.AnchorManifestationForWorldMode(
            RulerId,
            OutResult.FirstManifestationId,
            13,
            FirstAnchorTerritory,
            OutError) ||
        !TerritoryControl.AnchorManifestationForWorldMode(
            RulerId,
            OutResult.SecondManifestationId,
            13,
            SecondAnchorTerritory,
            OutError) ||
        FirstAnchorTerritory !=
            PreferredTerritory ||
        SecondAnchorTerritory !=
            PreferredTerritory)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Explicit World Mode roster anchoring did not use controlled Territory.");
        }
        return false;
    }

    FOGCombatUnitState RulerUnit =
        MakeUnit(
            RulerId,
            ProtagonistIdentityId(),
            0,
            0);
    FOGCombatUnitState AcquiredUnit =
        MakeUnit(
            OutResult.FirstManifestationId,
            AcquiredIdentityId(),
            0,
            100);
    FOGCombatUnitState DuplicateAcquiredUnit =
        MakeUnit(
            OutResult.SecondManifestationId,
            AcquiredIdentityId(),
            0,
            150);
    FOGCombatUnitState WorldAllyUnit =
        MakeUnit(
            WorldAllyId,
            WorldAllyIdentityId(),
            0,
            200);

    FString DuplicatePartyError;
    if (FOGActionCombatAdapter::ValidateSwitchParty(
            {
                RulerUnit,
                AcquiredUnit,
                DuplicateAcquiredUnit
            },
            DuplicatePartyError))
    {
        OutError =
            TEXT("Action combat failed to enforce local Character-Identity exclusivity.");
        return false;
    }

    if (!FOGActionCombatAdapter::ValidateSwitchParty(
            {
                RulerUnit,
                AcquiredUnit,
                WorldAllyUnit
            },
            OutError))
    {
        return false;
    }

    FOGSharedWorldStateService SharedWorld(
        Store);
    FOGWorldPresenceRecord Presence;
    Presence.EntityId =
        RulerId;
    Presence.LocationId =
        LocationId;
    Presence.LocalPosition =
        FVector3d(
            125.0,
            250.0,
            12.0);
    Presence.MovementContext =
        FName(TEXT("ground"));
    Presence.UpdatedWorldTick =
        20;

    if (!SharedWorld.UpdatePhysicalPresence(
            Presence,
            OutError) ||
        !SharedWorld.RecordLocationDiscovery(
            RulerId,
            LocationId,
            EOGLocationKnowledgeLevel::Explored,
            21,
            OutError))
    {
        return false;
    }

    FOGUiViewModelService Ui(
        Store);
    FOGWorldHudViewModel WorldHud;
    if (!Ui.BuildWorldHud(
            RulerId,
            RulerId,
            {
                OutResult.FirstManifestationId,
                WorldAllyId
            },
            {
                WorldAllyId
            },
            nullptr,
            false,
            WorldHud,
            OutError) ||
        !WorldHud.bShowMinimap ||
        WorldHud.CompanionPlacement !=
            FName(TEXT("upper_right")) ||
        WorldHud.Companions.Num() != 2 ||
        !WorldHud.Companions[1].bQteReady)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Landscape World Mode HUD/switch-QTE companion projection violated the frozen contract.");
        }
        return false;
    }

    // Action-combat proof consumes the same AcquiredUnit combat state later used
    // by the turn executor. The action layer owns timing/motion, while shared
    // deterministic combat math owns this resolved hit.
    FOGCombatUnitState BossUnit =
        MakeUnit(
            BossId,
            BossIdentityId(),
            1,
            0);
    BossUnit.Stats.Defense =
        FOGLargeNumber::FromInt64(
            250);

    FOGDamageRequest ActionRequest;
    ActionRequest.BaseDamage =
        FOGLargeNumber::ScaleByBasisPoints(
            AcquiredUnit.Stats.Attack,
            25000);
    ActionRequest.DefenseReference =
        AcquiredUnit.Stats.Attack;
    ActionRequest.bCanCrit =
        false;
    ActionRequest.bCanBeBlocked =
        false;
    ActionRequest.bAllowHitOverflowReplication =
        false;

    FOGDeterministicRng ActionRng(
        0xAC7100ull);
    const FOGDamageResolution ActionDamage =
        FOGCombatMath::ResolveDamage(
            ActionRequest,
            AcquiredUnit.Stats,
            BossUnit.Stats,
            ActionRng);

    if (FOGLargeNumber::Compare(
            ActionDamage.TotalDamage,
            FOGLargeNumber()) <= 0)
    {
        OutError =
            TEXT("Action-combat proof produced no meaningful damage.");
        return false;
    }

    OutResult.ActionDamageDisplay =
        FOGUiNumberFormatter::Format(
            ActionDamage.TotalDamage,
            true,
            true);

    if (OutResult.ActionDamageDisplay.IsEmpty() ||
        OutResult.ActionDamageDisplay.Contains(
            TEXT("10^")))
    {
        OutError =
            TEXT("Action-combat player-facing damage presentation is invalid.");
        return false;
    }

    if (!Store.UpsertEntity(
            BossId,
            FName(TEXT("boss")),
            22,
            TEXT("{\"condition\":\"wounded\",\"persistent_action_consequence\":true}"),
            OutError))
    {
        return false;
    }

    FOGWorldEvent ActionEvent;
    ActionEvent.EventId =
        FOGEntityId::NewId();
    ActionEvent.EventType =
        FName(TEXT("action_combat.boss_wounded"));
    ActionEvent.WorldTick =
        22;
    ActionEvent.PrimaryEntity =
        OutResult.FirstManifestationId;
    ActionEvent.RelatedEntities.Add(
        BossId);
    ActionEvent.bChronicleEligible =
        true;
    ActionEvent.PayloadJson =
        FString::Printf(
            TEXT("{\"damage_display\":\"%s\",\"rng_draws\":%llu}"),
            *OutResult.ActionDamageDisplay,
            static_cast<unsigned long long>(
                ActionRng.GetDrawCount()));

    if (!Store.AppendWorldEvent(
            ActionEvent,
            OutError))
    {
        return false;
    }
    OutResult.ActionCombatEventId =
        ActionEvent.EventId;

    FOGKnowledgeFactRecord BossCondition;
    BossCondition.OwnerEntityId =
        RulerId;
    BossCondition.FactKey =
        FName(TEXT("target_condition"));
    BossCondition.SubjectEntityId =
        BossId;
    BossCondition.ValueJson =
        TEXT("{\"condition\":\"wounded\"}");
    BossCondition.LearnedWorldTick =
        22;
    BossCondition.UpdatedWorldTick =
        22;
    BossCondition.BeliefState =
        FName(TEXT("confirmed"));
    BossCondition.ConfidenceBps =
        10000;
    BossCondition.SourceEventId =
        ActionEvent.EventId;
    BossCondition.bHasEvidenceWorldTick =
        true;
    BossCondition.EvidenceWorldTick =
        22;
    if (!Store.UpsertKnowledgeFact(
            BossCondition,
            OutError))
    {
        return false;
    }

    FOGReportService Reports(
        Store);
    if (!Reports.CreateReport(
            RulerId,
            ActionEvent.EventId,
            FName(TEXT("combat")),
            60,
            23,
            TEXT("{\"summary\":\"boss wounded in World Mode\"}"),
            OutResult.ReportId,
            OutError))
    {
        return false;
    }

    if (!Ui.BuildWorldHud(
            RulerId,
            RulerId,
            {
                OutResult.FirstManifestationId,
                WorldAllyId
            },
            {},
            &BossCondition,
            false,
            WorldHud,
            OutError) ||
        !WorldHud.bShowExactEnemyState)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Confirmed World Mode knowledge did not project exact known target state.");
        }
        return false;
    }

    // Domain/Core consequence shares the same canonical Territory state.
    FOGDomainCoreRecord Core;
    Core.CoreId =
        CoreId;
    Core.TerritoryId =
        TerritoryId;
    Core.ControllerRulerId =
        RulerId;
    Core.Lifecycle =
        EOGDomainCoreLifecycle::Awakened;
    Core.CurrentDurability =
        FOGLargeNumber::FromInt64(
            1000);
    Core.MaxDurability =
        FOGLargeNumber::FromInt64(
            1000);
    if (!Store.UpsertDomainCore(
            Core,
            30,
            OutError))
    {
        return false;
    }

    FOGDomainCoreService CoreService(
        Store);
    if (!CoreService.ActivateAwakenedCoreAsHeart(
            CoreId,
            30,
            OutError) ||
        !CoreService.ApplyCoreDurabilityDamage(
            CoreId,
            FOGLargeNumber::FromInt64(
                100),
            40,
            OutError))
    {
        return false;
    }

    FOGProjectResourceCost Cost;
    Cost.ResourceId =
        ProjectResourceId();
    Cost.Amount =
        25;

    FOGProjectService ProjectService(
        Store);
    if (!ProjectService.StartProject(
            RulerId,
            LocationId,
            FOGContentId(
                TEXT("slice:project.repair")),
            {
                Cost
            },
            50,
            80,
            TEXT("{\"proof\":true,\"scope\":\"physical_repair\"}"),
            OutResult.ProjectId,
            OutError))
    {
        return false;
    }

    FOGProjectRecord CompletedProject;
    if (!ProjectService.RefreshProject(
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
                TEXT("Vertical Slice 0 Project did not complete.");
        }
        return false;
    }

    // Objective-driven Dispatch proves the richer policy contract rather than
    // the legacy simple success/failure finalizer.
    FOGDispatchObjectiveRecord DispatchObjective;
    DispatchObjective.ObjectiveId =
        DispatchObjectiveId();
    DispatchObjective.Priority =
        100;
    DispatchObjective.bMandatory =
        true;
    DispatchObjective.TargetEntityId =
        CoreId;
    DispatchObjective.StateJson =
        TEXT("{\"goal\":\"protect_domain_heart\"}");

    FOGDispatchConstraintRecord DispatchConstraint;
    DispatchConstraint.ConstraintId =
        DispatchConstraintId();
    DispatchConstraint.StateJson =
        TEXT("{\"hard\":true}");

    FOGDispatchService DispatchService(
        Store);
    if (!DispatchService.StartDispatch(
            RulerId,
            CoreId,
            EOGDispatchType::Support,
            {
                WorldAllyId
            },
            55,
            65,
            2000,
            3000,
            TEXT("{\"abort_if\":\"heart_irrecoverable\"}"),
            {
                DispatchObjective
            },
            {
                DispatchConstraint
            },
            0xD15A7C,
            OutResult.DispatchId,
            OutError))
    {
        return false;
    }

    FOGDispatchRecord DispatchResult;
    if (!DispatchService.ResolveDispatchWithPolicy(
            OutResult.DispatchId,
            65,
            [](const FOGDispatchResolutionContext& Context,
               FOGDispatchResolutionResult& Resolution,
               FString& Error)
            {
                Error.Reset();

                if (Context.MandatoryObjectives.Num() != 1 ||
                    Context.MandatoryObjectives[0].ObjectiveId !=
                        DispatchObjectiveId() ||
                    Context.HardConstraints.Num() != 1 ||
                    Context.HardConstraints[0].ConstraintId !=
                        DispatchConstraintId())
                {
                    Error =
                        TEXT("Vertical Slice 0 Dispatch resolver did not receive mandatory objective/constraint state.");
                    return false;
                }

                Resolution.bMandatoryObjectivesEvaluated =
                    true;
                Resolution.bHardConstraintsEvaluated =
                    true;
                Resolution.bSurvivalAbortPolicyEvaluated =
                    true;
                Resolution.bSucceeded =
                    true;
                Resolution.OutcomeState =
                    FName(TEXT("objective_complete"));
                Resolution.ResultJson =
                    TEXT("{\"core_protected\":true}");
                return true;
            },
            DispatchResult,
            OutError) ||
        DispatchResult.Status !=
            EOGDispatchStatus::Succeeded ||
        DispatchResult.OutcomeState !=
            FName(TEXT("objective_complete")))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Objective-faithful Dispatch did not resolve correctly.");
        }
        return false;
    }

    // Minimal persistent faction/continuous-war state belongs to the same
    // world; war does not own Territory truth.
    FOGFactionRecord RulerFaction;
    RulerFaction.FactionId =
        ScenarioRulerFactionId();
    RulerFaction.LeaderRulerId =
        RulerId;
    RulerFaction.Kind =
        FName(TEXT("ruler_faction"));
    RulerFaction.Population =
        100;

    FOGFactionRecord EnemyFaction;
    EnemyFaction.FactionId =
        ScenarioEnemyFactionId();
    EnemyFaction.Kind =
        FName(TEXT("raiders"));
    EnemyFaction.Population =
        40;

    if (!Store.UpsertFaction(
            RulerFaction,
            60,
            OutError) ||
        !Store.UpsertFaction(
            EnemyFaction,
            60,
            OutError))
    {
        return false;
    }

    FOGFactionWarService WarService(
        Store);
    if (!WarService.DeclareWar(
            RulerFaction.FactionId,
            EnemyFaction.FactionId,
            FName(TEXT("defend_territory")),
            TerritoryId,
            60,
            OutResult.WarId,
            OutError) ||
        !WarService.CreateWarFront(
            OutResult.WarId,
            LocationId,
            FOGEntityId(),
            61,
            TEXT("{\"front\":\"starting_region\"}"),
            OutResult.WarFrontId,
            OutError))
    {
        return false;
    }

    FOGWarObjectiveRecord WarObjective;
    WarObjective.WarId =
        OutResult.WarId;
    WarObjective.FrontId =
        OutResult.WarFrontId;
    WarObjective.ObjectiveId =
        FOGContentId(
            TEXT("slice:war_objective.hold_starting_region"));
    WarObjective.TargetEntityId =
        TerritoryId;
    WarObjective.Priority =
        100;
    WarObjective.Status =
        FName(TEXT("active"));
    if (!WarService.SetWarObjective(
            WarObjective,
            OutError))
    {
        return false;
    }

    // Turn executor must enforce the same local Identity exclusivity as action
    // combat. First prove the invalid duplicate-copies roster is rejected.
    FOGCombatUnitState TurnEnemy =
        MakeUnit(
            EnemyFamilyMemberId,
            EnemyFamilyIdentityId(),
            1,
            100);

    FOGTurnBattleState InvalidDuplicateBattle;
    InvalidDuplicateBattle.BattleId =
        FixedId(16);
    InvalidDuplicateBattle.Teams =
    {
        MakeTeam(
            0,
            {
                AcquiredUnit.UnitEntityId,
                DuplicateAcquiredUnit.UnitEntityId
            }),
        MakeTeam(
            1,
            {
                TurnEnemy.UnitEntityId
            })
    };
    InvalidDuplicateBattle.Units =
    {
        AcquiredUnit,
        DuplicateAcquiredUnit,
        TurnEnemy
    };

    FOGTurnBattle DuplicateTurnBattle;
    FString DuplicateTurnError;
    if (DuplicateTurnBattle.Initialize(
            InvalidDuplicateBattle,
            DuplicateTurnError))
    {
        OutError =
            TEXT("Turn executor failed the same local Character-Identity exclusivity rule used by action combat.");
        return false;
    }

    // Valid turn encounter reuses the exact AcquiredUnit state from World Mode.
    FOGTurnBattleState BattleState;
    BattleState.BattleId =
        ScenarioTurnBattleId();
    BattleState.Teams =
    {
        MakeTeam(
            0,
            {
                AcquiredUnit.UnitEntityId
            }),
        MakeTeam(
            1,
            {
                TurnEnemy.UnitEntityId
            })
    };
    BattleState.Units =
    {
        AcquiredUnit,
        TurnEnemy
    };

    TArray<FOGReplayActionCommand> Commands;
    for (int32 Round = 0;
         Round < 3;
         ++Round)
    {
        FOGReplayActionCommand PlayerAction;
        PlayerAction.SourceUnitId =
            AcquiredUnit.UnitEntityId;
        PlayerAction.TargetUnitId =
            TurnEnemy.UnitEntityId;
        PlayerAction.SkillId =
            FOGContentId(
                TEXT("slice:skill.shared_identity_attack"));
        PlayerAction.SkillMultiplierBps =
            5000;
        PlayerAction.bCanCrit =
            false;
        PlayerAction.bCanBeBlocked =
            false;
        PlayerAction.ActionDelay =
            200;
        Commands.Add(
            PlayerAction);

        FOGReplayActionCommand EnemyAction;
        EnemyAction.SourceUnitId =
            TurnEnemy.UnitEntityId;
        EnemyAction.TargetUnitId =
            AcquiredUnit.UnitEntityId;
        EnemyAction.SkillId =
            FOGContentId(
                TEXT("slice:skill.enemy_counter"));
        EnemyAction.SkillMultiplierBps =
            3000;
        EnemyAction.bCanCrit =
            false;
        EnemyAction.bCanBeBlocked =
            false;
        EnemyAction.ActionDelay =
            200;
        Commands.Add(
            EnemyAction);
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
    OutResult.TurnBattleFingerprint =
        Replay.DeterministicFingerprint;

    // Return to portrait Ruler Mode and prove it is only another projection of
    // the same canonical world facts.
    FOGRulerShellViewModel Shell;
    FOGTerritoryViewModel TerritoryView;
    FOGRecordsHubViewModel RecordsView;
    TArray<FOGRosterIdentityViewModel> RosterView;

    if (!Ui.BuildRulerShell(
            RulerId,
            Shell,
            OutError) ||
        !Ui.BuildTerritory(
            RulerId,
            TerritoryId,
            TerritoryView,
            OutError) ||
        !Ui.BuildRecordsHub(
            RulerId,
            RecordsView,
            OutError) ||
        !Ui.BuildRoster(
            RulerId,
            RosterView,
            OutError))
    {
        return false;
    }

    const FOGTerritorySummaryViewModel* TerritorySummary =
        TerritoryView.Territories.FindByPredicate(
            [TerritoryId](
                const FOGTerritorySummaryViewModel& Candidate)
            {
                return Candidate.TerritoryId ==
                    TerritoryId;
            });
    const FOGRosterIdentityViewModel* AcquiredRoster =
        RosterView.FindByPredicate(
            [](const FOGRosterIdentityViewModel& Candidate)
            {
                return Candidate.IdentityId ==
                    AcquiredIdentityId();
            });

    if (Shell.PrimaryDestinations.Num() != 5 ||
        !Shell.bGachaUnlocked ||
        !TerritorySummary ||
        TerritorySummary->DomainState !=
            FName(TEXT("damaged")) ||
        RecordsView.ReportCount < 1 ||
        !AcquiredRoster ||
        AcquiredRoster->ManifestationCount != 2)
    {
        OutError =
            TEXT("Ruler Mode did not reflect the same post-World-Mode canonical facts.");
        return false;
    }

    for (const FOGRosterManifestationViewModel& ManifestationView :
         AcquiredRoster->Manifestations)
    {
        if (!ManifestationView.bWorldModeAnchored)
        {
            OutError =
                TEXT("Ruler roster projection lost World Mode anchoring state.");
            return false;
        }
    }

    FString IntegrityReport;
    if (!Store.RunIntegrityCheck(
            IntegrityReport,
            OutError))
    {
        return false;
    }

    if (!WriteCheckpoint(
            Store,
            OutResult,
            100,
            OutError))
    {
        return false;
    }

    FOGWorldEvent CompletionEvent;
    CompletionEvent.EventId =
        FOGEntityId::NewId();
    CompletionEvent.EventType =
        FName(TEXT("vertical_slice.completed"));
    CompletionEvent.WorldTick =
        100;
    CompletionEvent.PrimaryEntity =
        RulerId;
    CompletionEvent.RelatedEntities =
    {
        LocationId,
        CoreId,
        OutResult.FirstManifestationId,
        OutResult.SecondManifestationId,
        OutResult.ProjectId,
        OutResult.DispatchId,
        OutResult.WarId
    };
    CompletionEvent.bChronicleEligible =
        false;
    CompletionEvent.PayloadJson =
        TEXT("{\"checkpoint\":\"reconciled_vertical_slice_0\"}");
    if (!Store.AppendWorldEvent(
            CompletionEvent,
            OutError))
    {
        return false;
    }

    OutResult.bSucceeded =
        true;
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

    bool bPackageKnown = false;
    bool bPackageActivated = false;
    if (!Store.IsContentPackageActivated(
            ScenarioPackageId(),
            bPackageKnown,
            bPackageActivated,
            OutError) ||
        !bPackageKnown ||
        !bPackageActivated)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Validated Vertical Slice 0 package activation did not survive restart.");
        }
        return false;
    }

    TArray<FOGCharacterManifestationRecord> Manifestations;
    if (!Store.ListCharacterManifestationsByOwnerAndIdentity(
            ScenarioRulerId(),
            AcquiredIdentityId(),
            Manifestations,
            OutError) ||
        Manifestations.Num() != 2)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Repeated-acquisition Manifestation set did not survive restart.");
        }
        return false;
    }

    const FOGCharacterManifestationRecord* First =
        Manifestations.FindByPredicate(
            [&OutResult](
                const FOGCharacterManifestationRecord& Candidate)
            {
                return Candidate.ManifestationId ==
                    OutResult.FirstManifestationId;
            });
    const FOGCharacterManifestationRecord* Second =
        Manifestations.FindByPredicate(
            [&OutResult](
                const FOGCharacterManifestationRecord& Candidate)
            {
                return Candidate.ManifestationId ==
                    OutResult.SecondManifestationId;
            });

    if (!First ||
        !Second ||
        First->ManifestationId ==
            Second->ManifestationId ||
        First->WorldModeAnchorTerritoryId !=
            ScenarioTerritoryId() ||
        Second->WorldModeAnchorTerritoryId !=
            ScenarioTerritoryId())
    {
        OutError =
            TEXT("Independent Manifestations or their World Mode anchors did not survive restart.");
        return false;
    }

    FOGRulerGachaAccessRecord Access;
    if (!Store.TryReadRulerGachaAccess(
            ScenarioRulerId(),
            bFound,
            Access,
            OutError) ||
        !bFound ||
        !Access.bPermanentlyUnlocked)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Permanent gacha access did not survive restart.");
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
        GachaState.TotalPulls != 2)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Repeated-pull gacha state did not survive restart.");
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
                TEXT("Physical World Mode presence did not survive restart.");
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
            FOGLargeNumber::FromInt64(
                900)) != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Persistent action/Domain consequence did not survive restart.");
        }
        return false;
    }

    FOGTerritoryDomainStateRecord DomainState;
    if (!Store.TryReadTerritoryDomainState(
            ScenarioTerritoryId(),
            bFound,
            DomainState,
            OutError) ||
        !bFound ||
        DomainState.ActiveCoreId !=
            ScenarioCoreId() ||
        DomainState.DomainState !=
            FName(TEXT("damaged")))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Domain-heart state did not survive restart.");
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

    FOGDispatchRecord Dispatch;
    if (!Store.TryReadDispatch(
            OutResult.DispatchId,
            bFound,
            Dispatch,
            OutError) ||
        !bFound ||
        Dispatch.Status !=
            EOGDispatchStatus::Succeeded ||
        Dispatch.OutcomeState !=
            FName(TEXT("objective_complete")))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Objective-faithful Dispatch state did not survive restart.");
        }
        return false;
    }

    TArray<FOGDispatchObjectiveRecord> Objectives;
    TArray<FOGDispatchConstraintRecord> Constraints;
    if (!Store.ListDispatchObjectives(
            OutResult.DispatchId,
            Objectives,
            OutError) ||
        !Store.ListDispatchConstraints(
            OutResult.DispatchId,
            Constraints,
            OutError) ||
        Objectives.Num() != 1 ||
        !Objectives[0].bMandatory ||
        Objectives[0].ObjectiveId !=
            DispatchObjectiveId() ||
        Constraints.Num() != 1 ||
        Constraints[0].ConstraintId !=
            DispatchConstraintId())
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Dispatch objective/constraint fidelity did not survive restart.");
        }
        return false;
    }

    FOGWarRecord War;
    if (!Store.TryReadWar(
            OutResult.WarId,
            bFound,
            War,
            OutError) ||
        !bFound ||
        War.Status !=
            EOGWarStatus::Active)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Continuous War parent did not survive restart.");
        }
        return false;
    }

    TArray<FOGWarFrontRecord> Fronts;
    if (!Store.ListWarFronts(
            OutResult.WarId,
            Fronts,
            OutError) ||
        !Fronts.ContainsByPredicate(
            [&OutResult](
                const FOGWarFrontRecord& Front)
            {
                return Front.FrontId ==
                    OutResult.WarFrontId;
            }))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("War front did not survive restart.");
        }
        return false;
    }

    FName BossKind = NAME_None;
    FString BossState;
    int64 BossRevision = 0;
    if (!Store.TryReadEntity(
            ScenarioBossId(),
            bFound,
            BossKind,
            BossState,
            BossRevision,
            OutError) ||
        !bFound ||
        !BossState.Contains(
            TEXT("persistent_action_consequence")))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("World Mode action-combat consequence did not survive restart.");
        }
        return false;
    }

    FOGKnowledgeFactRecord BossCondition;
    if (!Store.TryReadKnowledgeFact(
            ScenarioRulerId(),
            FName(TEXT("target_condition")),
            ScenarioBossId(),
            bFound,
            BossCondition,
            OutError) ||
        !bFound ||
        BossCondition.BeliefState !=
            FName(TEXT("confirmed")) ||
        BossCondition.SourceEventId !=
            OutResult.ActionCombatEventId)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Knowledge/event provenance from action combat did not survive restart.");
        }
        return false;
    }

    FOGReportRecord Report;
    if (!Store.TryReadReport(
            OutResult.ReportId,
            bFound,
            Report,
            OutError) ||
        !bFound ||
        Report.SourceWorldEventId !=
            OutResult.ActionCombatEventId)
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Report/event history projection did not survive restart.");
        }
        return false;
    }

    // Report -> world_event foreign-key integrity proves the source event remains
    // in canonical history even though world_events intentionally has no broad
    // player-facing direct read API.
    FString IntegrityReport;
    if (!Store.RunIntegrityCheck(
            IntegrityReport,
            OutError))
    {
        return false;
    }

    FOGUiViewModelService Ui(
        Store);

    FOGRulerShellViewModel Shell;
    TArray<FOGRosterIdentityViewModel> Roster;
    FOGTerritoryViewModel TerritoryView;
    FOGWorldHudViewModel Hud;
    if (!Ui.BuildRulerShell(
            ScenarioRulerId(),
            Shell,
            OutError) ||
        !Ui.BuildRoster(
            ScenarioRulerId(),
            Roster,
            OutError) ||
        !Ui.BuildTerritory(
            ScenarioRulerId(),
            ScenarioTerritoryId(),
            TerritoryView,
            OutError) ||
        !Ui.BuildWorldHud(
            ScenarioRulerId(),
            ScenarioRulerId(),
            {
                OutResult.FirstManifestationId,
                ScenarioWorldAllyId()
            },
            {},
            &BossCondition,
            false,
            Hud,
            OutError))
    {
        return false;
    }

    const FOGRosterIdentityViewModel* AcquiredRoster =
        Roster.FindByPredicate(
            [](const FOGRosterIdentityViewModel& Candidate)
            {
                return Candidate.IdentityId ==
                    AcquiredIdentityId();
            });

    if (Shell.PrimaryDestinations.Num() != 5 ||
        !Shell.bGachaUnlocked ||
        !AcquiredRoster ||
        AcquiredRoster->ManifestationCount != 2 ||
        !Hud.bShowMinimap ||
        !Hud.bShowExactEnemyState)
    {
        OutError =
            TEXT("Reconciled UI projections do not reproduce the restarted canonical world.");
        return false;
    }

    if (OutResult.TurnBattleFingerprint.IsEmpty() ||
        OutResult.ActionDamageDisplay.IsEmpty())
    {
        OutError =
            TEXT("Combat proof fingerprints/presentation checkpoint did not survive restart.");
        return false;
    }

    OutResult.bSucceeded =
        true;
    return true;
}
