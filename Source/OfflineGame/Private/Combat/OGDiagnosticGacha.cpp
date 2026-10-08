#include "Combat/OGDiagnosticGacha.h"
#include "World/OGRealityGraphService.h"
#include "World/OGDomainCoreService.h"
#include "Progression/OGCharacterProgressionService.h"
#include "Runtime/OGCharacterVisualResolverProvider.h"
#include "Runtime/OGPackageManagerService.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FOGEntityId OGDiagnosticGacha::ScopedId(uint32 Ordinal)
{
    return FOGEntityId(FGuid(0x4f474449, 0x41474e4f, 0x53544943, Ordinal));
}

FOGEntityId OGDiagnosticGacha::TrainingRealityId() { return ScopedId(9); }

bool OGDiagnosticGacha::EnsureScopedFixture(IOGWorldStore& Store, int64 CanonicalWorldTick, FString& Error)
{
#if UE_BUILD_SHIPPING
    Error = TEXT("Training content is unavailable in shipping."); return false;
#else
    if (CanonicalWorldTick < 0) { Error = TEXT("Canonical command tick is invalid."); return false; }
    const FOGEntityId Ruler = ScopedId(1), TerritoryId = ScopedId(2), LocationId = ScopedId(3), DomainId = ScopedId(4);
    const FString Marker(TEXT("{\"diagnosticFixture\":\"combat_v2_actual_elapsed\"}"));
    bool bFound = false;
    FName Kind;
    FString Json;
    int64 Created = 0;
    if (!Store.TryReadEntity(Ruler, bFound, Kind, Json, Created, Error)) return false;
    if (bFound && (Kind != FName(TEXT("ruler")) || Json != Marker))
    { Error = TEXT("Scoped training Ruler ID conflicts with existing canonical content."); return false; }
    auto EnsureReinforcement = [&Store, CanonicalWorldTick, &Error](const FOGEntityId& ManifestationId)
    {
        bool bReinforcementFound = false;
        FOGManifestationReinforcementRecord Reinforcement;
        if (!Store.TryReadManifestationReinforcement(ManifestationId, bReinforcementFound, Reinforcement, Error)) return false;
        if (bReinforcementFound) return true;
        Reinforcement.ManifestationId = ManifestationId;
        Reinforcement.UpdatedWorldTick = CanonicalWorldTick;
        Reinforcement.StateJson = TEXT("{\"source\":\"authored_training_fixture\"}");
        return FOGCharacterProgressionService(Store).SetReinforcement(Reinforcement, Error);
    };
    if (!bFound)
    {
        // Preflight every deterministic non-Ruler ID. Never overwrite a collision.
        for (uint32 Ordinal = 2; Ordinal <= 7; ++Ordinal)
        {
            bool bCollision = false;
            if (!Store.TryReadEntity(ScopedId(Ordinal), bCollision, Kind, Json, Created, Error)) return false;
            if (bCollision) { Error = TEXT("Scoped diagnostic entity ID is already occupied."); return false; }
        }
        bool bTerritoryFound = false;
        FOGTerritoryRecord ExistingTerritory;
        if (!Store.TryReadTerritory(TerritoryId, bTerritoryFound, ExistingTerritory, Error)) return false;
        bool bDomainFound = false;
        FOGTimeDomainRecord ExistingDomain;
        if (!Store.TryReadTimeDomain(DomainId, bDomainFound, ExistingDomain, Error)) return false;
        if (bTerritoryFound || bDomainFound) { Error = TEXT("Scoped diagnostic record conflicts with existing content."); return false; }
        if (!Store.BeginTransaction(Error)) return false;
        auto Fail = [&Store]() { FString Ignored; Store.RollbackTransaction(Ignored); return false; };
        FOGLocationRecord Location;
        Location.LocationId = LocationId; Location.TerritoryId = TerritoryId; Location.Kind = TEXT("diagnostic_training");
        FOGTerritoryRecord Territory;
        Territory.TerritoryId = TerritoryId; Territory.RulerId = Ruler; Territory.RootLocationId = LocationId; Territory.bMainTerritory = true;
        Territory.ControlState = FName(TEXT("controlled"));
        FOGTerritoryClaimRecord Claim;
        Claim.ClaimId = ScopedId(5); Claim.RulerId = Ruler; Claim.TerritoryId = TerritoryId;
        Claim.ClaimKind = FName(TEXT("control"));
        Claim.ControlState = FName(TEXT("controlled"));
        Claim.ControlStrengthBps = 10000; Claim.bHasEffectiveControlStart = true;
        Claim.ClaimStartWorldTick = CanonicalWorldTick;
        Claim.EffectiveControlStartWorldTick = CanonicalWorldTick;
        Claim.UpdatedWorldTick = CanonicalWorldTick;
        Claim.StateJson = TEXT("{\"authoredTrainingHistory\":\"effective_control_begins_at_actual_entry\"}");
        FOGTimeDomainRecord Domain;
        Domain.TimeDomainId = DomainId; Domain.CalendarId = FOGContentId(TEXT("diagnostic:calendar.thirty_day"));
        // Authored training day is one canonical second/tick. Qualification
        // waits strictly more than thirty actually elapsed ticks.
        Domain.RateNumerator = Domain.RateDenominator = 1;
        Domain.ParentEpochTick = CanonicalWorldTick;
        Domain.LocalEpochTick = 0;
        FOGLocationTerritoryRecord Membership;
        Membership.LocationId = LocationId; Membership.TerritoryId = TerritoryId;
        FOGTerritoryDomainStateRecord TerritoryDomainState;
        // Location.territory_entity_id references the canonical entity row,
        // while Territory.root_location_entity_id references the Location row.
        // Seed only the Territory entity shell first to satisfy that intentional
        // two-table relationship without weakening either foreign key.
        if (!Store.UpsertEntity(Ruler, TEXT("ruler"), 0, Marker, Error) ||
            !Store.UpsertEntity(TerritoryId, TEXT("territory"), CanonicalWorldTick, TEXT("{}"), Error) ||
            !Store.UpsertLocation(Location, CanonicalWorldTick, Error) ||
            !Store.UpsertTerritory(Territory, CanonicalWorldTick, Error) ||
            !FOGDomainCoreService(Store).RefreshDomainHeartConsequences(TerritoryId, CanonicalWorldTick, false, TerritoryDomainState, Error) ||
            !Store.UpsertLocationTerritory(Membership, Error) ||
            !Store.UpsertTerritoryClaim(Claim, 0, Error) || !Store.UpsertTimeDomain(Domain, 0, Error)) return Fail();
        for (int32 Kit = 1; Kit <= 2; ++Kit)
        {
            FOGCharacterManifestationRecord Copy;
            Copy.ManifestationId = ScopedId(5 + Kit); Copy.OwningRulerId = Ruler;
            Copy.IdentityId = FOGContentId(FString::Printf(TEXT("diagnostic:identity.kit_%d"), Kit));
            Copy.ActiveVersionId = FOGContentId(FString::Printf(TEXT("diagnostic:version.kit_%d.base"), Kit));
            Copy.WorldModeAnchorTerritoryId = TerritoryId;
            Copy.WorldModeAnchorTick = CanonicalWorldTick;
            Copy.CurrentRarity = Kit == 1 ? FName(TEXT("UR")) : FName(TEXT("SR"));
            Copy.ProgressionStateJson = TEXT("{\"authoredTrainingRecruitment\":true}");
            if (!Store.UpsertCharacterManifestation(Copy, CanonicalWorldTick, Error) || !EnsureReinforcement(Copy.ManifestationId)) return Fail();
        }
        const FOGGachaBannerDefinition Banner = MakeBanner();
        if (!Store.SetResourceBalance(Ruler, Banner.CurrencyId, 1800, Error) ||
            !Store.SetResourceBalance(Ruler, Banner.CompatibleTicketIds[0], 2, Error)) return Fail();
        if (!Store.CommitTransaction(Error)) return Fail();
    }
    // Initialize only missing progression rows on older scoped recruits.
    for (int32 Kit = 1; Kit <= 2; ++Kit)
        if (!EnsureReinforcement(ScopedId(5 + Kit))) return false;
    // Older scoped fixtures omitted their required Domain-heart projection.
    // Initialize only the missing state through the canonical service; preserve
    // existing hearts, clocks, recruitment, currency and Territory history.
    bool bTerritoryDomainFound = false;
    FOGTerritoryDomainStateRecord TerritoryDomainState;
    if (!Store.TryReadTerritoryDomainState(TerritoryId, bTerritoryDomainFound, TerritoryDomainState, Error)) return false;
    if (!bTerritoryDomainFound &&
        !FOGDomainCoreService(Store).RefreshDomainHeartConsequences(TerritoryId, CanonicalWorldTick, false, TerritoryDomainState, Error)) return false;
    // Existing fixture migration also adds the missing authored Reality binding.
    bool RealityFound = false;
    FOGRealityNodeRecord Reality;
    if (!Store.TryReadRealityNode(TrainingRealityId(), RealityFound, Reality, Error)) return false;
    if (RealityFound)
    {
        if (Reality.Kind != FName(TEXT("diagnostic_training")) || Reality.TimeDomainId != DomainId)
        { Error = TEXT("Training Reality ID conflicts with existing content."); return false; }
    }
    else
    {
        bool EntityFound = false; FName EntityKind; FString EntityJson; int64 EntityCreated = 0;
        if (!Store.TryReadEntity(TrainingRealityId(), EntityFound, EntityKind, EntityJson, EntityCreated, Error)) return false;
        if (EntityFound) { Error = TEXT("Training Reality entity ID is already occupied."); return false; }
        Reality.RealityId = TrainingRealityId(); Reality.Kind = FName(TEXT("diagnostic_training"));
        Reality.TimeDomainId = DomainId;
        // Location records deliberately contain physical state only. This
        // authored node metadata binds the diagnostic site without modifying
        // the generic Location schema or inventing a second time source.
        Reality.StateJson = FString::Printf(TEXT("{\"physicalLocationId\":\"%s\",\"authoredDiagnostic\":true}"), *LocationId.ToString());
        if (!FOGRealityGraphService(Store).SaveRealityNode(Reality, CanonicalWorldTick, Error)) return false;
    }
    // Neutral authored visual metadata lives in the same canonical package
    // catalog consumed by every presenter, not a diagnostic character authority.
    const FOGContentId VisualPackageId(TEXT("diagnostic:package.character_visuals"));
    bool VisualFound = false;
    FOGContentPackageRecord VisualPackage;
    if (!Store.TryReadContentPackageRecord(VisualPackageId, VisualFound, VisualPackage, Error)) return false;
    if (VisualFound && (VisualPackage.Version != 1 ||
        VisualPackage.InstallUri != TEXT("embedded:foundation_character_visuals") ||
        VisualPackage.ContentHash != TEXT("embedded:foundation_character_visuals_v1")))
    { Error = TEXT("Diagnostic visual package conflicts with existing content; preserving it."); return false; }
    if (!VisualFound)
    {
        VisualPackage.PackageId = VisualPackageId;
        VisualPackage.Version = 1;
        VisualPackage.ContentHash = TEXT("embedded:foundation_character_visuals_v1");
        VisualPackage.InstallUri = TEXT("embedded:foundation_character_visuals");
        VisualPackage.bInstalled = VisualPackage.bValidated = true;
        VisualPackage.Category = FName(TEXT("character_presentation"));
    }
    const FString PreviousManifest = VisualPackage.ManifestJson;
    FString AuthoredTemplate;
    if (!FFileHelper::LoadFileToString(AuthoredTemplate,
        *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Foundation"), TEXT("CharacterVisualDiagnostic.json"))))
    { Error = TEXT("Neutral authored visual diagnostic template is unavailable."); return false; }
    for (int32 Kit = 1; Kit <= 2; ++Kit)
    {
        FOGFoundationCharacterContext Character;
        Character.EntityId = Character.ManifestationId = ScopedId(5 + Kit);
        Character.OwnerEntityId = Ruler;
        FOGFoundationCharacterProjection Projection;
        FString Row;
        if (!FOGFoundationCharacterRuntime(Store).Project(Character, Projection, Error) ||
            !FOGCharacterVisualResolverProvider::MakeDiagnosticRow(Projection, AuthoredTemplate, Row, Error) ||
            !FOGCharacterVisualResolverProvider::MergeAuthoredMetadata(VisualPackage, Row, Error)) return false;
    }
    // Preserve activation/leases when idempotent entry does not change metadata.
    if (!VisualFound || VisualPackage.ManifestJson != PreviousManifest)
    {
        FOGPackageManagerService Packages(Store);
        if (!Packages.RegisterPackage(VisualPackage, Error) ||
            !Packages.ActivatePackage(VisualPackage.PackageId, Error)) return false;
    }
    // Existing fixture entry does not refill currency/tickets or replace pulls.
    FOGRulerGachaAccessService Access(Store);
    FOGRulerGachaAccessRecord State;
    return Access.RefreshGachaQualification(Ruler, CanonicalWorldTick, DomainId,
        MakeCalendarResolver(), State, Error);
#endif
}

FOGGachaBannerDefinition OGDiagnosticGacha::MakeBanner()
{
    FOGGachaBannerDefinition Banner;
    Banner.BannerId = FOGContentId(TEXT("diagnostic:banner.companions"));
    Banner.PityCategory = TEXT("diagnostic_companions");
    Banner.CurrencyId = FOGContentId(TEXT("diagnostic:currency.pull"));
    Banner.CompatibleTicketIds.Add(FOGContentId(TEXT("diagnostic:ticket.companions")));
    Banner.PullCost = 100;
    Banner.TopRarity = TEXT("UR");
    Banner.HardPity = 2;
    for (int32 Kit = 1; Kit <= 2; ++Kit)
    {
        FOGGachaPoolEntry Entry;
        Entry.IdentityId = FOGContentId(FString::Printf(TEXT("diagnostic:identity.kit_%d"), Kit));
        Entry.VersionId = FOGContentId(FString::Printf(TEXT("diagnostic:version.kit_%d.base"), Kit));
        Entry.Rarity = Kit == 1 ? FName(TEXT("UR")) : FName(TEXT("SR"));
        Entry.Weight = 100;
        Entry.bFeatured = Kit == 1;
        Banner.Entries.Add(Entry);
    }
    return Banner;
}

FOGCalendarElapsedResolver OGDiagnosticGacha::MakeCalendarResolver()
{
    return [](const FOGCalendarElapsedQuery& Query, bool& bElapsed, FString& Error)
    {
        bElapsed = false;
        if (Query.CalendarId != FOGContentId(TEXT("diagnostic:calendar.thirty_day")) ||
            Query.DurationKind != FName(TEXT("month")) || Query.DurationCount != 1 ||
            Query.EndLocalTick < Query.StartLocalTick)
        { Error = TEXT("Diagnostic calendar cannot resolve this duration."); return false; }
        const int64 Elapsed = Query.EndLocalTick - Query.StartLocalTick;
        bElapsed = Query.bStrictlyMoreThan ? Elapsed > MonthTicks : Elapsed >= MonthTicks;
        Error.Reset();
        return true;
    };
}

bool OGDiagnosticGacha::Project(IOGWorldStore& Store, const FOGEntityId& RulerId,
    const FOGGachaBannerDefinition& Banner, FOGGachaViewModel& OutGacha,
    TArray<FOGGachaHistoryEntryViewModel>& OutHistory,
    TArray<FOGRosterIdentityViewModel>& OutRoster, FString& Error)
{
    FOGUiViewModelService Ui(Store);
    return Ui.BuildGacha(RulerId, Banner, OutGacha, Error) &&
        Ui.BuildGachaHistory(RulerId, 30, OutHistory, Error) &&
        Ui.BuildRoster(RulerId, OutRoster, Error);
}