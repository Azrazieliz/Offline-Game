#include "Runtime/OGPresentationModeSubsystem.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/BlueprintPlatformLibrary.h"
#include "OfflineGame.h"
#include "Runtime/OGPlayerProfileSettings.h"

namespace
{
EScreenOrientation::Type ToScreenOrientation(FName Name)
{
    if (Name == FName(TEXT("portrait")))
    {
        return EScreenOrientation::PortraitSensor;
    }
    if (Name == FName(TEXT("portrait_upside_down")))
    {
        return EScreenOrientation::PortraitUpsideDown;
    }
    if (Name == FName(TEXT("landscape_left")))
    {
        return EScreenOrientation::LandscapeLeft;
    }
    if (Name == FName(TEXT("landscape_right")))
    {
        return EScreenOrientation::LandscapeRight;
    }
    if (Name == FName(TEXT("sensor")) ||
        Name == FName(TEXT("full_sensor")))
    {
        return EScreenOrientation::FullSensor;
    }
    return EScreenOrientation::LandscapeSensor;
}
}

void UOGPresentationModeSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CurrentMode = EOGPresentationMode::World;
    bWorldPresentationSuspended = false;
}

void UOGPresentationModeSubsystem::Deinitialize()
{
    RestoreWorldPresentation();
    RegisteredWorldActors.Reset();
    PriorHiddenState.Reset();
    PriorTickState.Reset();
    PriorComponentTickState.Reset();
    Super::Deinitialize();
}

void UOGPresentationModeSubsystem::SuspendActorPresentation(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!PriorHiddenState.Contains(Actor))
    {
        PriorHiddenState.Add(Actor, Actor->IsHidden());
    }
    if (!PriorTickState.Contains(Actor))
    {
        PriorTickState.Add(Actor, Actor->IsActorTickEnabled());
    }

    TMap<TWeakObjectPtr<UActorComponent>, bool>& ComponentStates =
        PriorComponentTickState.FindOrAdd(Actor);
    TInlineComponentArray<UActorComponent*> Components;
    Actor->GetComponents(Components);
    const FName PersistentTag(TEXT("OG.PresentationPersistent"));
    for (UActorComponent* Component : Components)
    {
        if (!IsValid(Component))
        {
            continue;
        }

        // Canonical/UI adapters opt out; world presentation components do not.
        if (Component->ComponentHasTag(PersistentTag))
        {
            if (const bool* PriorState = ComponentStates.Find(Component))
            {
                Component->SetComponentTickEnabled(*PriorState);
                ComponentStates.Remove(Component);
            }
            continue;
        }

        // Re-registration discovers new components without overwriting the
        // original state of components that are already suspended.
        if (!ComponentStates.Contains(Component))
        {
            ComponentStates.Add(Component, Component->IsComponentTickEnabled());
        }
        Component->SetComponentTickEnabled(false);
    }

    Actor->SetActorHiddenInGame(true);
    Actor->SetActorTickEnabled(false);
}

void UOGPresentationModeSubsystem::RestoreActorPresentation(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    if (const bool* WasHidden = PriorHiddenState.Find(Actor))
    {
        Actor->SetActorHiddenInGame(*WasHidden);
    }
    if (const bool* WasTicking = PriorTickState.Find(Actor))
    {
        Actor->SetActorTickEnabled(*WasTicking);
    }
    if (const TMap<TWeakObjectPtr<UActorComponent>, bool>* ComponentStates =
            PriorComponentTickState.Find(Actor))
    {
        for (const TPair<TWeakObjectPtr<UActorComponent>, bool>& Entry : *ComponentStates)
        {
            if (UActorComponent* Component = Entry.Key.Get())
            {
                Component->SetComponentTickEnabled(Entry.Value);
            }
        }
    }

    PriorHiddenState.Remove(Actor);
    PriorTickState.Remove(Actor);
    PriorComponentTickState.Remove(Actor);
}

void UOGPresentationModeSubsystem::RegisterWorldPresentationActor(
    AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    RegisteredWorldActors.Add(Actor);
    if (bWorldPresentationSuspended || CurrentMode == EOGPresentationMode::Ruler)
    {
        SuspendActorPresentation(Actor);
    }
}

void UOGPresentationModeSubsystem::UnregisterWorldPresentationActor(
    AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    RestoreActorPresentation(Actor);
    RegisteredWorldActors.Remove(Actor);
}

void UOGPresentationModeSubsystem::DiscoverTaggedWorldPresentationActors()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FName PresentationTag(TEXT("OG.WorldPresentation"));
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->Tags.Contains(PresentationTag))
        {
            RegisteredWorldActors.Add(Actor);
        }
    }
}

void UOGPresentationModeSubsystem::ApplyWorldInputSuppression(
    bool bSuppress)
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (bOwnsInputSuppression && (!bSuppress || SuppressedController.Get() != Controller))
    {
        if (SuppressedController.IsValid())
        {
            SuppressedController->SetIgnoreMoveInput(false);
            SuppressedController->SetIgnoreLookInput(false);
        }
        bOwnsInputSuppression = false; SuppressedController.Reset();
    }
    if (bSuppress && Controller && !bOwnsInputSuppression)
    {
        Controller->SetIgnoreMoveInput(true); Controller->SetIgnoreLookInput(true);
        SuppressedController = Controller; bOwnsInputSuppression = true;
    }

}

void UOGPresentationModeSubsystem::SuspendWorldPresentation()
{
    DiscoverTaggedWorldPresentationActors();

    for (const TWeakObjectPtr<AActor>& WeakActor : RegisteredWorldActors)
    {
        SuspendActorPresentation(WeakActor.Get());
    }

    ApplyWorldInputSuppression(true);
    bWorldPresentationSuspended = true;
}

void UOGPresentationModeSubsystem::RestoreWorldPresentation()
{
    for (const TWeakObjectPtr<AActor>& WeakActor : RegisteredWorldActors)
    {
        RestoreActorPresentation(WeakActor.Get());
    }

    PriorHiddenState.Reset();
    PriorTickState.Reset();
    PriorComponentTickState.Reset();
    ApplyWorldInputSuppression(false);
    bWorldPresentationSuspended = false;
}

void UOGPresentationModeSubsystem::ApplyOrientationForMode(
    EOGPresentationMode Mode)
{
    FOGPlayerProfileSettings Profile;
    FString Error;
    if (!FOGPlayerProfileSettingsService::Load(
            FOGPlayerProfileSettingsService::DefaultProfilePath(),
            Profile,
            Error))
    {
        Profile = FOGPlayerProfileSettings();
    }

    const FName Preferred =
        Mode == EOGPresentationMode::Ruler
            ? FName(TEXT("portrait"))
            : FName(TEXT("landscape"));

    const FName Resolved =
        FOGPlayerProfileSettingsService::ResolveOrientation(
            Profile,
            Preferred);

    UBlueprintPlatformLibrary::SetAllowedDeviceOrientation(
        ToScreenOrientation(Resolved));
}

void UOGPresentationModeSubsystem::SetMode(
    EOGPresentationMode NewMode)
{
    if (NewMode == CurrentMode)
    {
        if (NewMode == EOGPresentationMode::Ruler)
        {
            SuspendWorldPresentation();
        }
        else
        {
            ApplyWorldInputSuppression(false);
        }
        ApplyOrientationForMode(NewMode);
        return;
    }

    if (NewMode == EOGPresentationMode::Ruler)
    {
        SuspendWorldPresentation();
    }
    else
    {
        RestoreWorldPresentation();
    }

    CurrentMode = NewMode;
    ApplyOrientationForMode(CurrentMode);
    OnPresentationModeChanged.Broadcast(CurrentMode);

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Presentation mode changed to %s. WorldPresentationSuspended=%s"),
        CurrentMode == EOGPresentationMode::Ruler
            ? TEXT("Ruler")
            : TEXT("World"),
        bWorldPresentationSuspended ? TEXT("true") : TEXT("false"));
}
