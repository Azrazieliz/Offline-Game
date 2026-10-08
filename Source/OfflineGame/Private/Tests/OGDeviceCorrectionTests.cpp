#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGDiagnosticEncounter.h"
#include "Combat/OGDiagnosticGacha.h"
#include "Combat/OGDiagnosticHumanoid.h"
#include "Combat/OGDiagnosticEnemyStateComponent.h"
#include "Animation/OGDiagnosticAnimationPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Gacha/OGGachaService.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDeviceCompanionRecoveryTest,
    "OfflineGame.Foundation.DeviceCorrections.LivingAssignmentRecoversDefeatedParty",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDeviceCompanionRecoveryTest::RunTest(const FString& Parameters)
{
    auto* Party = NewObject<UOGWorldPartyRuntimeComponent>();
    TArray<FOGCombatUnitState> Units;
    for (int32 Index = 0; Index < 3; ++Index)
        Units.Add(OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), Index));
    FString Error;
    if (!TestTrue(TEXT("Create canonical party"), Party->ConfigureParty(Units, Error))) return false;
    for (int32 Index = 0; Index < 3; ++Index)
        TestTrue(TEXT("Defeat original party"), Party->ApplyResolvedHp(Index, Units[Index].UnitEntityId, FOGLargeNumber(), Error));
    TestEqual(TEXT("No controlled member after complete defeat"), Party->GetControlledSlot(), INDEX_NONE);
    const auto Replacement = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 1);
    TestTrue(TEXT("Assign a different living owned companion"), Party->AssignResolvedCompanion(1, Replacement, Error));
    TestEqual(TEXT("Living assignment restores control"), Party->GetControlledSlot(), 1);
    TestTrue(TEXT("Replacement is active"), Party->GetSlots()[1].Unit.Presence == EOGCombatPresence::Active);
    TestTrue(TEXT("Original protagonist stays defeated"), Party->GetSlots()[0].bDefeated && Party->GetSlots()[0].Unit.CurrentHp.IsZero());
    TestTrue(TEXT("Other original companion stays defeated"), Party->GetSlots()[2].bDefeated && Party->GetSlots()[2].Unit.CurrentHp.IsZero());
    TestTrue(TEXT("Replacement retains resolved HP"), Party->GetSlots()[1].Unit.CurrentHp == Replacement.CurrentHp);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDeviceCorpseDriverTest,
    "OfflineGame.Foundation.DeviceCorrections.CorpseHasOneVisiblePresentationDriver",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOGDeviceCorpseDriverTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr, true,
        ERHIFeatureLevel::SM5, &Init, false);
    if (!TestNotNull(TEXT("Isolated physics world"), World)) return false;
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Enemy = World->SpawnActor<AOGDiagnosticHumanoid>();
    auto* Animation = Enemy->FindComponentByClass<UOGDiagnosticAnimationPresentation>();
    auto* State = Enemy->FindComponentByClass<UOGDiagnosticEnemyStateComponent>();
    const auto Unit = OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 0);
    State->Initialize(Unit);
    Animation->BindMesh(Enemy->GetMesh());
    TestNotNull(TEXT("Diagnostic ragdoll physics asset"), Enemy->GetMesh()->GetPhysicsAsset());
    FString Error;
    TestTrue(TEXT("Real resolved lethal damage"), State->ApplyResolvedDamage(Unit.CurrentHp, Error));
    TestFalse(TEXT("Canonical enemy is dead"), State->GetSnapshot().IsAlive());
    TestTrue(TEXT("Physical corpse is visible"), Enemy->GetMesh()->IsVisible());
    TInlineComponentArray<UPoseableMeshComponent*> Proxies;
    Enemy->GetComponents(Proxies);
    for (const auto* Proxy : Proxies)
        TestFalse(TEXT("Procedural proxy cannot drive the ragdoll corpse"), Proxy->IsVisible());
    TestTrue(TEXT("Ragdoll owns physical corpse"), Enemy->GetMesh()->IsSimulatingPhysics());
    World->DestroyWorld(true);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDeviceTenPullAtomicTest,
    "OfflineGame.Foundation.DeviceCorrections.TenPullUsesCanonicalAtomicAcquisition",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDeviceTenPullAtomicTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FOGSQLiteWorldStore Store;
    FString Error;
    if (!TestTrue(TEXT("Open isolated canonical store"), Store.Open(FPaths::Combine(Directory, TEXT("batch.db")), Error))) return false;
    const FOGEntityId Owner = FOGEntityId::NewId();
    FOGGachaBannerDefinition Banner;
    Banner.BannerId = FOGContentId(TEXT("test:banner.batch"));
    Banner.PityCategory = TEXT("batch");
    Banner.CurrencyId = FOGContentId(TEXT("test:currency.batch"));
    const FOGContentId Ticket(TEXT("test:ticket.batch"));
    Banner.CompatibleTicketIds.Add(Ticket);
    Banner.PullCost = 100;
    Banner.TopRarity = TEXT("UR");
    Banner.HardPity = 1;
    FOGGachaPoolEntry Entry;
    Entry.IdentityId = FOGContentId(TEXT("test:identity.batch"));
    Entry.VersionId = FOGContentId(TEXT("test:version.batch"));
    Entry.Rarity = TEXT("UR"); Entry.Weight = 100; Entry.bFeatured = true;
    Banner.Entries.Add(Entry);
    TestTrue(TEXT("Seed ruler"), Store.UpsertEntity(Owner, TEXT("ruler"), 0, TEXT("{}"), Error));
    FOGRulerGachaAccessRecord Access;
    Access.RulerId = Owner; Access.bPermanentlyUnlocked = true;
    Access.bHasUnlockedWorldTick = true;
    TestTrue(TEXT("Seed earned access"), Store.UpsertRulerGachaAccess(Access, Error));
    TestTrue(TEXT("Seed two tickets"), Store.SetResourceBalance(Owner, Ticket, 2, Error));
    TestTrue(TEXT("Seed insufficient money for final pull"), Store.SetResourceBalance(Owner, Banner.CurrencyId, 700, Error));
    FOGGachaService Service(Store);
    TArray<FOGGachaPullResult> Results;
    TestFalse(TEXT("Insufficient ten-pull fails as one purchase"), Service.PullBatch(Banner, Owner, 10, 101, 10, Results, Error));
    TestEqual(TEXT("Failure publishes no partial results"), Results.Num(), 0);
    bool Found = false; int64 Balance = 0;
    TestTrue(TEXT("Read unchanged currency"), Store.TryReadResourceBalance(Owner, Banner.CurrencyId, Found, Balance, Error));
    TestEqual(TEXT("Rollback restores money"), Balance, int64(700));
    TestTrue(TEXT("Read unchanged tickets"), Store.TryReadResourceBalance(Owner, Ticket, Found, Balance, Error));
    TestEqual(TEXT("Rollback restores tickets"), Balance, int64(2));
    TArray<FOGCharacterManifestationRecord> Copies;
    TestTrue(TEXT("Read acquisitions after failure"), Store.ListCharacterManifestationsByOwnerAndIdentity(Owner, Entry.IdentityId, Copies, Error));
    TestEqual(TEXT("Rollback leaves no acquisitions"), Copies.Num(), 0);
    FOGGachaStateRecord Pity;
    TestTrue(TEXT("Read pity after failure"), Store.TryReadGachaState(Owner, Banner.PityCategory, Found, Pity, Error));
    TestFalse(TEXT("Rollback leaves no new pity state"), Found);
    TestTrue(TEXT("Fund complete purchase"), Store.SetResourceBalance(Owner, Banner.CurrencyId, 800, Error));
    if (TestTrue(TEXT("Ten canonical acquisitions succeed"), Service.PullBatch(Banner, Owner, 20, 101, 10, Results, Error)))
    {
        TestEqual(TEXT("Exactly ten results"), Results.Num(), 10);
        TSet<FOGEntityId> Ids;
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            Ids.Add(Results[Index].ManifestationId);
            TestEqual(TEXT("Each pull advances persistent total"), Results[Index].UpdatedState.TotalPulls, int64(Index + 1));
            TestEqual(TEXT("Tickets consumed before money"), Results[Index].bUsedTicket, Index < 2);
            TestEqual(TEXT("Duplicate identities retain distinct acquisitions"), Results[Index].bDuplicateIdentity, Index > 0);
        }
        TestEqual(TEXT("Ten distinct manifestation IDs"), Ids.Num(), 10);
    }
    TestTrue(TEXT("Read spent currency"), Store.TryReadResourceBalance(Owner, Banner.CurrencyId, Found, Balance, Error));
    TestEqual(TEXT("Exactly eight currency payments"), Balance, int64(0));
    TestTrue(TEXT("Read spent tickets"), Store.TryReadResourceBalance(Owner, Ticket, Found, Balance, Error));
    TestEqual(TEXT("Exactly two ticket payments"), Balance, int64(0));
    TestTrue(TEXT("Read complete acquisition list"), Store.ListCharacterManifestationsByOwnerAndIdentity(Owner, Entry.IdentityId, Copies, Error));
    TestEqual(TEXT("Ten persisted copies"), Copies.Num(), 10);
    TestTrue(TEXT("Read committed pity"), Store.TryReadGachaState(Owner, Banner.PityCategory, Found, Pity, Error));
    TestTrue(TEXT("Pity is persisted"), Found);
    TestEqual(TEXT("Committed total is ten"), Pity.TotalPulls, int64(10));
    TestEqual(TEXT("Top rarity resets pity"), Pity.PullsSinceTopRarity, 0);
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "World/OGStartingRegionPresentation.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"

// Development-only visual verification uses the same commands a player taps.
// Normal capture does not mutate canonical data. The optional exercise flag
// uses only the existing diagnostic training owner to verify an actual ten-pull.
static FAutoConsoleCommandWithWorldAndArgs OGDeviceHudCaptureCommand(
    TEXT("OG.Tests.CaptureHud"),
    TEXT("Capture the existing HUD destinations to an absolute directory."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
        [](const TArray<FString>& Args, UWorld* World)
        {
            if (!World || Args.Num() < 1 || Args.Num() > 2) return;
            const bool bExerciseGacha = Args.Num() == 2 && Args[1] == TEXT("exercise-training-gacha");
            const FString Directory = Args[0];
            IFileManager::Get().MakeDirectory(*Directory, true);
            TWeakObjectPtr<UWorld> WeakWorld(World);
            auto Step = MakeShared<int32>(0);
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakWorld, Directory, Step, bExerciseGacha](float)
                {
                    if (!WeakWorld.IsValid()) return false;
                    APlayerController* PC = WeakWorld->GetFirstPlayerController();
                    AOGWorldPresentationHud* Hud = PC ? Cast<AOGWorldPresentationHud>(PC->GetHUD()) : nullptr;
                    if (!Hud) return true;
                    // The authored training scenario already binds its owner and banner.
                    // Reassigning the same owner here clears that valid command context.
                    static const TCHAR* Actions[] = {
                        TEXT(""), TEXT("OG.Opening.Ruler"), TEXT("OG.Ruler.characters"),
                        TEXT("OG.Ruler.gacha"), TEXT("OG.Gacha.Pull10"), TEXT("OG.Ruler.territory"), TEXT("OG.Ruler.records"),
                        TEXT("OG.Settings.Toggle"), TEXT("OG.Settings.Toggle"),
                        TEXT("OG.Ruler.World"), TEXT("OG.Pause.Toggle"), TEXT("OG.Pause.Resume")
                    };
                    static const TCHAR* Labels[] = {
                        TEXT("opening"), TEXT("ruler-home"), TEXT("characters"),
                        TEXT("gacha"), TEXT("gacha-results"), TEXT("territory"), TEXT("records"),
                        TEXT("settings"), TEXT("ruler-return"), TEXT("world"),
                        TEXT("pause"), TEXT("resumed")
                    };
                    if (*Step >= UE_ARRAY_COUNT(Actions) * 2)
                    {
                        FPlatformMisc::RequestExit(false);
                        return false;
                    }
                    if (*Step % 2 == 0)
                    {
                        const int32 Index = *Step / 2;
                        if (Index >= UE_ARRAY_COUNT(Actions))
                        {
                            FPlatformMisc::RequestExit(false);
                            return false;
                        }
                        if (Actions[Index][0] && (Index != 4 || bExerciseGacha))
                            Hud->NotifyHitBoxClick(FName(Actions[Index]));
                    }
                    else
                    {
                        const int32 Index = *Step / 2;
                        FScreenshotRequest::RequestScreenshot(
                            FPaths::Combine(Directory, FString(Labels[Index]) + TEXT(".png")), true, false);
                    }
                    ++*Step;
                    return true;
                }), 2.0f);
        }));
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/HUDHitBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGReadableSettingsTargetsTest,
    "OfflineGame.Foundation.DeviceCorrections.SettingsTargetsContainReadableLabels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOGReadableSettingsTargetsTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr, true,
        ERHIFeatureLevel::SM5, &Init, false);
    if (!TestNotNull(TEXT("HUD verification world"), World)) return false;
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<APlayerController>();
    auto* Pawn = World->SpawnActor<AOGWorldPrototypeCharacter>();
    PC->Possess(Pawn);
    Pawn->SetFoundationSettingsOpen(true);
    for (const FIntPoint Size : {FIntPoint(1080, 2340), FIntPoint(2340, 1080)})
    {
        FCanvas Drawing(nullptr, nullptr, World, ERHIFeatureLevel::SM5);
        auto* Canvas = NewObject<UCanvas>();
        Canvas->Init(Size.X, Size.Y, nullptr, &Drawing);
        auto* Hud = World->SpawnActor<AOGWorldPresentationHud>();
        Hud->SetOwner(PC); Hud->PlayerOwner = PC; Hud->SetCanvas(Canvas, Canvas);
        Hud->DrawHUD();
        for (const FName Name : {FName(TEXT("OG.Settings.Page.Accessibility")),
            FName(TEXT("OG.Settings.CameraH.Plus"))})
        {
            const FHUDHitBox* Box = Hud->GetHitBoxWithName(Name);
            if (!TestNotNull(*FString::Printf(TEXT("Visible settings target %s"), *Name.ToString()), Box)) continue;
            FVector2D Inside = FVector2D::ZeroVector;
            bool Found = false;
            for (int32 Y = 0; Y < Size.Y && !Found; Y += 4)
                for (int32 X = 0; X < Size.X && !Found; X += 4)
                    if (Box->Contains(FVector2D(X, Y))) {Inside = FVector2D(X,Y); Found = true;}
            TestTrue(TEXT("Target is inside the viewport"), Found);
            if (!Found) continue;
            float Top = Inside.Y, Bottom = Inside.Y;
            while (Top > 0 && Box->Contains(FVector2D(Inside.X, Top - 1))) --Top;
            while (Bottom < Size.Y && Box->Contains(FVector2D(Inside.X, Bottom + 1))) ++Bottom;
            const float ReadableHeight = FMath::Min(Size.X, Size.Y) / 27.0f;
            TestTrue(*FString::Printf(TEXT("%s target contains readable text and padding at %dx%d"),
                *Name.ToString(), Size.X, Size.Y), Bottom - Top >= ReadableHeight + 12.0f);
        }
        Hud->SetCanvas(nullptr, nullptr); Hud->Destroy();
    }
    PC->UnPossess(); Pawn->Destroy(); PC->Destroy(); World->DestroyWorld(true);
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDistinctActionTargetsTest,
    "OfflineGame.Foundation.DeviceCorrections.WorldActionTargetsRemainDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOGDistinctActionTargetsTest::RunTest(const FString&)
{
    UWorld::InitializationValues Init;
    Init.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr, true,
        ERHIFeatureLevel::SM5, &Init, false);
    if (!TestNotNull(TEXT("Action HUD verification world"), World)) return false;
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<APlayerController>();
    auto* Pawn = World->SpawnActor<AOGWorldPrototypeCharacter>();
    PC->Possess(Pawn);
    for (const FIntPoint Size : {FIntPoint(1080, 2340), FIntPoint(2340, 1080)})
    {
        FCanvas Drawing(nullptr, nullptr, World, ERHIFeatureLevel::SM5);
        auto* Canvas = NewObject<UCanvas>();
        Canvas->Init(Size.X, Size.Y, nullptr, &Drawing);
        auto* Hud = World->SpawnActor<AOGWorldPresentationHud>();
        Hud->SetOwner(PC); Hud->PlayerOwner = PC; Hud->SetCanvas(Canvas, Canvas);
        Hud->NotifyHitBoxClick(TEXT("OG.Opening.Continue"));
        Hud->DrawHUD();
        TArray<FBox2D> Rectangles;
        for (const FName Name : {FName(TEXT("OG.World.Attack")), FName(TEXT("OG.World.Dodge")),
            FName(TEXT("OG.World.Jump")), FName(TEXT("OG.World.Lock")), FName(TEXT("OG.World.Sprint"))})
        {
            const FHUDHitBox* Box = Hud->GetHitBoxWithName(Name);
            if (!TestNotNull(*Name.ToString(), Box)) continue;
            FVector2D Inside = FVector2D::ZeroVector; bool Found = false;
            for (int32 Y = 0; Y < Size.Y && !Found; Y += 4)
                for (int32 X = 0; X < Size.X && !Found; X += 4)
                    if (Box->Contains(FVector2D(X,Y))) {Inside=FVector2D(X,Y); Found=true;}
            TestTrue(TEXT("Action target is inside viewport"), Found);
            if (!Found) continue;
            FVector2D Low=Inside, High=Inside;
            while (Low.X > 0 && Box->Contains(FVector2D(Low.X-1,Inside.Y))) --Low.X;
            while (Low.Y > 0 && Box->Contains(FVector2D(Inside.X,Low.Y-1))) --Low.Y;
            while (High.X < Size.X && Box->Contains(FVector2D(High.X+1,Inside.Y))) ++High.X;
            while (High.Y < Size.Y && Box->Contains(FVector2D(Inside.X,High.Y+1))) ++High.Y;
            const FBox2D Rectangle(Low,High);
            for (const FBox2D& Other : Rectangles)
                TestFalse(*FString::Printf(TEXT("%s never overlaps another action at %dx%d"),
                    *Name.ToString(),Size.X,Size.Y), Rectangle.Intersect(Other));
            Rectangles.Add(Rectangle);
        }
        Hud->SetCanvas(nullptr,nullptr); Hud->Destroy();
    }
    PC->UnPossess(); Pawn->Destroy(); PC->Destroy(); World->DestroyWorld(true);
    return true;
}
#endif
