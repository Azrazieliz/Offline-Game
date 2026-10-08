#include "World/OGFoundationIntegrationWorld.h"
#include "World/OGFoundationInteractionFixture.h"
#include "World/OGFoundationWaterVolume.h"
#include "World/OGStartingRegionPresentation.h"
#include "World/OGTraversalFramework.h"
#include "Combat/OGWorldActionRuntime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/OGPresentationModeSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarOGWorldLabels(TEXT("og.Debug.WorldLabels"),0,
    TEXT("Show integration-world developer labels when the world is created."));

AOGFoundationIntegrationWorld::AOGFoundationIntegrationWorld()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Tags.Add(TEXT("OG.WorldPresentation"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeAsset.Succeeded()) { Cube = CubeAsset.Object; }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if(ConeAsset.Succeeded()) SceneryCone=ConeAsset.Object;
    if(SphereAsset.Succeeded()) ScenerySphere=SphereAsset.Object;
}

UStaticMeshComponent* AOGFoundationIntegrationWorld::Box(
    const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
    auto* Mesh = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetRelativeLocation(Center);
    Mesh->SetRelativeRotation(Rotation);
    Mesh->SetRelativeScale3D(Size / 100.0f);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    if (auto* Parent = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        auto* Tint = UMaterialInstanceDynamic::Create(Parent, this);
        const bool bGround=Size.Z<=100.f && FMath::Max(Size.X,Size.Y)>1000.f;
        const FLinearColor Color=Center.Z<-100.f?FLinearColor(.10f,.17f,.19f):
            bGround?FLinearColor(.20f,.27f,.22f):FLinearColor(.32f,.36f,.37f);
        Tint->SetVectorParameterValue(TEXT("Color"),Color);
        Mesh->SetMaterial(0, Tint);
    }
    Mesh->RegisterComponent();
    return Mesh;
}

void AOGFoundationIntegrationWorld::Label(const FVector& Center,
    const FString& Message)
{
    auto* Text = NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(Text);
    Text->SetupAttachment(RootComponent);
    Text->SetRelativeLocation(Center + FVector(0.0f, 0.0f, 150.0f));
    Text->SetCullDistance(2200.0f);
    Text->SetText(FText::FromString(Message));
    Text->SetWorldSize(30.0f);
    Text->SetTextRenderColor(FColor::White);
    Text->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetVisibility(CVarOGWorldLabels.GetValueOnGameThread()!=0);
    Text->RegisterComponent();
}

void AOGFoundationIntegrationWorld::Own(AActor* Actor)
{
    if (!Actor) { return; }
    OwnedActors.Add(Actor);
    Actor->SetOwner(this);
    if (UGameInstance* Instance = GetGameInstance())
    {
        if (auto* Presentation = Instance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Presentation->RegisterWorldPresentationActor(Actor);
        }
    }
}

bool AOGFoundationIntegrationWorld::IsInsidePlayableBoundary_Implementation(
    const FVector& Location) const
{
    const FVector Local = GetActorTransform().InverseTransformPosition(Location);
    return FMath::Abs(Local.X) <= 3600.0f && FMath::Abs(Local.Y) <= 3600.0f;
}

bool AOGFoundationIntegrationWorld::IsInsideStableGroundRegion_Implementation(
    const FVector& Location) const
{
    const FVector Local = GetActorTransform().InverseTransformPosition(Location);
    // Water, the shaft and edges are never recovery anchors. No Z limit.
    const bool bLake = Local.X >= 750.0f && Local.Y <= -1350.0f;
    const bool bShaft = Local.X <= -2450.0f && Local.Y >= 2450.0f;
    return FMath::Abs(Local.X) <= 3250.0f && FMath::Abs(Local.Y) <= 3250.0f
        && !bLake && !bShaft;
}

bool AOGFoundationIntegrationWorld::SetFlight(
    AOGWorldPrototypeCharacter* Character, bool bEnable)
{
    if (!Character || !Character->GetTraversalCapabilities()) { return false; }
    auto* Capabilities = Character->GetTraversalCapabilities();
    if (bEnable)
    {
        if (FlightBorrower.IsValid()) { return false; }
        bAddedFlight = !Capabilities->HasCapability(FOGTraversalCapabilityIds::Flight);
        if (bAddedFlight) { Capabilities->GrantCapability(FOGTraversalCapabilityIds::Flight); }
        if (!Character->BeginFlight())
        {
            if (bAddedFlight) { Capabilities->RevokeCapability(FOGTraversalCapabilityIds::Flight); }
            bAddedFlight = false;
            return false;
        }
        FlightBorrower = Character;
        if (auto* Party = Character->FindComponentByClass<UOGWorldPartyRuntimeComponent>())
        {
            if (Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
            {
                FlightOwnerId = Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId;
            }
        }
        return true;
    }
    if (FlightBorrower.Get() != Character) { return false; }
    Character->EndFlight();
    if (bAddedFlight) { Capabilities->RevokeCapability(FOGTraversalCapabilityIds::Flight); }
    FlightBorrower.Reset();
    bAddedFlight = false;
    FlightOwnerId = FOGEntityId();
    if (FlightStation.IsValid()) { FlightStation->ResetTemporaryFlightProjection(); }
    return true;
}

void AOGFoundationIntegrationWorld::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!FlightBorrower.IsValid()) { return; }
    auto* Pawn = FlightBorrower.Get();
    auto* Party = Pawn->FindComponentByClass<UOGWorldPartyRuntimeComponent>();
    if (!Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()) ||
        Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId != FlightOwnerId ||
        !Pawn->GetTraversalCapabilities()->HasCapability(FOGTraversalCapabilityIds::Flight))
    {
        SetFlight(Pawn, false);
    }
}

void AOGFoundationIntegrationWorld::EndPlay(const EEndPlayReason::Type Reason)
{
    if (FlightBorrower.IsValid()) { SetFlight(FlightBorrower.Get(), false); }
    if (bAddedTeleport && TravelCapabilityBorrower.IsValid())
    {
        if (UOGTraversalCapabilityComponent* Capabilities =
                TravelCapabilityBorrower->GetTraversalCapabilities())
        {
            Capabilities->RevokeCapability(FOGTraversalCapabilityIds::Teleport);
        }
    }
    TravelCapabilityBorrower.Reset();
    bAddedTeleport = false;
    for (AActor* Actor : OwnedActors) { if (IsValid(Actor)) { Actor->Destroy(); } }
    Super::EndPlay(Reason);
}

void AOGFoundationIntegrationWorld::Build(AOGWorldPrototypeCharacter* Character,
    TFunction<int64()> ReadCanonicalWorldTick)
{
    if (!GetWorld() || !Character || !OwnedActors.IsEmpty()) { return; }
    const FVector Origin = GetActorLocation();
    auto Position = [Origin](const FVector& Local) { return Origin + Local; };
    // Deck with two genuine openings: a water basin and a deep shaft.
    // No plane spans the lake/shaft underneath these components.
    Box(FVector(-850.0f, -50.0f, -40.0f), FVector(3300.0f, 6900.0f, 80.0f));
    Box(FVector(-3000.0f, -500.0f, -40.0f), FVector(1000.0f, 6000.0f, 80.0f));
    Box(FVector(2150.0f, 1050.0f, -40.0f), FVector(2700.0f, 4900.0f, 80.0f));
    Box(FVector(2150.0f, -3250.0f, -40.0f), FVector(2700.0f, 500.0f, 80.0f));
    Box(FVector(3350.0f, -2200.0f, -40.0f), FVector(300.0f, 1600.0f, 80.0f));
    // Continuous 16-degree bed creates walkable shallow -> swim -> dive depths.
    const float Slope = FMath::RadiansToDegrees(FMath::Atan2(700.0f, 2400.0f));
    Box(FVector(2000.0f, -2200.0f, -375.0f),
        FVector(FMath::Sqrt(2400.0f * 2400.0f + 700.0f * 700.0f), 1600.0f, 50.0f),
        FRotator(-Slope, 0.0f, 0.0f));
    // Seal the space below the deck. The sloped floor alone left open sides
    // into the sky/underside. West wall stops below the shallow walk-in lip.
    Box(FVector(2000.0f, -3025.0f, -450.0f), FVector(2500.0f, 50.0f, 900.0f));
    Box(FVector(2000.0f, -1375.0f, -450.0f), FVector(2500.0f, 50.0f, 900.0f));
    Box(FVector(3225.0f, -2200.0f, -450.0f), FVector(50.0f, 1600.0f, 900.0f));
    Box(FVector(775.0f, -2200.0f, -490.0f), FVector(50.0f, 1600.0f, 820.0f));
    Box(FVector(2000.0f, -2200.0f, -875.0f), FVector(2500.0f, 1700.0f, 50.0f));
    const FTransform WaterTransform(FRotator::ZeroRotator,
        Position(FVector(2000.0f, -2200.0f, -400.0f)));
    auto* Water = GetWorld()->SpawnActorDeferred<AOGFoundationWaterVolume>(
        AOGFoundationWaterVolume::StaticClass(), WaterTransform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (Water)
    {
        Water->Configure(FVector(1200.0f, 800.0f, 400.0f));
        UGameplayStatics::FinishSpawningActor(Water, WaterTransform);
        Own(Water);
    }
    auto* Surface = Box(FVector(2000.0f, -2200.0f, -2.0f), FVector(2400.0f, 1600.0f, 4.0f));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (auto* Parent = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Foundation/Materials/M_FoundationWaterSurface.M_FoundationWaterSurface")))
    {
        auto* Tint = UMaterialInstanceDynamic::Create(Parent, this);
        Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.42f, 0.75f));
        Tint->SetScalarParameterValue(TEXT("SurfaceOpacity"), 0.20f);
        Surface->SetMaterial(0, Tint);
    }
    // A thin tinted visual surface has no collision; the real volume owns immersion.
    Label(FVector(720.0f, -2200.0f, 160.0f), TEXT("WATER: walk in | descend to dive | ascend | walk ashore"));

    auto Fixture = [this, &Position, &ReadCanonicalWorldTick](uint32 Id, EOGFoundationInteractionKind Kind,
        const FString& Name, FVector At, FVector Size)
    {
        auto* Actor = GetWorld()->SpawnActor<AOGFoundationInteractionFixture>(
            Position(At), FRotator::ZeroRotator);
        if (Actor)
        {
            Actor->ReadCanonicalWorldTick = ReadCanonicalWorldTick;
            Actor->Configure(Id, Kind, Name, Size);
            Own(Actor);
        }
        return Actor;
    };
    Fixture(1, EOGFoundationInteractionKind::Npc, TEXT("Talk"),
        FVector(-700.0f, -500.0f, 90.0f), FVector(70.0f, 70.0f, 180.0f));
    Fixture(2, EOGFoundationInteractionKind::Pickup, TEXT("Collect"),
        FVector(-700.0f, 0.0f, 25.0f), FVector(45.0f, 45.0f, 50.0f));
    Fixture(3, EOGFoundationInteractionKind::Chest, TEXT("Open chest"),
        FVector(-700.0f, 500.0f, 40.0f), FVector(100.0f, 65.0f, 80.0f));
    Fixture(4, EOGFoundationInteractionKind::Usable, TEXT("Activate"),
        FVector(-700.0f, 1000.0f, 45.0f), FVector(80.0f, 80.0f, 90.0f));
    Fixture(5, EOGFoundationInteractionKind::Station, TEXT("Use station"),
        FVector(-700.0f, 1500.0f, 50.0f), FVector(120.0f, 80.0f, 100.0f));
    Fixture(12, EOGFoundationInteractionKind::Inventory, TEXT("Inventory"),
        FVector(-700.0f, 2000.0f, 50.0f), FVector(120.0f, 80.0f, 100.0f));
    auto* Flight = Fixture(6, EOGFoundationInteractionKind::FlightStation,
        TEXT("Flight"), FVector(600.0f, 400.0f, 40.0f), FVector(80.0f));
    FlightStation = Flight;
    if (Flight)
    {
        TWeakObjectPtr<AOGFoundationIntegrationWorld> WeakThis(this);
        Flight->SetTemporaryFlight = [WeakThis](AOGWorldPrototypeCharacter* Pawn, bool bEnable)
        { return WeakThis.IsValid() && WeakThis->SetFlight(Pawn, bEnable); };
    }
    // A roofed, physically enterable room with a 250cm door opening.
    Box(FVector(2150.0f, 900.0f, 150.0f), FVector(1200.0f, 60.0f, 300.0f));
    Box(FVector(1550.0f, 1400.0f, 150.0f), FVector(60.0f, 1000.0f, 300.0f));
    Box(FVector(2750.0f, 1400.0f, 150.0f), FVector(60.0f, 1000.0f, 300.0f));
    Box(FVector(1780.0f, 1900.0f, 150.0f), FVector(460.0f, 60.0f, 300.0f));
    Box(FVector(2520.0f, 1900.0f, 150.0f), FVector(460.0f, 60.0f, 300.0f));
    Box(FVector(2150.0f, 1400.0f, 325.0f), FVector(1200.0f, 1000.0f, 50.0f));
    Fixture(7, EOGFoundationInteractionKind::Door, TEXT("Open / close door"),
        FVector(2150.0f, 1900.0f, 140.0f), FVector(250.0f, 60.0f, 280.0f));
    Fixture(8, EOGFoundationInteractionKind::BreakableBarrier, TEXT("Breakable barricade"),
        FVector(1200.0f, 2600.0f, 125.0f), FVector(60.0f, 350.0f, 250.0f));
    Fixture(9, EOGFoundationInteractionKind::StrongBarrier, TEXT("Sealed barricade"),
        FVector(1900.0f, 2600.0f, 125.0f), FVector(60.0f, 350.0f, 250.0f));
    auto* PointA = Fixture(10, EOGFoundationInteractionKind::TravelPoint,
        TEXT("Travel point A"), FVector(0.0f, -700.0f, 40.0f), FVector(80.0f));
    auto* PointB = Fixture(11, EOGFoundationInteractionKind::TravelPoint,
        TEXT("Travel point B"), FVector(3000.0f, 3000.0f, 40.0f), FVector(80.0f));
    if (PointA && PointB)
    {
        PointA->SetTravelPeer(PointB);
        PointB->SetTravelPeer(PointA);

        // Teleport is deliberately not a baseline mortal capability. The
        // integration course authors a paired travel-point exercise, so its
        // lifetime owns a temporary capability grant just like the flight
        // station owns temporary flight.
        if (UOGTraversalCapabilityComponent* Capabilities =
                Character->GetTraversalCapabilities())
        {
            bAddedTeleport =
                !Capabilities->HasCapability(FOGTraversalCapabilityIds::Teleport);
            if (bAddedTeleport)
            {
                Capabilities->GrantCapability(FOGTraversalCapabilityIds::Teleport);
                TravelCapabilityBorrower = Character;
            }
        }
    }

    for (const FVector& Wall : {FVector(-1800.0f, 200.0f, 160.0f),
        FVector(-1800.0f, 1200.0f, 450.0f)})
    {
        auto* Actor = GetWorld()->SpawnActor<AOGFoundationClimbableWall>(
            Position(Wall), FRotator::ZeroRotator);
        if (Actor)
        {
            Actor->SetActorScale3D(FVector(1.0f, 1.0f, Wall.Z / 160.0f));
            Own(Actor);
        }
    }
    Box(FVector(-1940.0f, 1200.0f, 885.0f), FVector(250.0f, 300.0f, 30.0f));
    // A second unroofed room provides a visible navigation/occlusion comparison.
    Box(FVector(500.0f, -600.0f, 150.0f), FVector(600.0f, 40.0f, 300.0f));
    Box(FVector(220.0f, -950.0f, 150.0f), FVector(40.0f, 700.0f, 300.0f));
    Box(FVector(780.0f, -950.0f, 150.0f), FVector(40.0f, 700.0f, 300.0f));
    Label(FVector(500.0f, -1250.0f, 340.0f), TEXT("OPEN ROOM: enter from south"));
    Box(FVector(-1800.0f, 2200.0f, 250.0f), FVector(60.0f, 300.0f, 500.0f));
    Label(FVector(-1650.0f, 1700.0f, 180.0f), TEXT("CLIMB: low | tall + ledge | smooth nonclimbable"));
    for (int32 Index = 0; Index != 2; ++Index)
    {
        auto* Carrier = GetWorld()->SpawnActor<AOGFoundationTraversalCarrier>(
            Position(FVector(300.0f + Index * 650.0f, 1000.0f, 30.0f)), FRotator::ZeroRotator);
        if (Carrier)
        {
            Carrier->SetFoundationCarrierMode(Index == 0 ? EOGTraversalMode::Mounted : EOGTraversalMode::Vehicle);
            Own(Carrier);
        }
    }
    Label(FVector(-2800.0f, 2600.0f, 180.0f), TEXT("DEEP SHAFT: fall stays valid inside declared XY bounds"));
    Label(FVector(0.0f, 0.0f, 200.0f), TEXT("FOUNDATION INTEGRATION | move + camera + jump + context interact + attack"));
    BuildScenery();
}
