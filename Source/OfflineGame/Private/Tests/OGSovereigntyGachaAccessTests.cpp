#include "Gacha/OGGachaService.h"
#include "Gacha/OGRulerGachaAccessService.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGSovereigntyService.h"
#include "World/OGTerritoryControlService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeSovereigntyTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PersistRuler(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& RulerId,
    FString& Error)
{
    return Store.UpsertEntity(
        RulerId,
        TEXT("ruler"),
        0,
        TEXT("{}"),
        Error);
}

bool PersistLocationAndTerritory(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& LocationId,
    const FOGEntityId& TerritoryId,
    const FOGEntityId& CachedRulerId,
    bool bMain,
    FString& Error)
{
    FOGLocationRecord Location;
    Location.LocationId =
        LocationId;
    Location.Kind =
        FName(TEXT("region"));

    if (!Store.UpsertLocation(
            Location,
            0,
            Error))
    {
        return false;
    }

    FOGTerritoryRecord Territory;
    Territory.TerritoryId =
        TerritoryId;
    Territory.RulerId =
        CachedRulerId;
    Territory.RootLocationId =
        LocationId;
    Territory.bMainTerritory =
        bMain;
    Territory.Population = 1;
    Territory.ControlState =
        FName(TEXT("controlled"));

    return Store.UpsertTerritory(
        Territory,
        0,
        Error);
}

FOGTerritoryClaimRecord MakeEffectiveClaim(
    const FOGEntityId& ClaimId,
    const FOGEntityId& TerritoryId,
    const FOGEntityId& RulerId,
    int64 StartTick,
    int32 StrengthBps)
{
    FOGTerritoryClaimRecord Claim;
    Claim.ClaimId = ClaimId;
    Claim.TerritoryId =
        TerritoryId;
    Claim.RulerId =
        RulerId;
    Claim.ClaimKind =
        FName(TEXT("control"));
    Claim.ControlState =
        FName(TEXT("controlled"));
    Claim.ControlStrengthBps =
        StrengthBps;
    Claim.ClaimStartWorldTick =
        StartTick;
    Claim.bHasEffectiveControlStart =
        true;
    Claim.EffectiveControlStartWorldTick =
        StartTick;
    Claim.UpdatedWorldTick =
        StartTick;
    return Claim;
}

FOGGachaBannerDefinition MakeAccessGateBanner()
{
    FOGGachaBannerDefinition Banner;
    Banner.BannerId =
        FOGContentId(TEXT("test:banner.access_gate"));
    Banner.PityCategory =
        FName(TEXT("access_gate"));
    Banner.CurrencyId =
        FOGContentId(TEXT("test:currency.access_gate"));
    Banner.PullCost = 100;
    Banner.TopRarity =
        FName(TEXT("UR"));
    Banner.SoftPityStart = 0;
    Banner.HardPity = 1;

    FOGGachaPoolEntry Entry;
    Entry.IdentityId =
        FOGContentId(TEXT("test:character.access_gate"));
    Entry.VersionId =
        FOGContentId(TEXT("test:character.access_gate.base"));
    Entry.Rarity =
        FName(TEXT("UR"));
    Entry.Weight = 1;
    Entry.bFeatured = true;
    Banner.Entries.Add(Entry);
    return Banner;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGTerritoryReclamationAndSovereigntyTest,
    "OfflineGame.Sovereignty.OverlappingClaimsReclamationAndPermanentGachaUnlock",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGTerritoryReclamationAndSovereigntyTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeSovereigntyTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("sovereignty.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-8 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 8"),
        Store.GetSchemaVersion(Error),
        8);

    const FOGEntityId RulerA =
        FOGEntityId::NewId();
    const FOGEntityId RulerB =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId ClaimAId =
        FOGEntityId::NewId();
    const FOGEntityId ClaimBId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist Ruler A"),
        PersistRuler(
            Store,
            RulerA,
            Error));
    TestTrue(
        TEXT("Persist Ruler B"),
        PersistRuler(
            Store,
            RulerB,
            Error));
    TestTrue(
        TEXT("Persist Territory and normalized root relation"),
        PersistLocationAndTerritory(
            Store,
            LocationId,
            TerritoryId,
            RulerA,
            true,
            Error));

    FOGTerritoryClaimRecord ClaimA =
        MakeEffectiveClaim(
            ClaimAId,
            TerritoryId,
            RulerA,
            10,
            9000);
    FOGTerritoryClaimRecord ClaimB =
        MakeEffectiveClaim(
            ClaimBId,
            TerritoryId,
            RulerB,
            20,
            8000);

    TestTrue(
        TEXT("Persist first overlapping claim"),
        Store.UpsertTerritoryClaim(
            ClaimA,
            10,
            Error));
    TestTrue(
        TEXT("Persist second overlapping claim"),
        Store.UpsertTerritoryClaim(
            ClaimB,
            20,
            Error));

    FOGTerritoryControlService Control(
        Store);
    FOGTerritoryEffectiveControlResult Effective;
    TestTrue(
        TEXT("Evaluate overlapping effective control"),
        Control.EvaluateEffectiveControl(
            TerritoryId,
            25,
            Effective,
            Error));
    TestEqual(
        TEXT("Both genuine effective controllers are preserved"),
        Effective.EffectiveRulerIds.Num(),
        2);
    TestTrue(
        TEXT("Overlapping effective control is contested"),
        Effective.bContested);

    TArray<FOGTerritoryClaimRecord> LocationClaims;
    TestTrue(
        TEXT("List active claims for physical location"),
        Control.ListActiveClaimsForLocation(
            LocationId,
            LocationClaims,
            Error));
    TestEqual(
        TEXT("Location sees both claims"),
        LocationClaims.Num(),
        2);

    FOGRulerGachaAccessService Access(
        Store);
    FOGRulerGachaAccessRecord AccessState;
    TestTrue(
        TEXT("Start gacha qualification under effective control"),
        Access.RefreshGachaQualification(
            RulerA,
            10,
            false,
            AccessState,
            Error));
    TestTrue(
        TEXT("Qualification start is persisted"),
        AccessState.bHasQualificationStart);
    TestEqual(
        TEXT("Qualification starts from first observed valid control"),
        AccessState.QualificationStartWorldTick,
        static_cast<int64>(10));

    FOGSovereigntyService Sovereignty(
        Store);
    FOGRulerSovereigntyStateRecord SovereigntyState;
    TestTrue(
        TEXT("Project Overlord state when external requirements are met"),
        Sovereignty.RefreshSovereigntyState(
            RulerA,
            25,
            true,
            TEXT("{\"test_scope\":\"high_order\"}"),
            SovereigntyState,
            Error));
    TestEqual(
        TEXT("Overlord is the active universal title"),
        SovereigntyState.CurrentTitle,
        FName(TEXT("overlord")));
    TestEqual(
        TEXT("Historical peak records Overlord"),
        SovereigntyState.HistoricalPeakTitle,
        FName(TEXT("overlord")));

    // The deadline value is supplied by the future authoritative calendar/time
    // layer. The service deliberately does not invent a day-to-tick ratio.
    TestTrue(
        TEXT("Begin five-day reclamation state using supplied deadline"),
        Control.BeginDisplacement(
            ClaimAId,
            30,
            80,
            Error));

    TestTrue(
        TEXT("Suspend gacha qualification during reclamation grace"),
        Access.RefreshGachaQualification(
            RulerA,
            30,
            false,
            AccessState,
            Error));
    TestTrue(
        TEXT("Grace preserves original qualification start"),
        AccessState.bHasQualificationStart);
    TestEqual(
        TEXT("Grace does not reset qualification start"),
        AccessState.QualificationStartWorldTick,
        static_cast<int64>(10));
    TestTrue(
        TEXT("Qualification suspension is explicit"),
        AccessState.bHasQualificationSuspendedTick);

    TestTrue(
        TEXT("Reclaiming exactly at the deadline is allowed"),
        Control.ReclaimTerritory(
            ClaimAId,
            80,
            Error));

    TestTrue(
        TEXT("Resume qualification after reclamation"),
        Access.RefreshGachaQualification(
            RulerA,
            80,
            false,
            AccessState,
            Error));
    TestEqual(
        TEXT("Reclamation preserves continuous qualification origin"),
        AccessState.QualificationStartWorldTick,
        static_cast<int64>(10));
    TestFalse(
        TEXT("Reclamation clears suspension"),
        AccessState.bHasQualificationSuspendedTick);

    TestTrue(
        TEXT("Authoritative calendar can confirm strictly more than one month"),
        Access.RefreshGachaQualification(
            RulerA,
            81,
            true,
            AccessState,
            Error));
    TestTrue(
        TEXT("More-than-one-month qualification permanently unlocks gacha"),
        AccessState.bPermanentlyUnlocked);
    TestTrue(
        TEXT("Unlock tick is persisted"),
        AccessState.bHasUnlockedWorldTick);

    TestTrue(
        TEXT("Displace again after permanent unlock"),
        Control.BeginDisplacement(
            ClaimAId,
            90,
            140,
            Error));

    bool bExpired = false;
    TestTrue(
        TEXT("Expire reclamation after deadline"),
        Control.ExpireReclamationIfDue(
            ClaimAId,
            141,
            bExpired,
            Error));
    TestTrue(
        TEXT("Expired grace produces true Territory loss"),
        bExpired);

    TestTrue(
        TEXT("Refresh access after total continuity break"),
        Access.RefreshGachaQualification(
            RulerA,
            141,
            false,
            AccessState,
            Error));
    TestTrue(
        TEXT("Permanent gacha unlock survives later Territory loss"),
        AccessState.bPermanentlyUnlocked);

    TestTrue(
        TEXT("Refresh sovereignty after Territory loss"),
        Sovereignty.RefreshSovereigntyState(
            RulerA,
            141,
            false,
            TEXT("{}"),
            SovereigntyState,
            Error));
    TestEqual(
        TEXT("No active sovereignty title remains after grace expires"),
        SovereigntyState.CurrentTitle,
        FName(TEXT("none")));
    TestEqual(
        TEXT("Historical Overlord peak survives loss"),
        SovereigntyState.HistoricalPeakTitle,
        FName(TEXT("overlord")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGachaAccessGateAndManifestationAnchorTest,
    "OfflineGame.Gacha.AccessGateAndFirstTerritoryAnchor",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGachaAccessGateAndManifestationAnchorTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeSovereigntyTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("gacha_access.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open database"),
        Store.Open(
            DatabasePath,
            Error));

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId ClaimId =
        FOGEntityId::NewId();
    const FOGGachaBannerDefinition Banner =
        MakeAccessGateBanner();

    TestTrue(
        TEXT("Persist Ruler"),
        PersistRuler(
            Store,
            RulerId,
            Error));
    TestTrue(
        TEXT("Persist main Territory"),
        PersistLocationAndTerritory(
            Store,
            LocationId,
            TerritoryId,
            RulerId,
            true,
            Error));

    FOGTerritoryClaimRecord Claim =
        MakeEffectiveClaim(
            ClaimId,
            TerritoryId,
            RulerId,
            0,
            10000);
    TestTrue(
        TEXT("Persist effective Territory claim"),
        Store.UpsertTerritoryClaim(
            Claim,
            0,
            Error));

    TestTrue(
        TEXT("Seed pull currency"),
        Store.SetResourceBalance(
            RulerId,
            Banner.CurrencyId,
            500,
            Error));

    FOGGachaService Gacha(
        Store);
    FOGGachaPullResult LockedPull;
    TestFalse(
        TEXT("Locked Ruler cannot pull"),
        Gacha.Pull(
            Banner,
            RulerId,
            1,
            1001,
            LockedPull,
            Error));

    bool bKnown = false;
    int64 Balance = -1;
    TestTrue(
        TEXT("Read currency after rejected pull"),
        Store.TryReadResourceBalance(
            RulerId,
            Banner.CurrencyId,
            bKnown,
            Balance,
            Error));
    TestTrue(
        TEXT("Rejected pull leaves currency untouched"),
        bKnown &&
        Balance == 500);

    bool bGachaStateFound = false;
    FOGGachaStateRecord GachaState;
    TestTrue(
        TEXT("Read gacha state after rejected pull"),
        Store.TryReadGachaState(
            RulerId,
            Banner.PityCategory,
            bGachaStateFound,
            GachaState,
            Error));
    TestFalse(
        TEXT("Rejected pull does not mutate pity state"),
        bGachaStateFound);

    FOGRulerGachaAccessService Access(
        Store);
    FOGRulerGachaAccessRecord AccessState;
    TestTrue(
        TEXT("Begin qualification"),
        Access.RefreshGachaQualification(
            RulerId,
            0,
            false,
            AccessState,
            Error));
    TestTrue(
        TEXT("Complete more-than-one-month qualification"),
        Access.RefreshGachaQualification(
            RulerId,
            2,
            true,
            AccessState,
            Error));

    FOGGachaPullResult Pull;
    TestTrue(
        TEXT("Permanently unlocked Ruler can pull"),
        Gacha.Pull(
            Banner,
            RulerId,
            3,
            1002,
            Pull,
            Error));

    bool bManifestationFound = false;
    FOGCharacterManifestationRecord Manifestation;
    TestTrue(
        TEXT("Read acquired Manifestation"),
        Store.TryReadCharacterManifestation(
            Pull.ManifestationId,
            bManifestationFound,
            Manifestation,
            Error));
    TestTrue(
        TEXT("Acquired Manifestation exists"),
        bManifestationFound);
    TestTrue(
        TEXT("New Manifestation is anchored to effective main Territory"),
        Manifestation.WorldModeAnchorTerritoryId ==
            TerritoryId);
    TestEqual(
        TEXT("Anchor tick matches acquisition tick"),
        Manifestation.WorldModeAnchorTick,
        static_cast<int64>(3));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
