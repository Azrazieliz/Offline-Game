#include "Combat/OGDiagnosticEncounter.h"
#include "Combat/OGDiagnosticGacha.h"
#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDiagnosticTurnRoundTripTest,
    "OfflineGame.Diagnostic.CanonicalTurnRoundTrip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDiagnosticTurnRoundTripTest::RunTest(const FString& Parameters)
{
    FOGCombatUnitState Player = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 0);
    FOGCombatUnitState Companion = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 1);
    Companion.Presence = EOGCombatPresence::Reserve; // World input has one controlled unit.
    FOGCombatUnitState Enemy = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 3, 1);
    Enemy.Stats.Attack = FOGLargeNumber::FromInt64(1);
    FOGDiagnosticEncounter Session;
    FString Error;
    TestTrue(TEXT("Start canonical encounter"), Session.Start({Player, Companion}, {Enemy}, 77, {}, Error));
    TArray<FOGCombatUnitState> Returned;
    TestFalse(TEXT("Return blocked while battle runs"), Session.CollectReturnSnapshots(Returned, Error));
    const FOGEntityId InvalidTarget = FOGEntityId::NewId();
    if (Session.IsWaitingForPlayer())
        TestFalse(TEXT("Invalid target does not advance turn"), Session.SubmitPlayerAction(EOGDiagnosticCommand::Basic, InvalidTarget, Error));
    int32 Steps = 0;
    while (Session.GetState().Status == EOGTurnBattleStatus::Running && ++Steps <= 50)
    {
        // A basic is always legal; this test probes round-trip authority rather
        // than duplicating the timeline selector/resource implementation.
        const bool bResolved = Session.IsWaitingForPlayer()
            ? Session.SubmitPlayerAction(EOGDiagnosticCommand::Basic, Enemy.UnitEntityId, Error)
            : Session.StepEnemy(Error);
        if (!TestTrue(TEXT("Canonical turn resolves"), bResolved)) return false;
    }
    TestTrue(TEXT("Battle completes within diagnostic bound"), Steps <= 50);
    TestEqual(TEXT("Victory is from canonical completion"), Session.GetOutcome(), EOGDiagnosticOutcome::Victory);
    TestTrue(TEXT("Return snapshot available"), Session.CollectReturnSnapshots(Returned, Error));
    TestEqual(TEXT("Party size preserved"), Returned.Num(), 2);
    if (Returned.Num() == 2)
    {
        TestTrue(TEXT("Protagonist entity retained"), Returned[0].UnitEntityId == Player.UnitEntityId);
        TestEqual(TEXT("Companion returns to original World presence"), Returned[1].Presence, EOGCombatPresence::Reserve);
        TestTrue(TEXT("Kit retained"), Returned[1].SkillSet.UltimateSkill == Companion.SkillSet.UltimateSkill);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDiagnosticGachaHistoryTest,
    "OfflineGame.Diagnostic.EarnedGachaHistoryAndTicketCurrency",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDiagnosticGachaHistoryTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FOGSQLiteWorldStore Store;
    FString Error;
    if (!TestTrue(TEXT("Open isolated fixture DB"), Store.Open(FPaths::Combine(Directory, TEXT("diagnostic.db")), Error))) return false;
    const FOGEntityId Ruler = OGDiagnosticGacha::ScopedId(1);
    const FOGGachaBannerDefinition Banner = OGDiagnosticGacha::MakeBanner();
    TestTrue(TEXT("Banner valid"), FOGGachaService::ValidateBanner(Banner, Error));
    FOGGachaPullResult Pull;
    FOGGachaService Gacha(Store);
    TestFalse(TEXT("Money/content cannot bypass missing earned access"),
        Gacha.Pull(Banner, Ruler, 0, 42, Pull, Error));
    TestTrue(TEXT("Fresh scoped claim starts at entry tick"), OGDiagnosticGacha::EnsureScopedFixture(Store, 0, Error));
    TestTrue(TEXT("Exactly one month remains locked"), OGDiagnosticGacha::EnsureScopedFixture(Store, 30, Error));
    bool bAllowed = false;
    FOGRulerGachaAccessService AccessGate(Store);
    TestTrue(TEXT("Read boundary gate"), AccessGate.CanUseGacha(Ruler, 30, bAllowed, Error));
    TestFalse(TEXT("Strict boundary is locked"), bAllowed);
    TestTrue(TEXT("Actually elapsed 31 ticks qualifies"), OGDiagnosticGacha::EnsureScopedFixture(Store, 31, Error));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!TestTrue(TEXT("Canonical pull succeeds"), Gacha.Pull(Banner, Ruler,
            OGDiagnosticGacha::EarnedAccessTick + Index, 42 + Index, Pull, Error))) break;
        TestEqual(TEXT("Ticket priority then currency fallback"), Pull.bUsedTicket, Index < 2);
        bool bFound = false;
        FOGCharacterManifestationRecord Manifestation;
        TestTrue(TEXT("Read result manifestation"), Store.TryReadCharacterManifestation(Pull.ManifestationId, bFound, Manifestation, Error));
        TestTrue(TEXT("Acquisition does not silently anchor/spawn"), bFound && !Manifestation.WorldModeAnchorTerritoryId.IsValid());
    }
    FOGGachaViewModel View;
    TArray<FOGGachaHistoryEntryViewModel> History;
    TArray<FOGRosterIdentityViewModel> Roster;
    TestTrue(TEXT("Project canonical gacha/history/roster"), OGDiagnosticGacha::Project(Store, Ruler, Banner, View, History, Roster, Error));
    TestEqual(TEXT("Three history entries"), History.Num(), 3);
    TestTrue(TEXT("Roster is populated from Manifestations"), !Roster.IsEmpty());
    bool bKnown = false;
    int64 Balance = 0;
    TestTrue(TEXT("Read remaining currency"), Store.TryReadResourceBalance(Ruler, Banner.CurrencyId, bKnown, Balance, Error));
    TestTrue(TEXT("Currency charged once after two tickets"), bKnown && Balance == 1700);
    FOGTerritoryControlService TerritoryControl(Store);
    FOGEntityId Anchor;
    TestTrue(TEXT("Explicit deployment in controlled Territory anchors a selected copy"),
        TerritoryControl.AnchorManifestationForWorldMode(Ruler, Pull.ManifestationId,
            OGDiagnosticGacha::EarnedAccessTick + 3, Anchor, Error));
    TestTrue(TEXT("Physical anchor is concrete"), Anchor.IsValid());
    TArray<FOGTerritoryClaimRecord> Claims;
    TestTrue(TEXT("Read authored claim history"), Store.ListTerritoryClaimsByRuler(Ruler, Claims, Error));
    if (Claims.Num() == 1)
    {
        TestTrue(TEXT("Displace with explicit five-day calendar deadline"),
            TerritoryControl.BeginDisplacement(Claims[0].ClaimId, 40, 45, Error));
        bool bExpired = false;
        TestTrue(TEXT("Process loss after grace"), TerritoryControl.ExpireReclamationIfDue(Claims[0].ClaimId, 46, bExpired, Error));
        bool bCanUse = false;
        FOGRulerGachaAccessService Access(Store);
        TestTrue(TEXT("Check earned permanent access after Territory loss"), Access.CanUseGacha(Ruler, 46, bCanUse, Error));
        TestTrue(TEXT("Loss does not revoke permanent unlock"), bCanUse);
    }
    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDiagnosticTurnDefeatTest,
    "OfflineGame.Diagnostic.CanonicalTurnDefeatAndReturn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDiagnosticTurnDefeatTest::RunTest(const FString& Parameters)
{
    FOGCombatUnitState Player = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 0);
    FOGCombatUnitState Enemy = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 3, 1);
    Enemy.DefaultActionDelay = 1;
    Enemy.Stats.Attack = FOGLargeNumber::FromInt64(100000);
    FOGDiagnosticEncounter Session;
    FString Error;
    TestTrue(TEXT("Start encounter with canonical faster enemy"), Session.Start({Player}, {Enemy}, 99, {}, Error));
    TestFalse(TEXT("Player cannot act before enemy"), Session.SubmitPlayerAction(EOGDiagnosticCommand::Ultimate, Enemy.UnitEntityId, Error));
    TestTrue(TEXT("Enemy action damages and finishes action window"), Session.StepEnemy(Error));
    TestEqual(TEXT("Canonical result is defeat"), Session.GetOutcome(), EOGDiagnosticOutcome::Defeat);
    TArray<FOGCombatUnitState> Returned;
    TestTrue(TEXT("Defeat has World return result"), Session.CollectReturnSnapshots(Returned, Error));
    if (Returned.Num() == 1)
    {
        TestTrue(TEXT("Defeat HP is zero"), Returned[0].CurrentHp.IsZero());
        TestEqual(TEXT("Defeat presence returned"), Returned[0].Presence, EOGCombatPresence::Defeated);
    }
    TestFalse(TEXT("Enemy cannot keep acting after completion"), Session.StepEnemy(Error));
    return true;
}
#endif
