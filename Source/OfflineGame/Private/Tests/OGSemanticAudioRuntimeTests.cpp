#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/OGSemanticAudioRuntime.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Runtime/OGCanonicalCharacterPresentation.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
    // Real component lifecycle, but no audio device, physical feedback channels,
    // local player, asset streaming or canonical store is created by this fixture.
    struct FSemanticAudioFixture
    {
        UWorld* World = nullptr;
        AActor* Owner = nullptr;
        APlayerController* Controller = nullptr;
        UOGSemanticAudioRuntime* Audio = nullptr;

        explicit FSemanticAudioFixture(bool bPawnOwner = false)
        {
            UWorld::InitializationValues Init;
            Init.AllowAudioPlayback(false).CreatePhysicsScene(false)
                .CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr,
                true, ERHIFeatureLevel::SM5, &Init, false);
            if (!World) return;
            if (bPawnOwner)
            {
                APawn* Pawn = World->SpawnActor<APawn>();
                Controller = World->SpawnActor<APlayerController>();
                if (Controller)
                {
                    Controller->Player = NewObject<ULocalPlayer>(GEngine);
                }
                if (Controller && Pawn) Controller->Possess(Pawn);
                Owner = Pawn;
            }
            else Owner = World->SpawnActor<AActor>();
            if (!Owner) return;
            Audio = NewObject<UOGSemanticAudioRuntime>(Owner);
            Owner->AddInstanceComponent(Audio);
            Audio->RegisterComponentWithWorld(World);
            Owner->DispatchBeginPlay();
        }

        ~FSemanticAudioFixture()
        {
            if (World) World->DestroyWorld(true);
        }

        void Advance(float Seconds)
        {
            // The runtime is world-time based, but this isolated native world has no full
            // engine travel/game-instance context. Advance its public clocks directly so
            // the component sees production-equivalent elapsed world time without asking
            // UWorld::Tick to drive unrelated engine systems.
            World->TimeSeconds += Seconds;
            World->UnpausedTimeSeconds += Seconds;
            World->RealTimeSeconds += Seconds;
            World->AudioTimeSeconds += Seconds;
            World->DeltaTimeSeconds = Seconds;
            World->DeltaRealTimeSeconds = Seconds;
            Audio->TickComponent(Seconds, LEVELTICK_TimeOnly, &Audio->PrimaryComponentTick);
        }
    };

    FOGSemanticCaptionCue Cue(const TCHAR* Text, float Start = 0.0f,
        float Duration = 3.0f, bool bClosedCaption = false)
    {
        FOGSemanticCaptionCue Result;
        Result.Text = FText::FromString(Text);
        Result.Speaker = FText::FromString(TEXT("Authored speaker"));
        Result.StartSeconds = Start;
        Result.DurationSeconds = Duration;
        Result.bClosedCaption = bClosedCaption;
        return Result;
    }

    FOGSemanticAudioEventDefinition CaptionEvent(const TCHAR* Text,
        bool bWorldOnly = true, int32 Priority = 0)
    {
        FOGSemanticAudioEventDefinition Result;
        Result.bWorldOnly = bWorldOnly;
        Result.bAllowCaptionWithoutSound = true;
        Result.CaptionPriority = Priority;
        Result.Captions.Add(Cue(Text));
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSemanticAudioRoutingGainTest,
    "OfflineGame.Foundation.SemanticAudio.RouteAndProfileGainAppliedOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGSemanticAudioRoutingGainTest::RunTest(const FString& Parameters)
{
    FSemanticAudioFixture F;
    if (!TestNotNull(TEXT("Isolated runtime"), F.Audio)) return false;
    USoundClass* Routes[] = { NewObject<USoundClass>(), NewObject<USoundClass>(),
        NewObject<USoundClass>(), NewObject<USoundClass>() };
    const EOGSemanticAudioCategory Categories[] = { EOGSemanticAudioCategory::Music,
        EOGSemanticAudioCategory::Voice, EOGSemanticAudioCategory::Sfx,
        EOGSemanticAudioCategory::Ambience };
    const FName Names[] = { TEXT("music"), TEXT("voice"), TEXT("sfx"), TEXT("ambience") };
    USoundClass* AuthoredClass = NewObject<USoundClass>();
    AuthoredClass->Properties.Volume = 0.37f;
    USoundWave* AuthoredSound = NewObject<USoundWave>();
    AuthoredSound->SoundClassObject = AuthoredClass;
    for (int32 I = 0; I < 4; ++I)
    {
        Routes[I]->Properties.Volume = 0.43f;
        auto Definition = CaptionEvent(TEXT("Authored cue"));
        Definition.Category = Categories[I];
        Definition.Gain = 0.8f;
        Definition.Sound = AuthoredSound;
        F.Audio->RegisterEventDefinition(Names[I], Definition);
    }
    FOGPlayerProfileSettings Profile;
    Profile.MasterVolume = 0.5f;
    Profile.MusicVolume = 0.2f;
    Profile.VoiceVolume = 0.3f;
    Profile.SfxVolume = 0.4f;
    Profile.AmbienceVolume = 0.6f;
    Profile.EffectsVolume = 0.01f; // legacy aggregate must not replace category values
    F.Audio->ApplyProfile(Profile);
    F.Audio->ConfigureRoutes(Routes[0], Routes[1], Routes[2], Routes[3], true, true);
    const float CategoryGains[] = { 0.2f, 0.3f, 0.4f, 0.6f };
    for (int32 I = 0; I < 4; ++I)
    {
        auto Projection = F.Audio->GetPlaybackProjection(Names[I]);
        TestTrue(TEXT("Semantic category is preserved"), Projection.Category == Categories[I]);
        TestTrue(TEXT("Category selects its dedicated route"), Projection.Route == Routes[I]);
        TestTrue(TEXT("External master/category gain is not multiplied again"),
            FMath::IsNearlyEqual(Projection.EventGain, 0.8f));
    }
    F.Audio->ConfigureRoutes(Routes[0], Routes[1], Routes[2], Routes[3], false, false);
    for (int32 I = 0; I < 4; ++I)
        TestTrue(TEXT("Runtime-owned gain applies master and category exactly once"),
            FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(Names[I]).EventGain,
                0.8f * 0.5f * CategoryGains[I]));
    F.Audio->ConfigureRoutes(Routes[0], Routes[1], nullptr, Routes[3], true, false);
    TestTrue(TEXT("Missing route retains category gain when other routes are externally mixed"),
        FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(Names[2]).EventGain, 0.8f * 0.5f * 0.4f));
    for (USoundClass* Route : Routes)
        TestEqual(TEXT("Profile never mutates route asset base volume"), Route->Properties.Volume, 0.43f);
    TestEqual(TEXT("Profile never mutates authored base class volume"), AuthoredClass->Properties.Volume, 0.37f);
    TestTrue(TEXT("Routing never rewrites authored sound class"), AuthoredSound->SoundClassObject == AuthoredClass);
    TestEqual(TEXT("Caller master profile remains unchanged"), Profile.MasterVolume, 0.5f);
    TestFalse(TEXT("Unknown event has no fabricated projection"),
        F.Audio->GetPlaybackProjection(TEXT("missing")).bRegistered);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSemanticAudioDynamicRangeTest,
    "OfflineGame.Foundation.SemanticAudio.DynamicFallbackAndAuthoredMixOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGSemanticAudioDynamicRangeTest::RunTest(const FString& Parameters)
{
    FSemanticAudioFixture F;
    if (!TestNotNull(TEXT("Isolated runtime"), F.Audio)) return false;
    const EOGSemanticAudioCategory Categories[] = { EOGSemanticAudioCategory::Music,
        EOGSemanticAudioCategory::Voice, EOGSemanticAudioCategory::Sfx, EOGSemanticAudioCategory::Ambience };
    const FName Names[] = { TEXT("music"), TEXT("voice"), TEXT("sfx"), TEXT("ambience") };
    for (int32 I = 0; I < 4; ++I)
    {
        auto Definition = CaptionEvent(TEXT("Authored cue"));
        Definition.Category = Categories[I];
        F.Audio->RegisterEventDefinition(Names[I], Definition);
    }
    F.Audio->ConfigureRoutes(nullptr, nullptr, nullptr, nullptr, false, false);
    FOGPlayerProfileSettings Profile;
    Profile.DynamicRangeProfile = TEXT("night");
    F.Audio->ApplyProfile(Profile);
    const float Night[] = { 0.86f, 1.0f, 0.68f, 0.76f };
    for (int32 I = 0; I < 4; ++I)
        TestTrue(TEXT("Night fallback balances category without suppressing voice"),
            FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(Names[I]).EventGain, Night[I]));
    Profile.DynamicRangeProfile = TEXT("compressed");
    F.Audio->ApplyProfile(Profile);
    const float Compressed[] = { 0.92f, 1.0f, 0.82f, 0.88f };
    for (int32 I = 0; I < 4; ++I)
        TestTrue(TEXT("Compressed fallback uses its own balance"),
            FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(Names[I]).EventGain, Compressed[I]));

    USoundMix* FullMix = NewObject<USoundMix>();
    USoundMix* NightMix = NewObject<USoundMix>();
    USoundMix* CompressedMix = NewObject<USoundMix>();
    F.Audio->ConfigureDynamicRangeMixes(FullMix, NightMix, CompressedMix);
    TestTrue(TEXT("Ownership records authored compressed selection"),
        F.Audio->GetOwnershipProjection().ActiveMix == CompressedMix);
    TestEqual(TEXT("Authored mix does not also receive category fallback"),
        F.Audio->GetPlaybackProjection(TEXT("sfx")).EventGain, 1.0f);
    Profile.DynamicRangeProfile = TEXT("night");
    F.Audio->ApplyProfile(Profile);
    TestTrue(TEXT("Profile replaces owned mix selection"),
        F.Audio->GetOwnershipProjection().ActiveMix == NightMix);
    F.Audio->ReleaseDynamicRangeMixOwnership();
    TestFalse(TEXT("Release drops ownership"), F.Audio->GetOwnershipProjection().bOwnsDynamicRange);
    TestTrue(TEXT("Release drops active mix"), F.Audio->GetOwnershipProjection().ActiveMix == nullptr);
    TestTrue(TEXT("Release restores fallback"),
        FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(TEXT("sfx")).EventGain, Night[2]));
    F.Audio->ConfigureDynamicRangeMixes(FullMix, nullptr, CompressedMix);
    TestTrue(TEXT("Missing authored night mix keeps fallback"),
        FMath::IsNearlyEqual(F.Audio->GetPlaybackProjection(TEXT("sfx")).EventGain, Night[2]));
    F.Audio->ConfigureRoutes(NewObject<USoundClass>(), nullptr, NewObject<USoundClass>(),
        nullptr, true, true);
    TestEqual(TEXT("Pawn-owned mixed route does not receive a second dynamic fallback"),
        F.Audio->GetPlaybackProjection(TEXT("sfx")).EventGain, 1.0f);
    Profile.DynamicRangeProfile = TEXT("full");
    F.Audio->ApplyProfile(Profile);
    TestTrue(TEXT("Full selects its authored mix"),
        F.Audio->GetOwnershipProjection().ActiveMix == FullMix);
    F.Audio->EndPlay(EEndPlayReason::Destroyed);
    F.Owner->Destroy();
    TestTrue(TEXT("Owner end play releases authored mix"),
        F.Audio->GetOwnershipProjection().ActiveMix == nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSemanticAudioCaptionLifecycleTest,
    "OfflineGame.Foundation.SemanticAudio.AuthoredCaptionsCooldownAndWorldOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGSemanticAudioCaptionLifecycleTest::RunTest(const FString& Parameters)
{
    FSemanticAudioFixture F;
    if (!TestNotNull(TEXT("Isolated runtime"), F.Audio)) return false;
    int32 PlayedEvents = 0;
    F.Audio->OnSemanticEventPlayed.AddLambda([this, &PlayedEvents](FName, UAudioComponent* Audio)
    {
        ++PlayedEvents;
        TestTrue(TEXT("Text-only event supplies no audio component"), Audio == nullptr);
    });
    auto Definition = CaptionEvent(TEXT("initial"));
    Definition.bAllowCaptionWithoutSound = false;
    F.Audio->RegisterEventDefinition(TEXT("no_sound"), Definition);
    TestFalse(TEXT("Caption without playback requires authored opt-in"),
        F.Audio->PlaySemanticEvent(TEXT("no_sound")));
    TestEqual(TEXT("Rejected event does not notify listeners"), PlayedEvents, 0);
    Definition.bAllowCaptionWithoutSound = true;
    Definition.CooldownSeconds = 0.5f;
    Definition.Captions = { Cue(TEXT("Later authored sound description"), 0.4f, 0.2f, true),
        Cue(TEXT("First authored line"), 0.1f, 0.1f),
        Cue(TEXT("Invalid negative start"), -1.0f), Cue(TEXT(""), 0.0f),
        Cue(TEXT("Invalid empty duration"), 0.0f, 0.0f) };
    F.Audio->RegisterEventDefinition(TEXT("sequence"), Definition);
    TestTrue(TEXT("Validated authored captions are registered"),
        F.Audio->HasAuthoredCaptions(TEXT("sequence")));
    TestFalse(TEXT("Text-only event does not fabricate a sound"),
        F.Audio->HasAuthoredSound(TEXT("sequence")));
    TestTrue(TEXT("Opted-in text event starts"), F.Audio->PlaySemanticEvent(TEXT("sequence")));
    TestFalse(TEXT("Delayed caption is initially hidden"), F.Audio->GetCurrentCaption().bVisible);
    TestFalse(TEXT("Cooldown prevents duplicate play"), F.Audio->PlaySemanticEvent(TEXT("sequence")));
    F.Advance(0.125f);
    TestEqual(TEXT("Authored cues are selected by start time"),
        F.Audio->GetCurrentCaption().Text.ToString(), FString(TEXT("First authored line")));
    auto DetachedCaption = F.Audio->GetCurrentCaption();
    DetachedCaption.Text = FText::FromString(TEXT("caller changes copy"));
    TestEqual(TEXT("Projection copy cannot modify runtime caption"),
        F.Audio->GetCurrentCaption().Text.ToString(), FString(TEXT("First authored line")));
    F.Advance(0.125f);
    TestFalse(TEXT("Caption gap stays empty"), F.Audio->GetCurrentCaption().bVisible);
    F.Advance(0.2f);
    TestTrue(TEXT("Later authored cue is a closed caption"),
        F.Audio->GetCurrentCaption().bClosedCaption);
    TestEqual(TEXT("Authored speaker remains attached"),
        F.Audio->GetCurrentCaption().Speaker.ToString(), FString(TEXT("Authored speaker")));
    FOGPlayerProfileSettings Profile;
    Profile.bSubtitlesEnabled = false;
    Profile.SubtitlePresentation = TEXT("large");
    Profile.UiReadabilityProfile = TEXT("high_contrast");
    F.Audio->ApplyProfile(Profile);
    TestFalse(TEXT("Subtitle preference immediately hides active caption"),
        F.Audio->GetCurrentCaption().bVisible);
    TestEqual(TEXT("Readability remains a profile projection"),
        F.Audio->GetCurrentCaption().Readability, FName(TEXT("high_contrast")));
    Profile.bSubtitlesEnabled = true;
    F.Audio->ApplyProfile(Profile);
    TestTrue(TEXT("Re-enabling subtitles restores current authored cue"),
        F.Audio->GetCurrentCaption().bVisible);
    F.Advance(0.2f);
    TestFalse(TEXT("Last cue expiry hides projection"), F.Audio->GetCurrentCaption().bVisible);
    TestEqual(TEXT("Expired event releases active ownership"),
        F.Audio->GetOwnershipProjection().ActiveEvents, 0);
    TestTrue(TEXT("Cooldown expiry permits a later event"), F.Audio->PlaySemanticEvent(TEXT("sequence")));
    F.Audio->CancelOwnedPresentation();
    TestTrue(TEXT("Cancel clears cooldown for a new presentation session"),
        F.Audio->PlaySemanticEvent(TEXT("sequence")));
    F.Audio->CancelOwnedPresentation();

    F.Audio->RegisterEventDefinition(TEXT("world"), CaptionEvent(TEXT("World cue"), true, 10));
    F.Audio->RegisterEventDefinition(TEXT("ui"), CaptionEvent(TEXT("UI cue"), false, 1));
    TestTrue(TEXT("World event starts in World"), F.Audio->PlaySemanticEvent(TEXT("world")));
    TestTrue(TEXT("UI event can coexist"), F.Audio->PlaySemanticEvent(TEXT("ui")));
    TestEqual(TEXT("Caption priority overrides newest lower-priority event"),
        F.Audio->GetCurrentCaption().EventName, FName(TEXT("world")));
    F.Audio->SetWorldPresentationActive(false);
    TestEqual(TEXT("Leaving World cancels only World-owned event"),
        F.Audio->GetOwnershipProjection().ActiveEvents, 1);
    TestEqual(TEXT("UI caption survives leaving World"),
        F.Audio->GetCurrentCaption().EventName, FName(TEXT("ui")));
    const int32 BeforeRejectedPlay = PlayedEvents;
    TestFalse(TEXT("World-only playback is suppressed outside World"),
        F.Audio->PlaySemanticEvent(TEXT("world")));
    TestEqual(TEXT("Suppression emits no played event"), PlayedEvents, BeforeRejectedPlay);
    TestTrue(TEXT("Non-World presentation event remains allowed"),
        F.Audio->PlaySemanticEvent(TEXT("ui")));
    F.Audio->UnregisterEventDefinition(TEXT("ui"));
    TestEqual(TEXT("Unregister stops every owned instance of the event"),
        F.Audio->GetOwnershipProjection().ActiveEvents, 0);
    TestFalse(TEXT("Unregister clears caption projection"), F.Audio->GetCurrentCaption().bVisible);
    TestFalse(TEXT("Idle owner does not keep component tick enabled"), F.Audio->IsComponentTickEnabled());
    F.Audio->SetWorldPresentationActive(true);
    F.Audio->PlaySemanticEvent(TEXT("world"));
    F.Audio->EndPlay(EEndPlayReason::Destroyed);
    F.Owner->Destroy();
    TestEqual(TEXT("Owner end play releases all events"), F.Audio->GetOwnershipProjection().ActiveEvents, 0);
    TestFalse(TEXT("Owner end play clears caption"), F.Audio->GetCurrentCaption().bVisible);
    TestFalse(TEXT("Owner end play clears native notification subscriptions"),
        F.Audio->OnSemanticEventPlayed.IsBound());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSemanticAudioHapticCleanupTest,
    "OfflineGame.Foundation.SemanticAudio.HapticsDisableWorldCancelAndOwnerCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGSemanticAudioHapticCleanupTest::RunTest(const FString& Parameters)
{
    FSemanticAudioFixture F(true);
    if (!TestNotNull(TEXT("Local pawn runtime without a physical player"), F.Audio)) return false;
    const APawn* Pawn = Cast<APawn>(F.Owner);
    if (!TestTrue(TEXT("Synthetic controller is locally controlled"),
        Pawn && Pawn->IsLocallyControlled())) return false;
    auto Definition = CaptionEvent(TEXT("Haptic presentation"), true);
    // Empty authored effect has no channels; tests ownership with no device output.
    Definition.Haptic = NewObject<UForceFeedbackEffect>();
    Definition.HapticGain = 0.5f;
    F.Audio->RegisterEventDefinition(TEXT("feedback"), Definition);
    TestTrue(TEXT("Haptic presentation starts"), F.Audio->PlaySemanticEvent(TEXT("feedback")));
    TestEqual(TEXT("Runtime owns feedback component"), F.Audio->GetOwnershipProjection().FeedbackComponents, 1);
    FOGPlayerProfileSettings Profile;
    Profile.bHapticsEnabled = false;
    F.Audio->ApplyProfile(Profile);
    TestEqual(TEXT("Disabling haptics immediately releases owned feedback"),
        F.Audio->GetOwnershipProjection().FeedbackComponents, 0);
    TestTrue(TEXT("Disabling haptics preserves authored caption"), F.Audio->GetCurrentCaption().bVisible);
    Profile.bHapticsEnabled = true;
    F.Audio->ApplyProfile(Profile);
    TestEqual(TEXT("Re-enabling does not resurrect stopped feedback"),
        F.Audio->GetOwnershipProjection().FeedbackComponents, 0);
    F.Audio->StopSemanticEvent(TEXT("feedback"));
    F.Audio->PlaySemanticEvent(TEXT("feedback"));
    F.Audio->SetWorldPresentationActive(false);
    TestEqual(TEXT("Leaving World releases World feedback"),
        F.Audio->GetOwnershipProjection().FeedbackComponents, 0);
    F.Audio->SetWorldPresentationActive(true);
    F.Audio->PlaySemanticEvent(TEXT("feedback"));
    Profile.HapticsIntensity = 0.0f;
    F.Audio->ApplyProfile(Profile);
    TestEqual(TEXT("Zero intensity also releases owned feedback"),
        F.Audio->GetOwnershipProjection().FeedbackComponents, 0);
    Profile.HapticsIntensity = 0.8f;
    F.Audio->ApplyProfile(Profile);
    F.Audio->CancelOwnedPresentation();
    F.Audio->PlaySemanticEvent(TEXT("feedback"));
    F.Audio->EndPlay(EEndPlayReason::Destroyed);
    F.Owner->Destroy();
    const auto Ownership = F.Audio->GetOwnershipProjection();
    TestEqual(TEXT("Owner destruction releases feedback"), Ownership.FeedbackComponents, 0);
    TestEqual(TEXT("Owner destruction releases events"), Ownership.ActiveEvents, 0);
    TestEqual(TEXT("No speaker component was created"), Ownership.AudioComponents, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSemanticAudioCanonicalReadOnlyTest,
    "OfflineGame.Foundation.SemanticAudio.SharedCanonicalPresenterRemainsReadOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGSemanticAudioCanonicalReadOnlyTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("Automation"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Directory, true);
    bool bCompleted = false;
    {
        FOGSQLiteWorldStore Store;
        FString Error;
        if (TestTrue(TEXT("Open isolated canonical fixture"),
            Store.Open(FPaths::Combine(Directory, TEXT("presentation.db")), Error)))
        {
            FSemanticAudioFixture F;
            const FOGEntityId Entity = FOGEntityId::NewId();
            const FString CanonicalState = TEXT("{\"injury\":\"authored\",\"hp\":731}");
            FOGFoundationCharacterContext Context;
            Context.EntityId = Entity;
            if (TestNotNull(TEXT("Shared presentation owner"), F.Owner)
                && TestTrue(TEXT("Persist canonical entity once"),
                    Store.UpsertEntity(Entity, TEXT("ruler"), 19, CanonicalState, Error)))
            {
                bool bInitiallyFound = false;
                FName InitialKind;
                FString InitialState;
                int64 BeforeRevision = -1;
                TestTrue(TEXT("Read actual canonical baseline before binding"),
                    Store.TryReadEntity(Entity, bInitiallyFound, InitialKind, InitialState, BeforeRevision, Error));
                TestTrue(TEXT("Canonical baseline exists"), bInitiallyFound);
                TArray<FOGWorldEvent> BeforeEvents;
                TestTrue(TEXT("Read canonical events before binding"),
                    Store.ListWorldEvents(Entity, NAME_None, false, 1000, BeforeEvents, Error));
                TestEqual(TEXT("Fixture has no canonical events"), BeforeEvents.Num(), 0);
                USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(F.Owner);
                F.Owner->AddInstanceComponent(Mesh);
                Mesh->RegisterComponentWithWorld(F.World);
                auto* Presenter = NewObject<UOGCanonicalCharacterPresentation>(F.Owner);
                F.Owner->AddInstanceComponent(Presenter);
                Presenter->RegisterComponentWithWorld(F.World);
                if (TestTrue(TEXT("Bind actual presenter to the same canonical entity"),
                    Presenter->ConfigureFromCanonicalStore(Context, Mesh, TEXT("diagnostic"),
                        Store, nullptr, Error)))
                {
                    TestEqual(TEXT("Initial binding retains canonical revision"),
                        Presenter->GetProjection().EntityRevision, BeforeRevision);
                    FOGPlayerProfileSettings Profile;
                    Profile.MasterVolume = 0.4f;
                    Profile.SubtitlePresentation = TEXT("large");
                    Profile.UiReadabilityProfile = TEXT("high_contrast");
                    Profile.DynamicRangeProfile = TEXT("night");
                    F.Audio->ApplyProfile(Profile);
                    F.Audio->ConfigureRoutes(nullptr, nullptr, nullptr, nullptr, false, false);
                    F.Audio->SetEnvironmentState(TEXT("underwater"), 0.75f);
                    F.Audio->RegisterEventDefinition(TEXT("authored.presentation"),
                        CaptionEvent(TEXT("Authored description")));
                    TestTrue(TEXT("Presentation event starts on canonical actor"),
                        F.Audio->PlaySemanticEvent(TEXT("authored.presentation")));
                    F.Audio->SetWorldPresentationActive(false);
                    F.Audio->CancelOwnedPresentation();
                    Presenter->SetPrivacyPresentation(true);
                    TestTrue(TEXT("Read-only refresh succeeds after audio/profile cleanup"),
                        Presenter->RefreshFromCanonicalStore(Context, Store, nullptr, Error));
                    TestTrue(TEXT("Caller canonical selection is unchanged"), Context.EntityId == Entity);
                    TestEqual(TEXT("Caller profile is unchanged"), Profile.MasterVolume, 0.4f);
                    TestEqual(TEXT("Presenter still projects canonical payload"),
                        Presenter->GetProjection().EntityStateJson, CanonicalState);
                    TestEqual(TEXT("Presentation retains canonical revision"),
                        Presenter->GetProjection().EntityRevision, BeforeRevision);
                    F.Owner->Destroy();

                    bool bFound = false;
                    FName Kind;
                    FString State;
                    int64 Revision = -1;
                    TestTrue(TEXT("Read actual canonical entity after owner cleanup"),
                        Store.TryReadEntity(Entity, bFound, Kind, State, Revision, Error));
                    TestTrue(TEXT("Canonical entity remains present"), bFound);
                    TestEqual(TEXT("Audio/presentation cleanup preserves entity kind"), Kind, FName(TEXT("ruler")));
                    TestEqual(TEXT("Audio/profile/presentation do not rewrite canonical state"), State, CanonicalState);
                    TestEqual(TEXT("Audio/profile/presentation do not advance canonical revision"), Revision, BeforeRevision);
                    TArray<FOGWorldEvent> AfterEvents;
                    TestTrue(TEXT("Read canonical events after presentation"),
                        Store.ListWorldEvents(Entity, NAME_None, false, 1000, AfterEvents, Error));
                    TestEqual(TEXT("Native presentation emits no canonical world event"),
                        AfterEvents.Num(), BeforeEvents.Num());
                    bCompleted = true;
                }
            }
        }
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return bCompleted;
}

#endif
