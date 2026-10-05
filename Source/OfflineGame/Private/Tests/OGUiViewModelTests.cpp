#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGManifestationManagementService.h"
#include "Runtime/OGReportService.h"
#include "UI/OGUiViewModelService.h"
#include "World/OGSharedWorldStateService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeUiProjectionTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool PersistUiEntity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& Id,
    FName Kind,
    FString& Error)
{
    return Store.UpsertEntity(
        Id,
        Kind,
        0,
        TEXT("{}"),
        Error);
}

bool PersistUiManifestation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& Id,
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    int32 Ordinal,
    int64 Tick,
    FString& Error)
{
    FOGCharacterManifestationRecord Manifestation;
    Manifestation.ManifestationId = Id;
    Manifestation.OwningRulerId = RulerId;
    Manifestation.IdentityId = IdentityId;
    Manifestation.ActiveVersionId =
        FOGContentId(
            FString::Printf(
                TEXT("test:version.%d"),
                Ordinal));
    Manifestation.Level = 1 + Ordinal;
    Manifestation.CurrentRarity =
        Ordinal == 0
            ? FName(TEXT("sr"))
            : FName(TEXT("ur"));
    Manifestation.AcquisitionWorldTick = Tick;
    Manifestation.AcquisitionOrdinal = Ordinal;
    Manifestation.LifecycleState =
        FName(TEXT("active"));
    Manifestation.BuildLabel =
        FString::Printf(
            TEXT("Build %d"),
            Ordinal + 1);

    return Store.UpsertCharacterManifestation(
        Manifestation,
        Tick,
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRulerUiStableGrammarTest,
    "OfflineGame.UI.RulerMode.StableFiveDestinationGrammarAndRecordsHub",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRulerUiStableGrammarTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiProjectionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("ruler_ui.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-14 UI database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 14"),
        Store.GetSchemaVersion(
            Error),
        14);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));

    FOGRulerGachaAccessRecord Access;
    Access.RulerId = RulerId;
    Access.bPermanentlyUnlocked = true;
    Access.bHasUnlockedWorldTick = true;
    Access.UnlockedWorldTick = 10;
    Access.UpdatedWorldTick = 10;
    TestTrue(
        TEXT("Persist permanent gacha access"),
        Store.UpsertRulerGachaAccess(
            Access,
            Error));

    FOGReportService Reports(
        Store);
    FOGEntityId ReportId;
    TestTrue(
        TEXT("Create contextual Report"),
        Reports.CreateReport(
            RulerId,
            FOGEntityId(),
            FName(TEXT("project")),
            20,
            11,
            TEXT("{\"summary\":\"project completed\"}"),
            ReportId,
            Error));

    FOGUiViewModelService Ui(
        Store);

    FOGRulerShellViewModel Shell;
    TestTrue(
        TEXT("Build Ruler shell projection"),
        Ui.BuildRulerShell(
            RulerId,
            Shell,
            Error));

    TestEqual(
        TEXT("Ruler Mode has exactly five primary destinations"),
        Shell.PrimaryDestinations.Num(),
        5);
    TestEqual(
        TEXT("Home remains first"),
        Shell.PrimaryDestinations[0],
        EOGRulerPrimaryDestination::Home);
    TestEqual(
        TEXT("Characters remains second"),
        Shell.PrimaryDestinations[1],
        EOGRulerPrimaryDestination::Characters);
    TestEqual(
        TEXT("Gacha remains third"),
        Shell.PrimaryDestinations[2],
        EOGRulerPrimaryDestination::Gacha);
    TestEqual(
        TEXT("Territory remains fourth"),
        Shell.PrimaryDestinations[3],
        EOGRulerPrimaryDestination::Territory);
    TestEqual(
        TEXT("Records remains fifth"),
        Shell.PrimaryDestinations[4],
        EOGRulerPrimaryDestination::Records);
    TestTrue(
        TEXT("Gacha availability comes from canonical unlock state"),
        Shell.bGachaUnlocked);
    TestEqual(
        TEXT("Urgent state is contextual Report count rather than permanent Home dashboard clutter"),
        Shell.UnacknowledgedReportCount,
        1);
    TestTrue(
        TEXT("Records destination always opens its chooser"),
        Shell.bRecordsOpensHub);

    FOGRecordsHubViewModel RecordsHub;
    TestTrue(
        TEXT("Build Records hub"),
        Ui.BuildRecordsHub(
            RulerId,
            RecordsHub,
            Error));
    TestTrue(
        TEXT("Records opens general hub"),
        RecordsHub.bOpenAtGeneralHub);
    TestEqual(
        TEXT("Records hub exposes four internal destinations"),
        RecordsHub.Destinations.Num(),
        4);
    TestEqual(
        TEXT("Reports is first Records destination"),
        RecordsHub.Destinations[0],
        FName(TEXT("reports")));
    TestEqual(
        TEXT("Intelligence remains distinct"),
        RecordsHub.Destinations[3],
        FName(TEXT("intelligence")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRosterIdentityGroupingViewModelTest,
    "OfflineGame.UI.Characters.IdentityGroupedRosterUsesLastUsedManifestation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRosterIdentityGroupingViewModelTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiProjectionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("roster_ui.db"));
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
    const FOGEntityId OlderCopy =
        FOGEntityId::NewId();
    const FOGEntityId NewerCopy =
        FOGEntityId::NewId();
    const FOGEntityId OtherIdentityCopy =
        FOGEntityId::NewId();
    const FOGContentId IdentityA(
        TEXT("test:identity.alpha"));
    const FOGContentId IdentityB(
        TEXT("test:identity.beta"));

    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist older alpha Manifestation"),
        PersistUiManifestation(
            Store,
            OlderCopy,
            RulerId,
            IdentityA,
            0,
            10,
            Error));
    TestTrue(
        TEXT("Persist newer alpha Manifestation"),
        PersistUiManifestation(
            Store,
            NewerCopy,
            RulerId,
            IdentityA,
            1,
            20,
            Error));
    TestTrue(
        TEXT("Persist beta Manifestation"),
        PersistUiManifestation(
            Store,
            OtherIdentityCopy,
            RulerId,
            IdentityB,
            0,
            15,
            Error));

    FOGManifestationManagementMetadataRecord Management;
    Management.ManifestationId = OlderCopy;
    Management.bFavorite = true;
    Management.bProtected = true;
    Management.UpdatedWorldTick = 21;
    TestTrue(
        TEXT("Persist Favorite/Protected separately"),
        Store.UpsertManifestationManagementMetadata(
            Management,
            Error));

    FOGEntityRankStateRecord Rank;
    Rank.EntityId = OlderCopy;
    Rank.AttainedRankId =
        FOGContentId(TEXT("test:rank.saint"));
    Rank.AttainedLevel = 12;
    Rank.PeakRankId =
        Rank.AttainedRankId;
    Rank.PeakLevel = 12;
    Rank.UpdatedWorldTick = 21;
    TestTrue(
        TEXT("Persist normalized Rank projection source"),
        Store.UpsertEntityRankState(
            Rank,
            Error));

    FOGManifestationWorldFantasmStateRecord Fantasm;
    Fantasm.ManifestationId = OlderCopy;
    Fantasm.GradeId =
        FOGContentId(TEXT("test:fantasm.lr"));
    Fantasm.UnlockedWorldTick = 21;
    Fantasm.UpdatedWorldTick = 21;
    TestTrue(
        TEXT("Persist World Fantasm grade"),
        Store.UpsertManifestationWorldFantasmState(
            Fantasm,
            Error));

    FOGManifestationContextSelectionRecord Selection;
    Selection.OwnerEntityId = RulerId;
    Selection.ContextId =
        FOGUiViewModelService::CharacterLastUsedContextId(
            IdentityA);
    Selection.ManifestationId = OlderCopy;
    Selection.UpdatedWorldTick = 22;
    TestTrue(
        TEXT("Persist explicit last-used Manifestation"),
        Store.UpsertManifestationContextSelection(
            Selection,
            Error));

    FOGUiViewModelService Ui(
        Store);
    TArray<FOGRosterIdentityViewModel> Roster;
    TestTrue(
        TEXT("Build Identity-grouped roster"),
        Ui.BuildRoster(
            RulerId,
            Roster,
            Error));

    TestEqual(
        TEXT("Two Character Identities produce two roster cards"),
        Roster.Num(),
        2);

    const FOGRosterIdentityViewModel* Alpha =
        Roster.FindByPredicate(
            [&IdentityA](
                const FOGRosterIdentityViewModel& Candidate)
            {
                return Candidate.IdentityId ==
                    IdentityA;
            });
    TestNotNull(
        TEXT("Alpha Identity card exists"),
        Alpha);
    if (Alpha)
    {
        TestEqual(
            TEXT("Duplicate pulls remain grouped beneath one Identity card"),
            Alpha->ManifestationCount,
            2);
        TestTrue(
            TEXT("UI respects last-used copy instead of auto-selecting newest/strongest"),
            Alpha->SelectedManifestationId ==
                OlderCopy);
        TestTrue(
            TEXT("Favorite state remains available"),
            Alpha->bFavorite);
        TestTrue(
            TEXT("Protected state remains separate from Favorite"),
            Alpha->bProtected);

        const FOGRosterManifestationViewModel* Older =
            Alpha->Manifestations.FindByPredicate(
                [&OlderCopy](
                    const FOGRosterManifestationViewModel& Candidate)
                {
                    return Candidate.ManifestationId ==
                        OlderCopy;
                });
        TestNotNull(
            TEXT("Selected Manifestation detail exists"),
            Older);
        if (Older)
        {
            TestTrue(
                TEXT("Last-used marker is explicit"),
                Older->bLastUsed);
            TestTrue(
                TEXT("Rank is a resolved content identifier rather than formula"),
                Older->RankId ==
                    FOGContentId(TEXT("test:rank.saint")));
            TestTrue(
                TEXT("World Fantasm grade is projected compactly"),
                Older->WorldFantasmGradeId ==
                    FOGContentId(TEXT("test:fantasm.lr")));
        }
    }

    FOGCharacterIdentityDefinition AdultIdentity;
    AdultIdentity.IdentityId = IdentityA;
    AdultIdentity.DisplayNameKey =
        TEXT("test.character.alpha");
    AdultIdentity.CanonicalMaturity =
        EOGCanonicalMaturity::Adult;

    FOGCharacterIdentityViewModel Character;
    TestTrue(
        TEXT("Build Character Identity page"),
        Ui.BuildCharacterIdentity(
            RulerId,
            AdultIdentity,
            Character,
            Error));
    TestEqual(
        TEXT("Character page keeps four major bottom tabs"),
        Character.BottomTabs.Num(),
        4);
    TestTrue(
        TEXT("Canonically Adult Identity exposes Adult utility destination"),
        Character.bAdultUtilityVisible);
    TestFalse(
        TEXT("Internal formula decomposition remains hidden"),
        Character.bInternalFormulaBreakdownVisible);
    TestTrue(
        TEXT("Character page follows last-used Manifestation"),
        Character.SelectedManifestationId ==
            OlderCopy);

    AdultIdentity.CanonicalMaturity =
        EOGCanonicalMaturity::NonAdult;
    TestTrue(
        TEXT("Build nonadult Character page"),
        Ui.BuildCharacterIdentity(
            RulerId,
            AdultIdentity,
            Character,
            Error));
    TestFalse(
        TEXT("Nonadult Identity never receives Adult utility from appearance/runtime inference"),
        Character.bAdultUtilityVisible);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGKnowledgeLimitedWorldHudTest,
    "OfflineGame.UI.WorldMode.CompactHudRespectsKnowledgeAndUpperRightCompanions",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGKnowledgeLimitedWorldHudTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiProjectionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("world_hud.db"));
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
    const FOGEntityId CompanionA =
        FOGEntityId::NewId();
    const FOGEntityId CompanionB =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId EnemyId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist companion A"),
        PersistUiEntity(
            Store,
            CompanionA,
            FName(TEXT("manifestation")),
            Error));
    TestTrue(
        TEXT("Persist companion B"),
        PersistUiEntity(
            Store,
            CompanionB,
            FName(TEXT("manifestation")),
            Error));

    FOGLocationRecord Location;
    Location.LocationId = LocationId;
    Location.Kind =
        FName(TEXT("ruin"));
    TestTrue(
        TEXT("Persist physical location truth"),
        Store.UpsertLocation(
            Location,
            10,
            Error));

    FOGSharedWorldStateService SharedWorld(
        Store);
    FOGWorldPresenceRecord Presence;
    Presence.EntityId = RulerId;
    Presence.LocationId = LocationId;
    Presence.MovementContext =
        FName(TEXT("ground"));
    Presence.UpdatedWorldTick = 20;
    TestTrue(
        TEXT("Persist physical presence"),
        SharedWorld.UpdatePhysicalPresence(
            Presence,
            Error));

    FOGUiViewModelService Ui(
        Store);
    FOGWorldHudViewModel Hud;

    TestTrue(
        TEXT("Build HUD before map knowledge exists"),
        Ui.BuildWorldHud(
            RulerId,
            RulerId,
            {CompanionA, CompanionB},
            {CompanionB},
            nullptr,
            false,
            Hud,
            Error));
    TestTrue(
        TEXT("World HUD remains compact/contextual"),
        Hud.bCompactContextualHud);
    TestFalse(
        TEXT("World HUD is not a permanent MMO overlay"),
        Hud.bPersistentMmoOverlay);
    TestEqual(
        TEXT("Companions are placed upper-right"),
        Hud.CompanionPlacement,
        FName(TEXT("upper_right")));
    TestFalse(
        TEXT("Physical truth alone does not reveal minimap knowledge"),
        Hud.bShowMinimap);
    TestEqual(
        TEXT("Immediate party supports two companions"),
        Hud.Companions.Num(),
        2);
    TestTrue(
        TEXT("QTE readiness is contextual per companion"),
        Hud.Companions[1].bQteReady);

    TestTrue(
        TEXT("Acquire observed location knowledge"),
        SharedWorld.RecordLocationDiscovery(
            RulerId,
            LocationId,
            EOGLocationKnowledgeLevel::Observed,
            21,
            Error));

    FOGKnowledgeFactRecord Rumor;
    Rumor.OwnerEntityId = RulerId;
    Rumor.FactKey =
        FName(TEXT("target_condition"));
    Rumor.SubjectEntityId = EnemyId;
    Rumor.ValueJson =
        TEXT("{\"condition\":\"wounded\"}");
    Rumor.LearnedWorldTick = 22;
    Rumor.UpdatedWorldTick = 22;
    Rumor.BeliefState =
        FName(TEXT("rumor"));
    Rumor.ConfidenceBps = 3000;

    TestTrue(
        TEXT("Build HUD with rumor-level enemy information"),
        Ui.BuildWorldHud(
            RulerId,
            RulerId,
            {CompanionA, CompanionB},
            {CompanionB},
            &Rumor,
            false,
            Hud,
            Error));
    TestTrue(
        TEXT("Known location can appear on minimap"),
        Hud.bShowMinimap);
    TestFalse(
        TEXT("Rumor does not reveal exact enemy state"),
        Hud.bShowExactEnemyState);
    TestEqual(
        TEXT("Rumor is visibly distinguished"),
        Hud.TargetCondition.KnowledgeState,
        EOGUiKnowledgeState::Rumor);

    Rumor.BeliefState =
        FName(TEXT("confirmed"));
    Rumor.ConfidenceBps = 10000;
    TestTrue(
        TEXT("Build HUD with confirmed enemy condition"),
        Ui.BuildWorldHud(
            RulerId,
            RulerId,
            {CompanionA, CompanionB},
            {},
            &Rumor,
            false,
            Hud,
            Error));
    TestTrue(
        TEXT("Confirmed knowledge may reveal exact enemy condition"),
        Hud.bShowExactEnemyState);

    const FOGKnowledgeFactViewModel Outdated =
        FOGUiViewModelService::ProjectKnowledgeFact(
            &Rumor,
            true);
    TestEqual(
        TEXT("Outdated intelligence is explicitly marked rather than treated as truth"),
        Outdated.KnowledgeState,
        EOGUiKnowledgeState::Outdated);
    TestFalse(
        TEXT("Outdated intelligence loses exact-value privilege"),
        Outdated.bExactValueVisible);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGachaTerritoryOpeningAndNumberProjectionTest,
    "OfflineGame.UI.Projections.GachaTerritoryOpeningAndLargeNumbersFollowFreeze",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGachaTerritoryOpeningAndNumberProjectionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiProjectionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("projection_ui.db"));
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
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId ClaimId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));

    FOGRulerGachaAccessRecord Access;
    Access.RulerId = RulerId;
    Access.bPermanentlyUnlocked = true;
    Access.UpdatedWorldTick = 20;
    TestTrue(
        TEXT("Persist gacha unlock"),
        Store.UpsertRulerGachaAccess(
            Access,
            Error));

    const FOGContentId CurrencyId(
        TEXT("test:currency.pull"));
    TestTrue(
        TEXT("Persist pull currency"),
        Store.SetResourceBalance(
            RulerId,
            CurrencyId,
            975,
            Error));

    FOGGachaStateRecord GachaState;
    GachaState.RulerId = RulerId;
    GachaState.PityCategory =
        FName(TEXT("limited"));
    GachaState.PullsSinceTopRarity = 52;
    GachaState.bFeaturedGuarantee = true;
    GachaState.TotalPulls = 100;
    GachaState.UpdatedWorldTick = 20;
    TestTrue(
        TEXT("Persist pity/guarantee state"),
        Store.UpsertGachaState(
            GachaState,
            Error));

    const FOGContentId TicketIdForBalance(
        TEXT("test:ticket.limited"));
    TestTrue(
        TEXT("Persist compatible ticket balance"),
        Store.SetResourceBalance(
            RulerId,
            TicketIdForBalance,
            2,
            Error));

    FOGGachaBannerDefinition Banner;
    Banner.BannerId =
        FOGContentId(TEXT("test:banner.limited"));
    Banner.PityCategory =
        FName(TEXT("limited"));
    Banner.CurrencyId =
        CurrencyId;
    Banner.PullCost = 100;
    const FOGContentId TicketId(
        TEXT("test:ticket.limited"));
    Banner.CompatibleTicketIds.Add(
        TicketId);
    Banner.TopRarity =
        FName(TEXT("ur"));
    Banner.SoftPityStart = 50;
    Banner.SoftPityBonusPerPullBps = 500;
    Banner.HardPity = 70;

    FOGGachaPoolEntry Featured;
    Featured.IdentityId =
        FOGContentId(TEXT("test:identity.featured"));
    Featured.VersionId =
        FOGContentId(TEXT("test:version.featured"));
    Featured.Rarity =
        FName(TEXT("ur"));
    Featured.Weight = 12;
    Featured.bFeatured = true;
    Banner.Entries.Add(
        Featured);

    FOGGachaPoolEntry Other;
    Other.IdentityId =
        FOGContentId(TEXT("test:identity.other"));
    Other.VersionId =
        FOGContentId(TEXT("test:version.other"));
    Other.Rarity =
        FName(TEXT("sr"));
    Other.Weight = 88;
    Banner.Entries.Add(
        Other);

    FOGUiViewModelService Ui(
        Store);
    FOGGachaViewModel Gacha;
    TestTrue(
        TEXT("Build Gacha view model"),
        Ui.BuildGacha(
            RulerId,
            Banner,
            Gacha,
            Error));
    TestTrue(
        TEXT("Gacha page exposes permanent unlock"),
        Gacha.bUnlocked);
    TestEqual(
        TEXT("Pity count remains visible"),
        Gacha.PityCount,
        52);
    TestTrue(
        TEXT("Featured guarantee remains visible"),
        Gacha.bFeaturedGuarantee);
    TestEqual(
        TEXT("Currency remains visible"),
        Gacha.CurrencyBalance,
        static_cast<int64>(975));
    TestEqual(
        TEXT("Compatible ticket balance is visible"),
        Gacha.CompatibleTickets.Num(),
        1);
    TestTrue(
        TEXT("UI indicates ticket-first consumption when ticket is available"),
        Gacha.bWillUseTicketFirst &&
        Gacha.CompatibleTickets[0].bWillConsumeBeforeCurrency &&
        Gacha.CompatibleTickets[0].Balance == 2);
    TestEqual(
        TEXT("Details exposes exact declared base pool rows"),
        Gacha.BasePool.Num(),
        2);

    FOGLocationRecord Location;
    Location.LocationId = LocationId;
    Location.Kind =
        FName(TEXT("region"));
    TestTrue(
        TEXT("Persist Territory root location"),
        Store.UpsertLocation(
            Location,
            0,
            Error));

    FOGTerritoryRecord Territory;
    Territory.TerritoryId = TerritoryId;
    Territory.RulerId = RulerId;
    Territory.RootLocationId = LocationId;
    Territory.bMainTerritory = true;
    Territory.ControlState =
        FName(TEXT("controlled"));
    TestTrue(
        TEXT("Persist Territory"),
        Store.UpsertTerritory(
            Territory,
            0,
            Error));

    FOGTerritoryClaimRecord Claim;
    Claim.ClaimId = ClaimId;
    Claim.TerritoryId = TerritoryId;
    Claim.RulerId = RulerId;
    Claim.ControlState =
        FName(TEXT("controlled"));
    Claim.ControlStrengthBps = 10000;
    Claim.ClaimStartWorldTick = 0;
    Claim.bHasEffectiveControlStart = true;
    Claim.EffectiveControlStartWorldTick = 0;
    Claim.UpdatedWorldTick = 20;
    TestTrue(
        TEXT("Persist authoritative Territory claim"),
        Store.UpsertTerritoryClaim(
            Claim,
            0,
            Error));

    FOGTerritoryViewModel TerritoryView;
    TestTrue(
        TEXT("Build map-first Territory projection"),
        Ui.BuildTerritory(
            RulerId,
            TerritoryId,
            TerritoryView,
            Error));
    TestTrue(
        TEXT("Territory destination remains map-first"),
        TerritoryView.bMapFirst);
    TestEqual(
        TEXT("Territory quick sections preserve strategic grammar"),
        TerritoryView.QuickSections.Num(),
        6);
    TestEqual(
        TEXT("Only player-facing Territory summaries are projected"),
        TerritoryView.Territories.Num(),
        1);

    const FOGOpeningViewModel Opening =
        FOGUiViewModelService::BuildOpening(
            true,
            true,
            true,
            true);
    TestTrue(
        TEXT("Continue is available when canonical world exists"),
        Opening.bContinueAvailable);
    TestTrue(
        TEXT("Recovery is a primary opening action when recoverable world exists"),
        Opening.bRecoverExistingWorldAvailable);
    TestEqual(
        TEXT("Opening keeps Continue/Recover/Import/Settings grammar"),
        Opening.PrimaryActions.Num(),
        4);
    TestTrue(
        TEXT("Reduced-motion title presentation is represented"),
        Opening.bReducedMotionPresentation);
    TestTrue(
        TEXT("Clear World remains secondary and confirmed"),
        Opening.bClearWorldRequiresConfirmation);

    const FString Billion =
        FOGUiNumberFormatter::Format(
            FOGLargeNumber(
                100000000,
                1),
            true,
            false);
    TestEqual(
        TEXT("Large player-facing values use suffix notation"),
        Billion,
        FString(TEXT("1B")));
    TestFalse(
        TEXT("Large player-facing values never use scientific notation"),
        Billion.Contains(TEXT("e")) ||
        Billion.Contains(TEXT("E")) ||
        Billion.Contains(TEXT("10^")));

    TestEqual(
        TEXT("Knowledge-hidden values display Unknown"),
        FOGUiNumberFormatter::Format(
            FOGLargeNumber::FromInt64(100),
            false,
            true),
        FString(TEXT("Unknown")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
