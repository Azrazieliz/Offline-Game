#include "World/OGStartingRegionPresentation.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/InputComponent.h"

#include "Camera/CameraComponent.h"
#include "AudioDevice.h"
#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGDiagnosticCombatComponent.h"
#include "World/OGFoundationIntegrationWorld.h"
#include "Animation/OGDiagnosticAnimationPresentation.h"
#include "Runtime/OGSemanticAudioRuntime.h"
#include "Runtime/OGFoundationDiagnosticMenu.h"
#include "Runtime/OGCanonicalCharacterPresentation.h"
#include "Interaction/OGFoundationInteractionHost.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/TouchInterface.h"
#include "Gacha/OGGachaService.h"
#include "Kismet/BlueprintPlatformLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "OfflineGame.h"
#include "UI/OGUiViewModelService.h"
#include "Persistence/OGWorldStore.h"
#include "ProceduralMeshComponent.h"
#include "Runtime/OGAndroidBackupDocumentBridge.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Runtime/OGPlayerProfileSettings.h"
#include "Runtime/OGPresentationModeSubsystem.h"
#include "Runtime/OGReportService.h"
#include "Runtime/OGPackageManagerService.h"
#include "Persistence/OGRecoveryCatalogService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "World/OGWorldStateRecords.h"
#include "World/OGWorldModeInterfaces.h"
#include "World/OGTraversalFramework.h"

namespace
{
constexpr float SpawnClearRadius = 5200.0f;
constexpr float SiteClearRadius = 5000.0f;
constexpr float WorldCameraPitchMin = -42.0f;
constexpr float WorldCameraPitchMax = 28.0f;
constexpr float PerfectDodgePerceptionSlowdownSeconds = 2.5f;

bool AreFoundationDebugFixturesEnabled()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(
        FCommandLine::Get(),
        TEXT("FoundationFixtures"));
#endif
}

FString ReadJsonStringPreference(
    const FString& Json,
    const TCHAR* FieldName,
    const TCHAR* DefaultValue)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);
    if (FJsonSerializer::Deserialize(Reader, Root) &&
        Root.IsValid())
    {
        FString Value;
        if (Root->TryGetStringField(FieldName, Value) &&
            !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString(DefaultValue);
}

FString WriteJsonStringPreference(
    const FString& Json,
    const TCHAR* FieldName,
    const FString& Value)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) ||
        !Root.IsValid())
    {
        Root = MakeShared<FJsonObject>();
    }

    Root->SetStringField(FieldName, Value);

    FString Result;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(
        Root.ToSharedRef(),
        Writer);
    return Result;
}

FOGEntityId FoundationRulerProjectionId()
{
    // Stable presentation-query owner for Foundation shells. This ID is not
    // persisted or treated as canonical world state; real content may replace
    // the Ruler identity without changing the presentation contract.
    return FOGEntityId(
        FGuid(
            0x4f47464e,
            0x44525631,
            0x52554c45,
            0x5250524f));
}

FVector2D RotatePoint(const FVector2D& Point, float Radians)
{
    const float C = FMath::Cos(Radians);
    const float S = FMath::Sin(Radians);
    return FVector2D(
        Point.X * C - Point.Y * S,
        Point.X * S + Point.Y * C);
}

float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}
}

AOGStartingRegionGenerator::AOGStartingRegionGenerator()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
    Terrain->SetupAttachment(SceneRoot);
    Terrain->bUseComplexAsSimpleCollision = true;
    Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    RockInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Rocks"));
    RockInstances->SetupAttachment(SceneRoot);
    RockInstances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    TrunkInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeTrunks"));
    TrunkInstances->SetupAttachment(SceneRoot);
    TrunkInstances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    CanopyInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeCanopies"));
    CanopyInstances->SetupAttachment(SceneRoot);
    CanopyInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    StructureInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Structures"));
    StructureInstances->SetupAttachment(SceneRoot);
    StructureInstances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    RoofInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Roofs"));
    RoofInstances->SetupAttachment(SceneRoot);
    RoofInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RoadInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Roads"));
    RoadInstances->SetupAttachment(SceneRoot);
    RoadInstances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    AccentInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Accents"));
    AccentInstances->SetupAttachment(SceneRoot);
    AccentInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

    if (CubeMesh.Succeeded())
    {
        StructureInstances->SetStaticMesh(CubeMesh.Object);
        RoofInstances->SetStaticMesh(CubeMesh.Object);
        RoadInstances->SetStaticMesh(CubeMesh.Object);
        AccentInstances->SetStaticMesh(CubeMesh.Object);
    }
    if (SphereMesh.Succeeded())
    {
        RockInstances->SetStaticMesh(SphereMesh.Object);
    }
    if (CylinderMesh.Succeeded())
    {
        TrunkInstances->SetStaticMesh(CylinderMesh.Object);
    }
    if (ConeMesh.Succeeded())
    {
        CanopyInstances->SetStaticMesh(ConeMesh.Object);
    }
}

bool AOGStartingRegionGenerator::IsInsidePlayableBoundary_Implementation(
    const FVector& WorldLocation) const
{
    if (FoundationIntegrationBoundary.IsValid() &&
        FoundationIntegrationBoundary->GetClass()->ImplementsInterface(UOGPlayableBoundaryProvider::StaticClass()))
    {
        return IOGPlayableBoundaryProvider::Execute_IsInsidePlayableBoundary(
            FoundationIntegrationBoundary.Get(), WorldLocation);
    }
    const FVector LocalLocation =
        GetActorTransform().InverseTransformPosition(WorldLocation);
    return !FOGWorldSafetyPolicy::IsOutsidePlayableRegion(
        LocalLocation,
        GetPlayableHalfExtent(),
        TerrainCellSize);
}

bool AOGStartingRegionGenerator::IsInsideStableGroundRegion_Implementation(
    const FVector& WorldLocation) const
{
    if (FoundationIntegrationBoundary.IsValid() &&
        FoundationIntegrationBoundary->GetClass()->ImplementsInterface(UOGPlayableBoundaryProvider::StaticClass()))
    {
        return IOGPlayableBoundaryProvider::Execute_IsInsideStableGroundRegion(
            FoundationIntegrationBoundary.Get(), WorldLocation);
    }
    const FVector LocalLocation =
        GetActorTransform().InverseTransformPosition(WorldLocation);
    const float SafeHalfExtent =
        FOGWorldSafetyPolicy::GetSafeHalfExtent(
            GetPlayableHalfExtent(),
            TerrainCellSize);
    const float StableInset =
        FMath::Max(800.0f, TerrainCellSize);
    return FMath::Abs(LocalLocation.X) < SafeHalfExtent - StableInset &&
        FMath::Abs(LocalLocation.Y) < SafeHalfExtent - StableInset;
}

void AOGStartingRegionGenerator::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        ActiveSeed = EditorPreviewSeed;
        GenerateRegion(EditorPreviewSeed);
    }
}

void AOGStartingRegionGenerator::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGPresentationModeSubsystem* Presentation =
                GameInstance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Presentation->RegisterWorldPresentationActor(this);
        }
    }

    int32 Seed = EditorPreviewSeed;
    ResolveCanonicalSeed(Seed);
    ActiveSeed = Seed;

    GenerateRegion(Seed);
    PersistCanonicalLocations(Seed);

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Starting region presentation generated. Seed=%d GeneratorVersion=%d"),
        Seed,
        GeneratorVersion);
}

void AOGStartingRegionGenerator::RebuildPreview()
{
    ActiveSeed = EditorPreviewSeed;
    GenerateRegion(EditorPreviewSeed);
}

FOGEntityId AOGStartingRegionGenerator::StartingRegionMetadataId()
{
    return FOGEntityId(FGuid(
        0x4f475357u,
        0x52454749u,
        0x4f4e0001u,
        0x00000001u));
}

FOGEntityId AOGStartingRegionGenerator::MakeCanonicalLocationId(
    int32 Seed,
    uint32 Ordinal)
{
    const uint32 SafeSeed = static_cast<uint32>(Seed);
    return FOGEntityId(FGuid(
        0x4f474c4fu,
        SafeSeed ^ 0x9e3779b9u,
        0x43415449u ^ (Ordinal * 0x45d9f3bu),
        0x4f4e0000u | (Ordinal & 0xffffu)));
}

bool AOGStartingRegionGenerator::ResolveCanonicalSeed(int32& OutSeed)
{
    UGameInstance* GameInstance = GetGameInstance();
    UOGGameCoreSubsystem* Core = GameInstance
        ? GameInstance->GetSubsystem<UOGGameCoreSubsystem>()
        : nullptr;
    IOGWorldStore* Store = Core ? Core->GetWorldStore() : nullptr;

    if (!Store)
    {
        UE_LOG(
            LogOfflineGame,
            Warning,
            TEXT("Starting region could not access the authoritative store; using editor preview seed."));
        OutSeed = EditorPreviewSeed;
        return false;
    }

    const FOGEntityId MetadataId = StartingRegionMetadataId();
    bool bFound = false;
    FName Kind;
    FString StateJson;
    int64 Revision = 0;
    FString Error;

    if (!Store->TryReadEntity(
            MetadataId,
            bFound,
            Kind,
            StateJson,
            Revision,
            Error))
    {
        UE_LOG(LogOfflineGame, Warning, TEXT("Starting-region seed read failed: %s"), *Error);
        OutSeed = EditorPreviewSeed;
        return false;
    }

    if (bFound)
    {
        TSharedPtr<FJsonObject> Json;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StateJson);
        double SeedNumber = 0.0;
        double VersionNumber = 0.0;
        if (FJsonSerializer::Deserialize(Reader, Json) &&
            Json.IsValid() &&
            Json->TryGetNumberField(TEXT("seed"), SeedNumber) &&
            Json->TryGetNumberField(TEXT("generator_version"), VersionNumber) &&
            FMath::RoundToInt(VersionNumber) == GeneratorVersion)
        {
            OutSeed = FMath::Max(1, FMath::RoundToInt(SeedNumber));
            return true;
        }

        UE_LOG(
            LogOfflineGame,
            Warning,
            TEXT("Starting-region metadata exists but is not compatible with generator version %d; using preview seed without rewriting canonical metadata."),
            GeneratorVersion);
        OutSeed = EditorPreviewSeed;
        return false;
    }

    uint32 Hash = GetTypeHash(FGuid::NewGuid()) & 0x7fffffffu;
    if (Hash == 0)
    {
        Hash = static_cast<uint32>(EditorPreviewSeed);
    }
    OutSeed = static_cast<int32>(Hash);

    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("seed"), OutSeed);
    Json->SetNumberField(TEXT("generator_version"), GeneratorVersion);
    Json->SetStringField(TEXT("profile"), TEXT("starting_world_painterly_anime_pbr_v1"));

    FString NewStateJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NewStateJson);
    FJsonSerializer::Serialize(Json, Writer);

    if (!Store->UpsertEntity(
            MetadataId,
            FName(TEXT("world_region_generation")),
            0,
            NewStateJson,
            Error))
    {
        UE_LOG(LogOfflineGame, Warning, TEXT("Starting-region seed persistence failed: %s"), *Error);
        return false;
    }

    return true;
}

void AOGStartingRegionGenerator::PersistCanonicalLocations(int32 Seed)
{
    UGameInstance* GameInstance = GetGameInstance();
    UOGGameCoreSubsystem* Core = GameInstance
        ? GameInstance->GetSubsystem<UOGGameCoreSubsystem>()
        : nullptr;
    IOGWorldStore* Store = Core ? Core->GetWorldStore() : nullptr;

    if (!Store)
    {
        return;
    }

    const FOGEntityId RegionId = MakeCanonicalLocationId(Seed, 1);

    TArray<FOGLocationRecord> Locations;
    Locations.Reserve(7);

    FOGLocationRecord Region;
    Region.LocationId = RegionId;
    Region.Kind = FName(TEXT("starting_region"));
    Region.bPhysicallyAccessible = true;
    Locations.Add(Region);

    auto AddChild = [&Locations, &RegionId, Seed](
        uint32 Ordinal,
        const TCHAR* Kind)
    {
        FOGLocationRecord Location;
        Location.LocationId = MakeCanonicalLocationId(Seed, Ordinal);
        Location.ParentLocationId = RegionId;
        Location.Kind = FName(Kind);
        Location.bPhysicallyAccessible = true;
        Locations.Add(Location);
    };

    AddChild(2, TEXT("settlement"));
    AddChild(3, TEXT("dungeon"));
    AddChild(4, TEXT("territorial_creature_range"));
    AddChild(5, TEXT("faction_site"));
    AddChild(6, TEXT("faction_site"));
    AddChild(7, TEXT("faction_site"));

    FString Error;
    for (const FOGLocationRecord& Location : Locations)
    {
        if (!Store->UpsertLocation(Location, 0, Error))
        {
            UE_LOG(
                LogOfflineGame,
                Warning,
                TEXT("Failed to persist generated starting-world location %s: %s"),
                *Location.LocationId.ToString(),
                *Error);
            return;
        }
    }
}

void AOGStartingRegionGenerator::GenerateRegion(int32 Seed)
{
    ClearGeneratedContent();
    BuildLayout(Seed);
    ApplyPresentationMaterials();
    BuildTerrain(Seed);
    BuildRoads(Seed);
    BuildVegetationAndRocks(Seed);
    BuildSettlement(Seed);
    BuildFactionSites(Seed);
    BuildDungeonAndCreatureRange(Seed);
}

void AOGStartingRegionGenerator::ClearGeneratedContent()
{
    if (Terrain)
    {
        Terrain->ClearAllMeshSections();
    }

    UHierarchicalInstancedStaticMeshComponent* Components[] =
    {
        RockInstances,
        TrunkInstances,
        CanopyInstances,
        StructureInstances,
        RoofInstances,
        RoadInstances,
        AccentInstances
    };

    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
}

void AOGStartingRegionGenerator::BuildLayout(int32 Seed)
{
    FRandomStream Rng(Seed ^ 0x51a7d3);
    ActiveLayout.RotationRadians = Rng.FRandRange(-PI, PI);

    ActiveLayout.SettlementCenter = RotateLocal(FVector2D(20500.0f, 6500.0f));
    ActiveLayout.AuthoritySite = ActiveLayout.SettlementCenter + RotateLocal(FVector2D(5200.0f, 2800.0f));
    ActiveLayout.RivalSite = RotateLocal(FVector2D(-23500.0f, 16500.0f));
    ActiveLayout.ThirdPartySite = RotateLocal(FVector2D(12500.0f, -25500.0f));
    ActiveLayout.DungeonEntrance = RotateLocal(FVector2D(-17500.0f, -14500.0f));
    ActiveLayout.TerritorialCreatureRange = RotateLocal(FVector2D(-25000.0f, 4500.0f));
}

FVector2D AOGStartingRegionGenerator::RotateLocal(const FVector2D& Local) const
{
    return RotatePoint(Local, ActiveLayout.RotationRadians);
}

float AOGStartingRegionGenerator::SampleHeight(float X, float Y, int32 Seed) const
{
    const float Phase = static_cast<float>(Seed % 4093) * 0.0037f;

    float Height =
        FMath::Sin(X * 0.00017f + Phase) * 620.0f +
        FMath::Cos(Y * 0.00019f - Phase * 0.71f) * 480.0f +
        FMath::Sin((X + Y) * 0.000071f + Phase * 0.33f) * 760.0f +
        FMath::Cos((X - Y) * 0.000043f - Phase * 0.51f) * 520.0f;

    const float RidgeSignal =
        FMath::Abs(FMath::Sin(
            X * 0.000047f +
            Y * 0.000036f +
            Phase));
    Height += FMath::Pow(RidgeSignal, 4.0f) * 1150.0f;

    const float SpawnDistance = FVector2D(X, Y).Size();
    const float SpawnBlend = Smooth01((SpawnDistance - 2600.0f) / 4200.0f);
    Height *= SpawnBlend;

    auto FlattenAround = [&Height, X, Y](
        const FVector2D& Center,
        float Radius,
        float Target,
        float Strength)
    {
        const float Distance = FVector2D::Distance(FVector2D(X, Y), Center);
        const float Weight = 1.0f - Smooth01(Distance / Radius);
        Height = FMath::Lerp(Height, Target, Weight * Strength);
    };

    FlattenAround(ActiveLayout.SettlementCenter, 6200.0f, 90.0f, 0.92f);
    FlattenAround(ActiveLayout.AuthoritySite, 4200.0f, 180.0f, 0.75f);
    FlattenAround(ActiveLayout.RivalSite, 4700.0f, 260.0f, 0.78f);
    FlattenAround(ActiveLayout.ThirdPartySite, 4300.0f, 120.0f, 0.72f);
    FlattenAround(ActiveLayout.TerritorialCreatureRange, 5600.0f, 180.0f, 0.62f);

    return Height;
}

FVector AOGStartingRegionGenerator::ToSurface(
    const FVector2D& Point,
    float ZOffset) const
{
    return FVector(
        Point.X,
        Point.Y,
        SampleHeight(Point.X, Point.Y, ActiveSeed) + ZOffset);
}

float AOGStartingRegionGenerator::SampleFootprintGroundHeight(
    const FVector2D& Point,
    float YawDegrees,
    const FVector& Scale) const
{
    const float HalfX = FMath::Max(25.0f, FMath::Abs(Scale.X) * 50.0f);
    const float HalfY = FMath::Max(25.0f, FMath::Abs(Scale.Y) * 50.0f);
    const float YawRadians = FMath::DegreesToRadians(YawDegrees);

    TArray<float, TInlineAllocator<5>> Samples;
    Samples.Add(SampleHeight(Point.X, Point.Y, ActiveSeed));

    const FVector2D Corners[] =
    {
        FVector2D(-HalfX, -HalfY),
        FVector2D(-HalfX, HalfY),
        FVector2D(HalfX, -HalfY),
        FVector2D(HalfX, HalfY)
    };

    for (const FVector2D& Corner : Corners)
    {
        const FVector2D Rotated = RotatePoint(Corner, YawRadians);
        Samples.Add(
            SampleHeight(
                Point.X + Rotated.X,
                Point.Y + Rotated.Y,
                ActiveSeed));
    }

    Samples.Sort();

    // The sites are deliberately terrain-flattened. Median footprint height
    // ignores one anomalous corner and prevents a single high terrain sample
    // from lifting an entire house into the air.
    return Samples[Samples.Num() / 2];
}

FTransform AOGStartingRegionGenerator::GroundedBoxTransform(
    const FVector2D& Point,
    float YawDegrees,
    const FVector& Scale,
    float ExtraClearance) const
{
    const float GroundZ =
        SampleFootprintGroundHeight(
            Point,
            YawDegrees,
            Scale);
    const float HalfHeight =
        FMath::Abs(Scale.Z) * 50.0f;

    return FTransform(
        FRotator(0.0f, YawDegrees, 0.0f),
        FVector(
            Point.X,
            Point.Y,
            GroundZ + HalfHeight + ExtraClearance),
        Scale);
}

bool AOGStartingRegionGenerator::IsProtectedClearing(
    const FVector2D& Point) const
{
    if (Point.Size() < SpawnClearRadius)
    {
        return true;
    }

    const FVector2D Sites[] =
    {
        ActiveLayout.SettlementCenter,
        ActiveLayout.AuthoritySite,
        ActiveLayout.RivalSite,
        ActiveLayout.ThirdPartySite,
        ActiveLayout.DungeonEntrance,
        ActiveLayout.TerritorialCreatureRange
    };

    for (const FVector2D& Site : Sites)
    {
        if (FVector2D::Distance(Point, Site) < SiteClearRadius)
        {
            return true;
        }
    }

    return false;
}

void AOGStartingRegionGenerator::BuildTerrain(int32 Seed)
{
    const int32 Cells = FMath::Clamp(TerrainCellsPerSide, 24, 128);
    const float Cell = FMath::Clamp(TerrainCellSize, 400.0f, 2400.0f);
    const int32 VerticesPerSide = Cells + 1;
    const float HalfExtent = Cells * Cell * 0.5f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(VerticesPerSide * VerticesPerSide);
    Normals.Reserve(VerticesPerSide * VerticesPerSide);
    UV0.Reserve(VerticesPerSide * VerticesPerSide);
    Colors.Reserve(VerticesPerSide * VerticesPerSide);
    Tangents.Reserve(VerticesPerSide * VerticesPerSide);
    Triangles.Reserve(Cells * Cells * 6);

    for (int32 YIndex = 0; YIndex <= Cells; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex <= Cells; ++XIndex)
        {
            const float X = -HalfExtent + XIndex * Cell;
            const float Y = -HalfExtent + YIndex * Cell;
            const float Z = SampleHeight(X, Y, Seed);

            Vertices.Add(FVector(X, Y, Z));
            UV0.Add(FVector2D(X / 7200.0f, Y / 7200.0f));

            const float HeightL = SampleHeight(X - Cell, Y, Seed);
            const float HeightR = SampleHeight(X + Cell, Y, Seed);
            const float HeightD = SampleHeight(X, Y - Cell, Seed);
            const float HeightU = SampleHeight(X, Y + Cell, Seed);

            const float DX = (HeightR - HeightL) / (2.0f * Cell);
            const float DY = (HeightU - HeightD) / (2.0f * Cell);
            const FVector Normal = FVector(-DX, -DY, 1.0f).GetSafeNormal();
            Normals.Add(Normal);
            Tangents.Add(FProcMeshTangent(FVector(1.0f, 0.0f, DX), false));

            const float HeightTint = FMath::Clamp((Z + 1200.0f) / 3600.0f, 0.0f, 1.0f);
            Colors.Add(FLinearColor(
                0.13f + HeightTint * 0.10f,
                0.16f + HeightTint * 0.09f,
                0.12f + HeightTint * 0.08f,
                1.0f));
        }
    }

    for (int32 YIndex = 0; YIndex < Cells; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < Cells; ++XIndex)
        {
            const int32 A = YIndex * VerticesPerSide + XIndex;
            const int32 B = A + 1;
            const int32 C = A + VerticesPerSide;
            const int32 D = C + 1;

            Triangles.Add(A);
            Triangles.Add(B);
            Triangles.Add(C);

            Triangles.Add(B);
            Triangles.Add(D);
            Triangles.Add(C);
        }
    }

    Terrain->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        Colors,
        Tangents,
        true,
        false);


    TArray<int32> BackTriangles;
    BackTriangles.Reserve(Triangles.Num());
    for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
    {
        BackTriangles.Add(Triangles[Index]);
        BackTriangles.Add(Triangles[Index + 2]);
        BackTriangles.Add(Triangles[Index + 1]);
    }

    TArray<FVector> BackNormals;
    BackNormals.Reserve(Normals.Num());
    for (const FVector& Normal : Normals)
    {
        BackNormals.Add(-Normal);
    }

    Terrain->CreateMeshSection_LinearColor(
        1,
        Vertices,
        BackTriangles,
        BackNormals,
        UV0,
        Colors,
        Tangents,
        false,
        false);

    TArray<FVector> SkirtVertices;
    TArray<int32> SkirtTriangles;
    TArray<FVector> SkirtNormals;
    TArray<FVector2D> SkirtUVs;
    TArray<FLinearColor> SkirtColors;
    TArray<FProcMeshTangent> SkirtTangents;

    auto AddSkirtSegment =
        [&](int32 IndexA, int32 IndexB, const FVector& OutwardNormal)
        {
            const FVector TopA = Vertices[IndexA];
            const FVector TopB = Vertices[IndexB];
            FVector BottomA = TopA;
            FVector BottomB = TopB;
            BottomA.Z -= 6500.0f;
            BottomB.Z -= 6500.0f;

            const int32 Base = SkirtVertices.Num();
            SkirtVertices.Add(TopA);
            SkirtVertices.Add(TopB);
            SkirtVertices.Add(BottomA);
            SkirtVertices.Add(BottomB);

            for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
            {
                SkirtNormals.Add(OutwardNormal);
                SkirtUVs.Add(FVector2D(
                    VertexIndex == 1 || VertexIndex == 3 ? 1.0f : 0.0f,
                    VertexIndex >= 2 ? 1.0f : 0.0f));
                SkirtTangents.Add(
                    FProcMeshTangent(FVector(1.0f, 0.0f, 0.0f), false));
            }

            SkirtColors.Add(Colors[IndexA]);
            SkirtColors.Add(Colors[IndexB]);
            SkirtColors.Add(Colors[IndexA] * 0.55f);
            SkirtColors.Add(Colors[IndexB] * 0.55f);

            SkirtTriangles.Add(Base);
            SkirtTriangles.Add(Base + 1);
            SkirtTriangles.Add(Base + 2);
            SkirtTriangles.Add(Base + 1);
            SkirtTriangles.Add(Base + 3);
            SkirtTriangles.Add(Base + 2);
        };

    for (int32 XIndex = 0; XIndex < Cells; ++XIndex)
    {
        AddSkirtSegment(
            XIndex,
            XIndex + 1,
            FVector(0.0f, -1.0f, 0.0f));

        const int32 TopRow = Cells * VerticesPerSide;
        AddSkirtSegment(
            TopRow + XIndex + 1,
            TopRow + XIndex,
            FVector(0.0f, 1.0f, 0.0f));
    }

    for (int32 YIndex = 0; YIndex < Cells; ++YIndex)
    {
        const int32 Row = YIndex * VerticesPerSide;
        const int32 NextRow = (YIndex + 1) * VerticesPerSide;

        AddSkirtSegment(
            NextRow,
            Row,
            FVector(-1.0f, 0.0f, 0.0f));

        AddSkirtSegment(
            Row + Cells,
            NextRow + Cells,
            FVector(1.0f, 0.0f, 0.0f));
    }

    Terrain->CreateMeshSection_LinearColor(
        2,
        SkirtVertices,
        SkirtTriangles,
        SkirtNormals,
        SkirtUVs,
        SkirtColors,
        SkirtTangents,
        false,
        false);

    if (TerrainMaterial)
    {
        Terrain->SetMaterial(1, TerrainMaterial);
        Terrain->SetMaterial(2, TerrainMaterial);
    }
}

UMaterialInstanceDynamic* AOGStartingRegionGenerator::CreateTintedMaterial(
    UMaterialInterface* Parent,
    const FLinearColor& Color,
    float Roughness)
{
    if (!Parent)
    {
        return nullptr;
    }

    UMaterialInstanceDynamic* Material =
        UMaterialInstanceDynamic::Create(Parent, this);

    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    }

    return Material;
}

void AOGStartingRegionGenerator::ApplyPresentationMaterials()
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (!Parent)
    {
        return;
    }

    TerrainMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.19f, 0.22f, 0.17f, 1.0f),
        0.92f);
    RockMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.25f, 0.28f, 0.30f, 1.0f),
        0.96f);
    VegetationMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.10f, 0.16f, 0.105f, 1.0f),
        0.94f);
    WoodMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.16f, 0.105f, 0.075f, 1.0f),
        0.90f);
    StructureMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.39f, 0.37f, 0.33f, 1.0f),
        0.95f);
    RoofMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.13f, 0.145f, 0.16f, 1.0f),
        0.90f);
    RoadMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.20f, 0.19f, 0.16f, 1.0f),
        0.98f);
    AccentMaterial = CreateTintedMaterial(
        Parent,
        FLinearColor(0.35f, 0.035f, 0.045f, 1.0f),
        0.82f);

    Terrain->SetMaterial(0, TerrainMaterial);
    RockInstances->SetMaterial(0, RockMaterial);
    TrunkInstances->SetMaterial(0, WoodMaterial);
    CanopyInstances->SetMaterial(0, VegetationMaterial);
    StructureInstances->SetMaterial(0, StructureMaterial);
    RoofInstances->SetMaterial(0, RoofMaterial);
    RoadInstances->SetMaterial(0, RoadMaterial);
    AccentInstances->SetMaterial(0, AccentMaterial);
}

void AOGStartingRegionGenerator::BuildRoads(int32 Seed)
{
    const FVector2D Endpoints[] =
    {
        RotateLocal(FVector2D(34500.0f, 12500.0f)),
        RotateLocal(FVector2D(8500.0f, 35000.0f))
    };

    for (const FVector2D& End : Endpoints)
    {
        const FVector2D Start = ActiveLayout.SettlementCenter;
        const FVector2D Delta = End - Start;
        const float Distance = Delta.Size();
        const FVector2D Direction = Delta.GetSafeNormal();
        const int32 Segments = FMath::Max(1, FMath::CeilToInt(Distance / 750.0f));
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));

        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float Alpha = (Index + 0.5f) / static_cast<float>(Segments);
            const FVector2D Point = FMath::Lerp(Start, End, Alpha);
            const FVector Position = ToSurface(Point, 12.0f);
            const float SegmentLength = Distance / Segments;

            RoadInstances->AddInstance(FTransform(
                FRotator(0.0f, Yaw, 0.0f),
                Position,
                FVector(SegmentLength / 100.0f, 2.5f, 0.08f)));
        }
    }
}

void AOGStartingRegionGenerator::BuildVegetationAndRocks(int32 Seed)
{
    FRandomStream Rng(Seed ^ 0x2c13a95);
    const float HalfExtent = TerrainCellsPerSide * TerrainCellSize * 0.5f * 0.92f;

    int32 TreesPlaced = 0;
    for (int32 Attempt = 0; Attempt < 620 && TreesPlaced < 285; ++Attempt)
    {
        const FVector2D Point(
            Rng.FRandRange(-HalfExtent, HalfExtent),
            Rng.FRandRange(-HalfExtent, HalfExtent));

        if (IsProtectedClearing(Point))
        {
            continue;
        }

        const float H = SampleHeight(Point.X, Point.Y, Seed);
        const float HX = SampleHeight(Point.X + 600.0f, Point.Y, Seed);
        const float HY = SampleHeight(Point.X, Point.Y + 600.0f, Seed);
        if (FMath::Abs(HX - H) > 430.0f ||
            FMath::Abs(HY - H) > 430.0f)
        {
            continue;
        }

        const float TrunkHeight = Rng.FRandRange(320.0f, 590.0f);
        const float TrunkRadiusScale = Rng.FRandRange(0.22f, 0.38f);
        const float CanopyHeight = Rng.FRandRange(330.0f, 620.0f);
        const float CanopyWidth = Rng.FRandRange(0.85f, 1.55f);
        const float Yaw = Rng.FRandRange(0.0f, 360.0f);

        TrunkInstances->AddInstance(FTransform(
            FRotator(0.0f, Yaw, 0.0f),
            FVector(Point.X, Point.Y, H + TrunkHeight * 0.5f),
            FVector(
                TrunkRadiusScale,
                TrunkRadiusScale,
                TrunkHeight / 100.0f)));

        CanopyInstances->AddInstance(FTransform(
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                Point.X,
                Point.Y,
                H + TrunkHeight + CanopyHeight * 0.43f),
            FVector(
                CanopyWidth,
                CanopyWidth,
                CanopyHeight / 100.0f)));

        ++TreesPlaced;
    }

    int32 RocksPlaced = 0;
    for (int32 Attempt = 0; Attempt < 320 && RocksPlaced < 125; ++Attempt)
    {
        const FVector2D Point(
            Rng.FRandRange(-HalfExtent, HalfExtent),
            Rng.FRandRange(-HalfExtent, HalfExtent));

        if (Point.Size() < SpawnClearRadius * 0.72f)
        {
            continue;
        }

        const float Radius = Rng.FRandRange(0.45f, 2.35f);
        const float Flatten = Rng.FRandRange(0.55f, 1.25f);
        const FVector Position = ToSurface(Point, Radius * 32.0f);

        RockInstances->AddInstance(FTransform(
            FRotator(
                Rng.FRandRange(-18.0f, 18.0f),
                Rng.FRandRange(0.0f, 360.0f),
                Rng.FRandRange(-12.0f, 12.0f)),
            Position,
            FVector(Radius, Radius * 0.82f, Radius * Flatten)));

        ++RocksPlaced;
    }
}

void AOGStartingRegionGenerator::BuildSettlement(int32 Seed)
{
    FRandomStream Rng(Seed ^ 0x714f33);
    const FVector2D Center = ActiveLayout.SettlementCenter;

    for (int32 Index = 0; Index < 22; ++Index)
    {
        const float Radius = Rng.FRandRange(900.0f, 4300.0f);
        const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
        const FVector2D Point =
            Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;

        const float Width = Rng.FRandRange(1.8f, 3.1f);
        const float Depth = Rng.FRandRange(1.5f, 2.6f);
        const float HeightScale = Rng.FRandRange(1.8f, 3.1f);
        const float Yaw = Rng.FRandRange(0.0f, 360.0f);
        const FVector BodyScale(Width, Depth, HeightScale);
        const FTransform BodyTransform =
            GroundedBoxTransform(
                Point,
                Yaw,
                BodyScale);
        StructureInstances->AddInstance(BodyTransform);

        const FVector RoofScale(
            Width * 1.13f,
            Depth * 1.13f,
            0.28f);
        const float BodyTopZ =
            BodyTransform.GetLocation().Z +
            FMath::Abs(BodyScale.Z) * 50.0f;
        const float RoofHalfHeight =
            FMath::Abs(RoofScale.Z) * 50.0f;
        RoofInstances->AddInstance(FTransform(
            FRotator(0.0f, Yaw, 0.0f),
            FVector(
                Point.X,
                Point.Y,
                BodyTopZ + RoofHalfHeight),
            RoofScale));
    }

    const float HallYaw =
        FMath::RadiansToDegrees(
            ActiveLayout.RotationRadians);
    const FVector HallScale(4.2f, 3.2f, 4.1f);
    const FTransform HallTransform =
        GroundedBoxTransform(
            Center,
            HallYaw,
            HallScale);
    StructureInstances->AddInstance(HallTransform);

    const FVector HallRoofScale(4.6f, 3.6f, 0.34f);
    const float HallTopZ =
        HallTransform.GetLocation().Z +
        FMath::Abs(HallScale.Z) * 50.0f;
    RoofInstances->AddInstance(FTransform(
        FRotator(0.0f, HallYaw, 0.0f),
        FVector(
            Center.X,
            Center.Y,
            HallTopZ +
                FMath::Abs(HallRoofScale.Z) * 50.0f),
        HallRoofScale));

    AccentInstances->AddInstance(
        GroundedBoxTransform(
            Center + RotateLocal(FVector2D(500.0f, -340.0f)),
            HallYaw,
            FVector(0.12f, 0.75f, 2.1f)));
}

void AOGStartingRegionGenerator::BuildFactionSites(int32 Seed)
{
    const float BaseYaw = FMath::RadiansToDegrees(ActiveLayout.RotationRadians);

    // Established local authority: compact watch-site near the settlement.
    {
        const FVector2D Site = ActiveLayout.AuthoritySite;
        StructureInstances->AddInstance(
            GroundedBoxTransform(
                Site,
                BaseYaw + 12.0f,
                FVector(2.5f, 2.5f, 3.0f)));

        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const FVector2D Offset =
                RotateLocal(FVector2D(0.0f, Side * 420.0f));
            StructureInstances->AddInstance(
                GroundedBoxTransform(
                    Site + Offset,
                    BaseYaw,
                    FVector(1.2f, 0.35f, 1.2f)));
        }
    }

    // Rival power: austere hill fort with four strong vertical silhouettes.
    {
        const FVector2D Site = ActiveLayout.RivalSite;
        const FVector2D Corners[] =
        {
            FVector2D(-520.0f, -520.0f),
            FVector2D(-520.0f, 520.0f),
            FVector2D(520.0f, -520.0f),
            FVector2D(520.0f, 520.0f)
        };

        for (const FVector2D& Corner : Corners)
        {
            const FVector2D Point = Site + RotateLocal(Corner);
            StructureInstances->AddInstance(
                GroundedBoxTransform(
                    Point,
                    BaseYaw,
                    FVector(1.45f, 1.45f, 2.8f)));
        }

        StructureInstances->AddInstance(
            GroundedBoxTransform(
                Site,
                BaseYaw,
                FVector(5.0f, 3.1f, 1.2f)));
    }

    // Third party: open ceremonial/trade site rather than another fort.
    {
        const FVector2D Site = ActiveLayout.ThirdPartySite;
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const float Angle = 2.0f * PI * Index / 6.0f;
            const FVector2D Offset(
                FMath::Cos(Angle) * 720.0f,
                FMath::Sin(Angle) * 720.0f);
            const FVector2D Point = Site + RotateLocal(Offset);

            StructureInstances->AddInstance(
                GroundedBoxTransform(
                    Point,
                    BaseYaw + Index * 13.0f,
                    FVector(0.55f, 0.55f, 1.9f)));
        }

        AccentInstances->AddInstance(
            GroundedBoxTransform(
                Site,
                BaseYaw,
                FVector(0.22f, 0.22f, 2.45f)));
    }
}

void AOGStartingRegionGenerator::BuildDungeonAndCreatureRange(int32 Seed)
{
    const float BaseYaw = FMath::RadiansToDegrees(ActiveLayout.RotationRadians);

    // Region-valid early dungeon: a cave/old-workings mouth, not an endgame ruin.
    {
        const FVector2D Entrance = ActiveLayout.DungeonEntrance;
        const FVector2D ArchOffsets[] =
        {
            FVector2D(-440.0f, 0.0f),
            FVector2D(440.0f, 0.0f),
            FVector2D(-300.0f, 0.0f),
            FVector2D(300.0f, 0.0f),
            FVector2D(0.0f, 0.0f)
        };

        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ArchOffsets); ++Index)
        {
            const FVector2D Point =
                Entrance + RotateLocal(ArchOffsets[Index]);
            const float ZOffset = Index == 4 ? 540.0f : 260.0f;
            const FVector Scale =
                Index == 4
                    ? FVector(2.7f, 1.9f, 1.15f)
                    : FVector(1.6f, 1.45f, 2.4f);

            RockInstances->AddInstance(FTransform(
                FRotator(0.0f, BaseYaw, 0.0f),
                ToSurface(Point, ZOffset),
                Scale));
        }

        RoofInstances->AddInstance(
            GroundedBoxTransform(
                Entrance,
                BaseYaw,
                FVector(2.7f, 0.28f, 2.8f),
                5.0f));
    }

    // Optional territorial creature range: readable natural arena, not a tutorial boss gate.
    {
        const FVector2D Center = ActiveLayout.TerritorialCreatureRange;
        for (int32 Index = 0; Index < 13; ++Index)
        {
            const float Angle = 2.0f * PI * Index / 13.0f;
            const float Radius = 3600.0f + (Index % 3) * 260.0f;
            const FVector2D Point =
                Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;

            RockInstances->AddInstance(FTransform(
                FRotator(0.0f, Angle * 57.29578f, 0.0f),
                ToSurface(Point, 95.0f),
                FVector(1.15f, 0.82f, 1.65f + (Index % 4) * 0.23f)));
        }

        AccentInstances->AddInstance(FTransform(
            FRotator(0.0f, BaseYaw, 0.0f),
            ToSurface(Center + RotateLocal(FVector2D(900.0f, 250.0f)), 46.0f),
            FVector(1.5f, 0.55f, 0.12f)));
    }
}

AOGFoundationCombatTarget::AOGFoundationCombatTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(FName(TEXT("OG.WorldPresentation")));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetRelativeScale3D(FVector(0.72f, 0.72f, 1.25f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CylinderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        Mesh->SetMaterial(0, BasicMaterial.Object);
    }
}

bool AOGFoundationCombatTarget::CanBeTargeted_Implementation(
    AActor* Requester) const
{
    return Requester != nullptr && !IsDefeated();
}

FVector AOGFoundationCombatTarget::GetTargetPoint_Implementation(
    AActor* Requester) const
{
    return GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
}

float AOGFoundationCombatTarget::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    const float Applied =
        FMath::Clamp(DamageAmount, 0.0f, CurrentHealth);
    if (Applied <= 0.0f || IsDefeated())
    {
        return 0.0f;
    }

    CurrentHealth -= Applied;
    LastDamageAmount = Applied;

    const float HealthRatio =
        MaxHealth > KINDA_SMALL_NUMBER
            ? CurrentHealth / MaxHealth
            : 0.0f;
    Mesh->SetRelativeScale3D(
        FVector(
            0.72f,
            0.72f,
            FMath::Lerp(0.35f, 1.25f, HealthRatio)));

    if (IsDefeated())
    {
        SetActorEnableCollision(false);
    }

    return Applied;
}

void AOGFoundationCombatTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    AOGWorldPrototypeCharacter* Character =
        Cast<AOGWorldPrototypeCharacter>(
            UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!World || !Character || IsDefeated())
    {
        return;
    }

    const float Distance =
        FVector::Dist(
            GetActorLocation(),
            Character->GetActorLocation());
    if (Distance > 520.0f)
    {
        NextAttackSeconds = 0.0;
        Mesh->SetRelativeScale3D(
            FVector(
                0.72f,
                0.72f,
                FMath::Lerp(
                    0.35f,
                    1.25f,
                    CurrentHealth / MaxHealth)));
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (NextAttackSeconds <= 0.0)
    {
        NextAttackSeconds = Now + 1.35;
    }

    const double Remaining = NextAttackSeconds - Now;
    const float Pulse =
        Remaining > 0.0 && Remaining < 0.55
            ? 1.0f + 0.10f *
                FMath::Sin(
                    static_cast<float>(
                        (0.55 - Remaining) * 22.0))
            : 1.0f;
    const float HealthRatio =
        FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);
    Mesh->SetRelativeScale3D(
        FVector(
            0.72f * Pulse,
            0.72f * Pulse,
            FMath::Lerp(0.35f, 1.25f, HealthRatio) * Pulse));

    if (Now >= NextAttackSeconds)
    {
        UGameplayStatics::ApplyDamage(
            Character,
            10.0f,
            nullptr,
            this,
            nullptr);
        NextAttackSeconds = Now + 2.4;
    }
}

AOGFoundationInteractableMarker::AOGFoundationInteractableMarker()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(FName(TEXT("OG.WorldPresentation")));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.85f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
}

bool AOGFoundationInteractableMarker::CanInteract_Implementation(
    AActor* Interactor) const
{
    return Interactor != nullptr;
}

FText AOGFoundationInteractableMarker::GetInteractionPrompt_Implementation(
    AActor* Interactor) const
{
    return bActivated
        ? FText::FromString(TEXT("Foundation marker active"))
        : FText::FromString(TEXT("Activate foundation marker"));
}

void AOGFoundationInteractableMarker::Interact_Implementation(
    AActor* Interactor)
{
    bActivated = !bActivated;
    Mesh->SetRelativeScale3D(
        bActivated
            ? FVector(0.62f, 0.62f, 1.05f)
            : FVector(0.45f, 0.45f, 0.85f));
    AddActorLocalRotation(FRotator(0.0f, 30.0f, 0.0f));
}

AOGFoundationClimbableWall::AOGFoundationClimbableWall()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(FName(TEXT("OG.WorldPresentation")));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClimbWallMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetRelativeScale3D(FVector(0.45f, 3.0f, 3.2f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
}

bool AOGFoundationClimbableWall::CanClimb_Implementation(
    ACharacter* Climber,
    FVector HitLocation,
    FVector SurfaceNormal) const
{
    return Climber != nullptr;
}

bool AOGFoundationClimbableWall::CanInteract_Implementation(
    AActor* Interactor) const
{
    return Cast<AOGWorldPrototypeCharacter>(Interactor) != nullptr;
}

FText AOGFoundationClimbableWall::GetInteractionPrompt_Implementation(
    AActor* Interactor) const
{
    return FText::FromString(TEXT("Climb"));
}

void AOGFoundationClimbableWall::Interact_Implementation(
    AActor* Interactor)
{
    if (AOGWorldPrototypeCharacter* Character =
            Cast<AOGWorldPrototypeCharacter>(Interactor))
    {
        Character->TryBeginClimb();
    }
}

AOGFoundationTraversalCarrier::AOGFoundationTraversalCarrier()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(FName(TEXT("OG.WorldPresentation")));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarrierMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetRelativeScale3D(FVector(1.8f, 1.0f, 0.6f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
}

bool AOGFoundationTraversalCarrier::CanBoard_Implementation(
    ACharacter* Rider) const
{
    return Rider != nullptr && !CurrentRider.IsValid() &&
        (CarrierMode == EOGTraversalMode::Mounted ||
         CarrierMode == EOGTraversalMode::Vehicle);
}

EOGTraversalMode
AOGFoundationTraversalCarrier::GetCarrierTraversalMode_Implementation() const
{
    return CarrierMode;
}

FTransform AOGFoundationTraversalCarrier::GetRiderTransform_Implementation(ACharacter* Rider) const
{
    const float HalfHeight = Rider && Rider->GetCapsuleComponent()
        ? Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
    const float Height = Mesh->Bounds.BoxExtent.Z + HalfHeight + 4.0f;
    return FTransform(GetActorRotation(), GetActorLocation() + FVector(0.0f, 0.0f, Height), FVector::OneVector);
}

APawn* AOGFoundationTraversalCarrier::GetCarrierPawn_Implementation(
    ACharacter* Rider) const
{
    return nullptr;
}

void AOGFoundationTraversalCarrier::AddCarrierMovementInput_Implementation(ACharacter* Rider, FVector2D Input)
{
    if (CurrentRider.Get() == Rider) { PendingMovementInput += Input; }
}

void AOGFoundationTraversalCarrier::AddCarrierVerticalInput_Implementation(ACharacter* Rider, float Input)
{
    // These authored diagnostic carriers are terrestrial. A powered vertical
    // carrier would explicitly opt into that capability in its own definition.
}

void AOGFoundationTraversalCarrier::OnBoarded_Implementation(ACharacter* Rider)
{
    CurrentRider = Rider;
    Mesh->IgnoreActorWhenMoving(Rider, true);
}

void AOGFoundationTraversalCarrier::OnUnboarded_Implementation(ACharacter* Rider)
{
    Mesh->IgnoreActorWhenMoving(Rider, false);
    CurrentRider.Reset();
    PendingMovementInput = FVector2D::ZeroVector;
}

void AOGFoundationTraversalCarrier::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector2D Input = PendingMovementInput.GetClampedToMaxSize(1.0f);
    PendingMovementInput = FVector2D::ZeroVector;
    if (!FMath::IsNearlyZero(Input.SizeSquared()))
    {
        const FVector Delta = (GetActorRightVector() * Input.X +
            GetActorForwardVector() * Input.Y) * 520.0f * DeltaSeconds;
        AddActorWorldOffset(Delta, true);
    }
    VerticalVelocity += (GetWorld() ? GetWorld()->GetGravityZ() : -980.0f) * DeltaSeconds;
    FHitResult Ground;
    AddActorWorldOffset(FVector(0.0f, 0.0f, VerticalVelocity * DeltaSeconds), true, &Ground);
    if (Ground.bBlockingHit)
    {
        VerticalVelocity = 0.0f;
    }
}

bool AOGFoundationTraversalCarrier::CanInteract_Implementation(
    AActor* Interactor) const
{
    return CanBoard_Implementation(
        Cast<ACharacter>(Interactor));
}

FText AOGFoundationTraversalCarrier::GetInteractionPrompt_Implementation(
    AActor* Interactor) const
{
    return FText::FromString(
        CarrierMode == EOGTraversalMode::Vehicle
            ? TEXT("Enter vehicle")
            : TEXT("Mount"));
}

void AOGFoundationTraversalCarrier::Interact_Implementation(
    AActor* Interactor)
{
    if (AOGWorldPrototypeCharacter* Character =
            Cast<AOGWorldPrototypeCharacter>(Interactor))
    {
        Character->TryBoardCarrier(this);
    }
}

AOGFoundationSwimStation::AOGFoundationSwimStation()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(FName(TEXT("OG.WorldPresentation")));

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwimStationMesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.35f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Mesh->SetStaticMesh(CubeMesh.Object);
    }
}

bool AOGFoundationSwimStation::CanInteract_Implementation(
    AActor* Interactor) const
{
    return Cast<AOGWorldPrototypeCharacter>(Interactor) != nullptr;
}

FText AOGFoundationSwimStation::GetInteractionPrompt_Implementation(
    AActor* Interactor) const
{
    const AOGWorldPrototypeCharacter* Character =
        Cast<AOGWorldPrototypeCharacter>(Interactor);
    const UOGTraversalCapabilityComponent* Capabilities =
        Character
            ? Character->GetTraversalCapabilities()
            : nullptr;
    const EOGTraversalMode Mode =
        Capabilities
            ? Capabilities->GetTraversalMode()
            : EOGTraversalMode::Ground;

    if (Mode == EOGTraversalMode::Swimming)
    {
        return FText::FromString(TEXT("Dive"));
    }
    if (Mode == EOGTraversalMode::Diving)
    {
        return FText::FromString(TEXT("Exit swim / dive test"));
    }
    return FText::FromString(TEXT("Enter swim test"));
}

void AOGFoundationSwimStation::Interact_Implementation(
    AActor* Interactor)
{
    AOGWorldPrototypeCharacter* Character =
        Cast<AOGWorldPrototypeCharacter>(Interactor);
    if (!Character ||
        !Character->GetTraversalCapabilities())
    {
        return;
    }

    UOGTraversalCapabilityComponent* Capabilities =
        Character->GetTraversalCapabilities();
    const EOGTraversalMode Mode =
        Capabilities->GetTraversalMode();

    if (Mode == EOGTraversalMode::Ground)
    {
        if (Capabilities->HasCapability(
                FOGTraversalCapabilityIds::Swim))
        {
            Capabilities->SetTraversalMode(
                EOGTraversalMode::Swimming);
            Character->GetCharacterMovement()->SetMovementMode(
                MOVE_Swimming);
        }
    }
    else if (Mode == EOGTraversalMode::Swimming)
    {
        Character->SetDiving(true);
    }
    else if (Mode == EOGTraversalMode::Diving)
    {
        Character->SetDiving(false);
        Capabilities->SetTraversalMode(EOGTraversalMode::Ground);
        Character->GetCharacterMovement()->SetMovementMode(
            MOVE_Falling);
    }
}

AOGWorldPrototypeCharacter::AOGWorldPrototypeCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.0f, 620.0f, 0.0f);
    Movement->MaxWalkSpeed = 650.0f;
    Movement->MaxAcceleration = 1800.0f;
    Movement->BrakingDecelerationWalking = 1500.0f;
    Movement->JumpZVelocity = 520.0f;
    Movement->AirControl = 0.32f;
    Movement->MaxSwimSpeed = 520.0f;
    Movement->MaxFlySpeed = 900.0f;
    Movement->NavAgentProps.bCanSwim = true;

    TraversalCapabilities =
        CreateDefaultSubobject<UOGTraversalCapabilityComponent>(
            TEXT("TraversalCapabilities"));
    ActionRuntime =
        CreateDefaultSubobject<UOGWorldActionRuntimeComponent>(
            TEXT("WorldActionRuntime"));
    PartyRuntime =
        CreateDefaultSubobject<UOGWorldPartyRuntimeComponent>(
            TEXT("WorldPartyRuntime"));
    DiagnosticCombat = CreateDefaultSubobject<UOGDiagnosticCombatComponent>(TEXT("FoundationCombat"));
    DiagnosticCombat->ComponentTags.Add(TEXT("OG.PresentationPersistent"));
    DiagnosticAnimations = CreateDefaultSubobject<UOGDiagnosticAnimationPresentation>(TEXT("DiagnosticAnimations"));
    SemanticAudio = CreateDefaultSubobject<UOGSemanticAudioRuntime>(TEXT("SemanticAudio"));
    SemanticAudio->ComponentTags.Add(TEXT("OG.PresentationPersistent"));
    DiagnosticMenu = CreateDefaultSubobject<UOGFoundationDiagnosticMenu>(TEXT("DiagnosticMenu"));
    InteractionHost = CreateDefaultSubobject<UOGFoundationInteractionHost>(TEXT("InteractionHost"));
    CanonicalPresentation = CreateDefaultSubobject<UOGCanonicalCharacterPresentation>(TEXT("CanonicalCharacterPresentation"));
    CanonicalPresentation->bUseNeutralDiagnosticTint = true;
    Movement->NavAgentProps.bCanSwim = TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Swim);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetCapsuleComponent());
    CameraBoom->TargetArmLength = 680.0f;
    CameraBoom->TargetOffset = FVector(0.0f, 0.0f, 55.0f);
    CameraBoom->ProbeSize = 16.0f;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    CameraBoom->SocketOffset = FVector(0.0f, 48.0f, 82.0f);
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 14.0f;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 18.0f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    FollowCamera->FieldOfView = 80.0f;
    // The fixed-light diagnostic course does not need eye-adaptation from
    // the black Ruler screen. Use a stable exposure on the first world frame.
    FollowCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    FollowCamera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    FollowCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    FollowCamera->PostProcessSettings.AutoExposureBias = 0.0f;
    FollowCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    FollowCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;

    // Engine-supplied articulated proof rig. Capsule remains the only pawn
    // collision body; rig proportions and facing must stay stable for diagnosis.
    PrototypeBody = GetMesh();
    PrototypeBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrototypeBody->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    PrototypeBody->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    PrototypeBody->SetRelativeScale3D(FVector::OneVector);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> DiagnosticRig(
        TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> DiagnosticLocomotion(
        TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_AnimBlueprint"));
    if (DiagnosticRig.Succeeded())
    {
        PrototypeBody->SetSkeletalMesh(DiagnosticRig.Object);
    }
    if (DiagnosticLocomotion.Succeeded())
    {
        PrototypeBody->SetAnimInstanceClass(DiagnosticLocomotion.Class);
    }
}

void AOGWorldPrototypeCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGPresentationModeSubsystem* Presentation =
                GameInstance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Presentation->RegisterWorldPresentationActor(this);
        }
    }

    DiagnosticMenu->OnCanonicalCharacterChanged.AddUObject(this, &AOGWorldPrototypeCharacter::RefreshCanonicalPresentation);
    InteractionHost->OnCharacterPresentation.AddUObject(this, &AOGWorldPrototypeCharacter::ApplyInteractionCharacterPresentation);
    DiagnosticAnimations->BindMesh(GetMesh());
    if (auto* GI = GetGameInstance())
        if (auto* Core = GI->GetSubsystem<UOGGameCoreSubsystem>())
            Core->OnCanonicalRuntimeReleasing.AddUObject(this, &AOGWorldPrototypeCharacter::ShutdownFoundationRuntime);
    DiagnosticCombat->OnControlledCharacterChanged.AddUObject(this, &AOGWorldPrototypeCharacter::UpdateProtagonistRepresentation);
    DiagnosticCombat->OnCombatPoseRequested.AddUObject(this, &AOGWorldPrototypeCharacter::PresentAcceptedCombatAction);
    CacheStartingRegion();
    LoadControlProfile();
    ConfigureFoundationParty();
    CreateFoundationTouchInterface();
    GroundSnapAttempts = 0;
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Flying);
    }
    GetWorldTimerManager().SetTimerForNextTick(
        this,
        &AOGWorldPrototypeCharacter::TrySnapToGeneratedGround);

    if (Controller)
    {
        FRotator ViewRotation = Controller->GetControlRotation();
        ViewRotation.Pitch = -10.0f;
        Controller->SetControlRotation(ViewRotation);

        if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
        {
            if (PlayerController->PlayerCameraManager)
            {
                PlayerController->PlayerCameraManager->ViewPitchMin = WorldCameraPitchMin;
                PlayerController->PlayerCameraManager->ViewPitchMax = WorldCameraPitchMax;
            }
        }
    }

    ensureMsgf(
        PrototypeBody && PrototypeBody->GetSkeletalMeshAsset() &&
            PrototypeBody->GetAnimInstance(),
        TEXT("Foundation diagnostic humanoid and locomotion must be available."));
}


void AOGWorldPrototypeCharacter::LoadControlProfile()
{
    FOGPlayerProfileSettings Profile;
    FString Error;
    if (!FOGPlayerProfileSettingsService::Load(
            FOGPlayerProfileSettingsService::DefaultProfilePath(),
            Profile,
            Error))
    {
        UE_LOG(
            LogOfflineGame,
            Warning,
            TEXT("World Mode control profile load failed; using defaults. Error=%s"),
            *Error);
        Profile = FOGPlayerProfileSettings();
    }

    CameraHorizontalSensitivity =
        FMath::Clamp(Profile.CameraHorizontalSensitivity, 0.35f, 2.50f);
    CameraVerticalSensitivity =
        FMath::Clamp(Profile.CameraVerticalSensitivity, 0.30f, 2.00f);
    CameraResponseExponent =
        FMath::Clamp(Profile.CameraResponseExponent, 1.0f, 2.5f);
    bInvertCameraX = Profile.bInvertCameraX;
    bInvertCameraY = Profile.bInvertCameraY;
    bSprintTogglePreference = Profile.bSprintToggle;
    bLeftHandedControls = Profile.bLeftHandedControls;
    TouchControlScale = Profile.TouchControlScale;
    TouchControlOpacity = Profile.TouchControlOpacity;
    MovementDeadzone = Profile.MovementDeadzone;
    LookDeadzone = Profile.LookDeadzone;
    MovementStickInset = Profile.MovementStickInset;
    MovementStickBottom = Profile.MovementStickBottom;
    ActionClusterHorizontalOffset =
        Profile.ActionClusterHorizontalOffset;
    ActionClusterVerticalOffset =
        Profile.ActionClusterVerticalOffset;
    bSfwPresentation = Profile.bSfwPresentation;
    RosterDensity =
        Profile.RosterDensity.IsNone()
            ? FName(TEXT("dense"))
            : Profile.RosterDensity;
    CinematicRepeatPolicy =
        FName(*ReadJsonStringPreference(
            Profile.CinematicPreferencesJson,
            TEXT("ultimate_cinematics"),
            TEXT("first_time")));
    bAutoDownload = Profile.bAutoDownload;
    bLargeDownloadsUnmeteredOnly =
        ReadJsonStringPreference(
            Profile.NetworkPreferencesJson,
            TEXT("large_downloads"),
            TEXT("unmetered_only")) !=
        TEXT("any_network");
    bReducedMotion = Profile.bReducedMotion;
    bReducedCameraShake = Profile.bReducedCameraShake;
    bSubtitlesEnabled = Profile.bSubtitlesEnabled;
    SubtitlePresentation = Profile.SubtitlePresentation;
    UiReadabilityProfile = Profile.UiReadabilityProfile;
    ColorVisionProfile = Profile.ColorVisionProfile;
    bHapticsEnabled = Profile.bHapticsEnabled;
    HapticsIntensity = Profile.HapticsIntensity;
    MasterVolume = Profile.MasterVolume;
    MusicVolume = Profile.MusicVolume;
    VoiceVolume = Profile.VoiceVolume;
    SfxVolume = Profile.SfxVolume;
    AmbienceVolume = Profile.AmbienceVolume;
    DynamicRangeProfile = Profile.DynamicRangeProfile;
    DamageNumberPresentation = Profile.DamageNumberPresentation;
    OrientationOverride = Profile.OrientationLock.IsNone()
        ? FName(TEXT("automatic"))
        : Profile.OrientationLock;

    if (CameraBoom)
    {
        CameraBoom->bEnableCameraLag = !bReducedMotion;
        CameraBoom->bEnableCameraRotationLag =
            !bReducedMotion && !bReducedCameraShake;
    }

    ApplyAudioAndSubtitlePresentationSettings();
    SemanticAudio->ConfigureRoutes(FoundationMusicSoundClass, FoundationVoiceSoundClass, FoundationSfxSoundClass, FoundationAmbienceSoundClass);
    SemanticAudio->ApplyProfile(Profile);
    CanonicalPresentation->SetPrivacyPresentation(bSfwPresentation);
    if (ProtagonistRepresentation)
        if (auto* Presenter = ProtagonistRepresentation->FindComponentByClass<UOGCanonicalCharacterPresentation>())
            Presenter->SetPrivacyPresentation(bSfwPresentation);
}

void AOGWorldPrototypeCharacter::ApplyAudioAndSubtitlePresentationSettings()
{
    UGameplayStatics::SetSubtitlesEnabled(
        bSubtitlesEnabled);

    if (GEngine)
    {
        FAudioDeviceHandle AudioDevice =
            GEngine->GetMainAudioDevice();
        if (AudioDevice.IsValid())
        {
            AudioDevice->SetTransientPrimaryVolume(
                FMath::Clamp(MasterVolume, 0.0f, 1.0f));
        }
    }

    if (!FoundationVolumeMix)
    {
        FoundationVolumeMix =
            LoadObject<USoundMix>(
                nullptr,
                TEXT("/Game/Foundation/Audio/SM_FoundationVolumes.SM_FoundationVolumes"));
        if (!FoundationVolumeMix)
        {
            FoundationVolumeMix =
                NewObject<USoundMix>(
                    this,
                    TEXT("FoundationRuntimeVolumeMix"));
        }
    }

    auto ResolveSemanticClass =
        [this](
            TObjectPtr<USoundClass>& Slot,
            const TCHAR* AssetPath,
            const TCHAR* RuntimeName)
        {
            if (!Slot)
            {
                Slot = LoadObject<USoundClass>(
                    nullptr,
                    AssetPath);
                if (!Slot)
                {
                    Slot =
                        NewObject<USoundClass>(
                            this,
                            FName(RuntimeName));
                }
            }
            return Slot.Get();
        };

    USoundClass* MusicClass =
        ResolveSemanticClass(
            FoundationMusicSoundClass,
            TEXT("/Game/Foundation/Audio/SC_Music.SC_Music"),
            TEXT("FoundationMusic"));
    USoundClass* VoiceClass =
        ResolveSemanticClass(
            FoundationVoiceSoundClass,
            TEXT("/Game/Foundation/Audio/SC_Voice.SC_Voice"),
            TEXT("FoundationVoice"));
    USoundClass* SfxClass =
        ResolveSemanticClass(
            FoundationSfxSoundClass,
            TEXT("/Game/Foundation/Audio/SC_SFX.SC_SFX"),
            TEXT("FoundationSFX"));
    USoundClass* AmbienceClass =
        ResolveSemanticClass(
            FoundationAmbienceSoundClass,
            TEXT("/Game/Foundation/Audio/SC_Ambience.SC_Ambience"),
            TEXT("FoundationAmbience"));

    const TCHAR* DynamicMixPaths[] =
    {
        TEXT("/Game/Foundation/Audio/SM_DynamicFull.SM_DynamicFull"),
        TEXT("/Game/Foundation/Audio/SM_DynamicNight.SM_DynamicNight"),
        TEXT("/Game/Foundation/Audio/SM_DynamicCompressed.SM_DynamicCompressed")
    };
    const TCHAR* SelectedDynamicMixPath =
        DynamicRangeProfile == FName(TEXT("night"))
            ? DynamicMixPaths[1]
            : DynamicRangeProfile == FName(TEXT("compressed"))
                ? DynamicMixPaths[2]
                : DynamicMixPaths[0];
    USoundMix* SelectedAuthoredDynamicMix =
        LoadObject<USoundMix>(
            nullptr,
            SelectedDynamicMixPath);

    float MusicDynamicMultiplier = 1.0f;
    float VoiceDynamicMultiplier = 1.0f;
    float SfxDynamicMultiplier = 1.0f;
    float AmbienceDynamicMultiplier = 1.0f;

    // If content has not supplied a proper DSP mix yet, preserve a real
    // functional night/compressed fallback through semantic category gains.
    // Once an authored mix exists it takes over, avoiding double-processing.
    if (!SelectedAuthoredDynamicMix)
    {
        if (DynamicRangeProfile == FName(TEXT("night")))
        {
            MusicDynamicMultiplier = 0.86f;
            SfxDynamicMultiplier = 0.68f;
            AmbienceDynamicMultiplier = 0.76f;
        }
        else if (DynamicRangeProfile == FName(TEXT("compressed")))
        {
            MusicDynamicMultiplier = 0.92f;
            SfxDynamicMultiplier = 0.82f;
            AmbienceDynamicMultiplier = 0.88f;
        }
    }

    if (FoundationVolumeMix)
    {
        UGameplayStatics::SetBaseSoundMix(
            this,
            FoundationVolumeMix);

        struct FVolumeRoute
        {
            USoundClass* SoundClass = nullptr;
            float Volume = 1.0f;
        };
        const FVolumeRoute Routes[] =
        {
            { MusicClass, MusicVolume * MusicDynamicMultiplier },
            { VoiceClass, VoiceVolume * VoiceDynamicMultiplier },
            { SfxClass, SfxVolume * SfxDynamicMultiplier },
            { AmbienceClass, AmbienceVolume * AmbienceDynamicMultiplier }
        };

        for (const FVolumeRoute& Route : Routes)
        {
            if (!Route.SoundClass)
            {
                continue;
            }

            // Runtime fallback classes need registration on this world's device.
            // Keep asset base volume authored; the mix contributes exactly one gain.
            if (Route.SoundClass->GetOuter() == this && GetWorld())
            {
                FAudioDeviceHandle Device = GetWorld()->GetAudioDevice();
                if (Device.IsValid())
                {
                    Device->RegisterSoundClass(Route.SoundClass);
                }
            }
            UGameplayStatics::SetSoundMixClassOverride(
                this,
                FoundationVolumeMix,
                Route.SoundClass,
                FMath::Clamp(Route.Volume, 0.0f, 1.0f),
                1.0f,
                0.05f,
                true);
        }
    }

    if (OwnedDynamicRangeMix != SelectedAuthoredDynamicMix)
    {
        if (OwnedDynamicRangeMix) UGameplayStatics::PopSoundMixModifier(this, OwnedDynamicRangeMix);
        OwnedDynamicRangeMix = SelectedAuthoredDynamicMix;
        if (OwnedDynamicRangeMix) UGameplayStatics::PushSoundMixModifier(this, OwnedDynamicRangeMix);
    }

}

bool AOGWorldPrototypeCharacter::LoadMutablePlayerProfile(
    FOGPlayerProfileSettings& OutProfile) const
{
    FString Error;
    if (FOGPlayerProfileSettingsService::Load(
            FOGPlayerProfileSettingsService::DefaultProfilePath(),
            OutProfile,
            Error))
    {
        return true;
    }

    UE_LOG(
        LogOfflineGame,
        Warning,
        TEXT("Player profile load failed while editing settings; defaults will be used. Error=%s"),
        *Error);
    OutProfile = FOGPlayerProfileSettings();
    return false;
}

void AOGWorldPrototypeCharacter::SaveMutablePlayerProfileAndApply(
    const FOGPlayerProfileSettings& Profile)
{
    FOGPlayerProfileSettings UpgradedProfile = Profile;
    UpgradedProfile.Version =
        FMath::Max(UpgradedProfile.Version, 4);

    FString Error;
    if (!FOGPlayerProfileSettingsService::Save(
            FOGPlayerProfileSettingsService::DefaultProfilePath(),
            UpgradedProfile,
            Error))
    {
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Player profile settings save failed. Error=%s"),
            *Error);
        return;
    }

    LoadControlProfile();
    LastTouchViewportSize = FIntPoint::ZeroValue;
    RefreshFoundationTouchInterfaceIfNeeded();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGPresentationModeSubsystem* Presentation =
                GameInstance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Presentation->SetMode(Presentation->GetMode());
        }
    }
}

AActor* AOGWorldPrototypeCharacter::GetContextInteractableForHud() const
{
    // Rider exit remains discoverable after leaving the boarding station.
    return CurrentCarrier.IsValid() ? CurrentCarrier.Get() : FindBestInteractable();
}

bool AOGWorldPrototypeCharacter::IsFoundationWorldInputAllowed() const
{
    if (bFoundationSettingsOpen || bFoundationPauseMenuOpen ||
        (InteractionHost && InteractionHost->IsActive()) ||
        (DiagnosticCombat && DiagnosticCombat->IsInTurn())) return false;
    if (const auto* PC = Cast<APlayerController>(Controller))
        if (const auto* Hud = Cast<AOGWorldPresentationHud>(PC->GetHUD()))
            if (!Hud->AllowsWorldGameplayInput()) return false;
    if (const UGameInstance* Instance = GetGameInstance())
        if (const auto* Mode = Instance->GetSubsystem<UOGPresentationModeSubsystem>())
            if (Mode->GetMode() != EOGPresentationMode::World) return false;
    return true;
}

void AOGWorldPrototypeCharacter::RefreshFoundationInputState()
{
    auto* PC = Cast<APlayerController>(Controller);
    const bool bAllow = IsFoundationWorldInputAllowed();
    // Ignore-input flags are counters. Release only the suppression we own.
    if (bOwnsFoundationInputSuppression &&
        (bAllow || FoundationInputController.Get() != PC))
    {
        if (auto* Previous = FoundationInputController.Get())
        {
            Previous->SetIgnoreMoveInput(false);
            Previous->SetIgnoreLookInput(false);
        }
        bOwnsFoundationInputSuppression = false;
        FoundationInputController.Reset();
    }
    if (!bAllow && PC && !bOwnsFoundationInputSuppression)
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        FoundationInputController = PC;
        bOwnsFoundationInputSuppression = true;
        bCameraTouchActive = false;
        bTraversalAscendHeld = bTraversalDescendHeld = false;
        bSprinting = false;
        StopJumping();
        ConsumeMovementInputVector();
        if (auto* Movement = GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->MaxWalkSpeed = 650.0f;
        }
        if (auto* Hud = Cast<AOGWorldPresentationHud>(PC->GetHUD()))
            Hud->PressedWorldControls.Reset();
    }
    if (PC) PC->SetVirtualJoystickVisibility(bAllow);
}

void AOGWorldPrototypeCharacter::SetFoundationSettingsOpen(bool bOpen)
{
    bFoundationSettingsOpen = bOpen;
    RefreshFoundationInputState();
}

void AOGWorldPrototypeCharacter::SetFoundationPauseMenuOpen(bool bOpen)
{
    bFoundationPauseMenuOpen = bOpen;
    RefreshFoundationInputState();
    if (GetWorld()) UGameplayStatics::SetGamePaused(GetWorld(), bOpen);
}

void AOGWorldPrototypeCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime, OutResult);
    // Keep spring-arm collision intact. Hide the owner's body only when the
    // collision-shortened view is inside its silhouette; restore on retreat.
    const FVector RelativeCamera = OutResult.Location - GetActorLocation();
    const bool bInsideBody = RelativeCamera.SizeSquared2D() < FMath::Square(180.0f) &&
        FMath::Abs(RelativeCamera.Z) < GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 100.0f;
    TInlineComponentArray<USkinnedMeshComponent*> Bodies;
    GetComponents(Bodies);
    for (auto* Body : Bodies)
        if (Body == GetMesh() || Body->ComponentHasTag(TEXT("OG.DiagnosticPose")))
            Body->SetOwnerNoSee(bInsideBody);
}

void AOGWorldPrototypeCharacter::SetFoundationSettingsPage(int32 Page)
{
    FoundationSettingsPage = FMath::Clamp(Page, 0, 3);
}

void AOGWorldPrototypeCharacter::AdjustCameraHorizontalSensitivity(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.CameraHorizontalSensitivity =
        FMath::Clamp(CameraHorizontalSensitivity + Delta, 0.35f, 2.50f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustCameraVerticalSensitivity(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.CameraVerticalSensitivity =
        FMath::Clamp(CameraVerticalSensitivity + Delta, 0.30f, 2.00f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustCameraResponseExponent(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.CameraResponseExponent =
        FMath::Clamp(CameraResponseExponent + Delta, 1.0f, 2.5f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleInvertCameraX()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bInvertCameraX = !bInvertCameraX;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleInvertCameraY()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bInvertCameraY = !bInvertCameraY;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleSprintPreference()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bSprintToggle = !bSprintTogglePreference;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleOrientationOverride()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);

    const FName Current = OrientationOverride.IsNone()
        ? FName(TEXT("automatic"))
        : OrientationOverride;

    if (Current == FName(TEXT("automatic")))
    {
        Profile.OrientationLock = FName(TEXT("portrait"));
    }
    else if (Current == FName(TEXT("portrait")))
    {
        Profile.OrientationLock = FName(TEXT("landscape"));
    }
    else if (Current == FName(TEXT("landscape")))
    {
        Profile.OrientationLock = FName(TEXT("sensor"));
    }
    else
    {
        Profile.OrientationLock = FName(TEXT("automatic"));
    }

    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleLeftHandedControls()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bLeftHandedControls = !bLeftHandedControls;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustTouchControlScale(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.TouchControlScale =
        FMath::Clamp(TouchControlScale + Delta, 0.75f, 1.40f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustTouchControlOpacity(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.TouchControlOpacity =
        FMath::Clamp(TouchControlOpacity + Delta, 0.30f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustMovementDeadzone(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.MovementDeadzone =
        FMath::Clamp(MovementDeadzone + Delta, 0.0f, 0.35f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustLookDeadzone(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.LookDeadzone =
        FMath::Clamp(LookDeadzone + Delta, 0.0f, 0.25f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustMovementStickInset(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.MovementStickInset =
        FMath::Clamp(MovementStickInset + Delta, 0.06f, 0.34f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustMovementStickBottom(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.MovementStickBottom =
        FMath::Clamp(MovementStickBottom + Delta, 0.08f, 0.36f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustActionClusterHorizontalOffset(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.ActionClusterHorizontalOffset =
        FMath::Clamp(
            ActionClusterHorizontalOffset + Delta,
            -0.08f,
            0.12f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustActionClusterVerticalOffset(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.ActionClusterVerticalOffset =
        FMath::Clamp(
            ActionClusterVerticalOffset + Delta,
            -0.12f,
            0.12f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleSfwPresentation()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bSfwPresentation = !bSfwPresentation;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleRosterDensity()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (RosterDensity == FName(TEXT("dense")))
    {
        Profile.RosterDensity = FName(TEXT("comfortable"));
    }
    else if (RosterDensity == FName(TEXT("comfortable")))
    {
        Profile.RosterDensity = FName(TEXT("compact"));
    }
    else
    {
        Profile.RosterDensity = FName(TEXT("dense"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleCinematicRepeatPolicy()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);

    FString Next(TEXT("first_time"));
    if (CinematicRepeatPolicy == FName(TEXT("first_time")))
    {
        Next = TEXT("always");
    }
    else if (CinematicRepeatPolicy == FName(TEXT("always")))
    {
        Next = TEXT("skip_repeats");
    }

    Profile.CinematicPreferencesJson =
        WriteJsonStringPreference(
            Profile.CinematicPreferencesJson,
            TEXT("ultimate_cinematics"),
            Next);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleAutoDownload()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bAutoDownload = !bAutoDownload;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleLargeDownloadsUnmeteredOnly()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.NetworkPreferencesJson =
        WriteJsonStringPreference(
            Profile.NetworkPreferencesJson,
            TEXT("large_downloads"),
            bLargeDownloadsUnmeteredOnly
                ? TEXT("any_network")
                : TEXT("unmetered_only"));
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleReducedMotion()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bReducedMotion = !bReducedMotion;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleReducedCameraShake()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bReducedCameraShake = !bReducedCameraShake;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleSubtitles()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bSubtitlesEnabled = !bSubtitlesEnabled;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleSubtitlePresentation()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (SubtitlePresentation == FName(TEXT("standard")))
    {
        Profile.SubtitlePresentation = FName(TEXT("large"));
    }
    else if (SubtitlePresentation == FName(TEXT("large")))
    {
        Profile.SubtitlePresentation = FName(TEXT("high_contrast"));
    }
    else
    {
        Profile.SubtitlePresentation = FName(TEXT("standard"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleUiReadabilityProfile()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (UiReadabilityProfile == FName(TEXT("standard")))
    {
        Profile.UiReadabilityProfile = FName(TEXT("large_text"));
    }
    else if (UiReadabilityProfile == FName(TEXT("large_text")))
    {
        Profile.UiReadabilityProfile = FName(TEXT("high_contrast"));
    }
    else
    {
        Profile.UiReadabilityProfile = FName(TEXT("standard"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleColorVisionProfile()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (ColorVisionProfile == FName(TEXT("standard")))
    {
        Profile.ColorVisionProfile = FName(TEXT("deuteranopia"));
    }
    else if (ColorVisionProfile == FName(TEXT("deuteranopia")))
    {
        Profile.ColorVisionProfile = FName(TEXT("protanopia"));
    }
    else if (ColorVisionProfile == FName(TEXT("protanopia")))
    {
        Profile.ColorVisionProfile = FName(TEXT("tritanopia"));
    }
    else
    {
        Profile.ColorVisionProfile = FName(TEXT("standard"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::ToggleHaptics()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.bHapticsEnabled = !bHapticsEnabled;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustHapticsIntensity(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.HapticsIntensity =
        FMath::Clamp(HapticsIntensity + Delta, 0.0f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustMasterVolume(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.MasterVolume = FMath::Clamp(MasterVolume + Delta, 0.0f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustMusicVolume(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.MusicVolume = FMath::Clamp(MusicVolume + Delta, 0.0f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustVoiceVolume(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.VoiceVolume = FMath::Clamp(VoiceVolume + Delta, 0.0f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustSfxVolume(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.SfxVolume = FMath::Clamp(SfxVolume + Delta, 0.0f, 1.0f);
    Profile.EffectsVolume = Profile.SfxVolume;
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::AdjustAmbienceVolume(float Delta)
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    Profile.AmbienceVolume =
        FMath::Clamp(AmbienceVolume + Delta, 0.0f, 1.0f);
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleDynamicRangeProfile()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (DynamicRangeProfile == FName(TEXT("full")))
    {
        Profile.DynamicRangeProfile = FName(TEXT("night"));
    }
    else if (DynamicRangeProfile == FName(TEXT("night")))
    {
        Profile.DynamicRangeProfile = FName(TEXT("compressed"));
    }
    else
    {
        Profile.DynamicRangeProfile = FName(TEXT("full"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CycleDamageNumberPresentation()
{
    FOGPlayerProfileSettings Profile;
    LoadMutablePlayerProfile(Profile);
    if (DamageNumberPresentation == FName(TEXT("standard")))
    {
        Profile.DamageNumberPresentation = FName(TEXT("compact"));
    }
    else if (DamageNumberPresentation == FName(TEXT("compact")))
    {
        Profile.DamageNumberPresentation = FName(TEXT("off"));
    }
    else
    {
        Profile.DamageNumberPresentation = FName(TEXT("standard"));
    }
    SaveMutablePlayerProfileAndApply(Profile);
}

void AOGWorldPrototypeCharacter::CreateFoundationTouchInterface()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    APlayerController* PlayerController =
        Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    JumpReleased(); TraversalDownReleased(); SprintReleased();
    bCameraTouchActive = false;
    FoundationTouchInterface =
        NewObject<UTouchInterface>(this);
    if (!FoundationTouchInterface)
    {
        return;
    }

    UTexture2D* Thumb = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Thumb.VirtualJoystick_Thumb"));
    UTexture2D* Background = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Engine/MobileResources/HUD/VirtualJoystick_Background.VirtualJoystick_Background"));

    int32 ViewWidth = 0;
    int32 ViewHeight = 0;
    PlayerController->GetViewportSize(ViewWidth, ViewHeight);
    const float RelativeXForSquare =
        ViewWidth > 0 && ViewHeight > 0
            ? static_cast<float>(ViewHeight) / static_cast<float>(ViewWidth)
            : 1.0f;

    const float ShortSideFraction = ViewHeight > 0
        ? static_cast<float>(FMath::Min(ViewWidth, ViewHeight)) / ViewHeight : 1.0f;
    auto SquareRelative =
        [RelativeXForSquare, ShortSideFraction, this](float RelativeSize)
        {
            const float ScaledSize =
                RelativeSize * ShortSideFraction * FMath::Clamp(TouchControlScale, 0.9f, 1.1f);
            return FVector2D(
                ScaledSize * RelativeXForSquare,
                ScaledSize);
        };

    auto ActionCenter =
        [this](float HorizontalInset, float BottomInset)
        {
            const float X =
                FMath::Clamp(
                    HorizontalInset + ActionClusterHorizontalOffset,
                    0.06f,
                    0.46f);
            const float Y =
                FMath::Clamp(
                    BottomInset + ActionClusterVerticalOffset,
                    0.08f,
                    0.48f);
            return FVector2D(
                bLeftHandedControls ? X : -X,
                -Y);
        };

    auto MakeButton =
        [&](int32 TextureIndex,
            const FVector2D& Center,
            const FKey& Key,
            float VisualSize = 0.055f,
            float InteractionSize = 0.034f)
        {
            // The engine MobileHUDButton textures are very wide pills on modern
            // ultrawide phones. Reuse the circular joystick art for compact,
            // aspect-ratio-stable Foundation action controls instead.
            (void)TextureIndex;
            FTouchInputControl Button;
            Button.bTreatAsButton = true;
            Button.Image1 = Background;
            Button.Image2 = Thumb;
            Button.Center = Center;
            Button.VisualSize = SquareRelative(VisualSize);
            Button.ThumbSize = SquareRelative(VisualSize);
            Button.InteractionSize = SquareRelative(InteractionSize);
            Button.InputScale = FVector2D(1.0f, 1.0f);
            Button.MainInputKey = Key;
            return Button;
        };

    FTouchInputControl LeftStick;
    LeftStick.Image1 = Thumb;
    LeftStick.Image2 = Background;
    const FVector2D StickTouchSize = SquareRelative(0.20f);
    const float StickInsetX = FMath::Clamp(MovementStickInset,
        StickTouchSize.X * 0.5f + 0.015f, 0.30f);
    const float StickInsetY = FMath::Clamp(MovementStickBottom,
        StickTouchSize.Y * 0.5f + 0.015f, 0.30f);
    LeftStick.Center = FVector2D(
        bLeftHandedControls ? -StickInsetX : StickInsetX, -StickInsetY);
    // TouchInterface relative X and Y sizes are measured against different
    // viewport dimensions. Aspect-correct them so the movement stick is a true
    // screen-space X/Y circle instead of a landscape-stretched X/Z-looking oval.
    LeftStick.VisualSize = SquareRelative(0.15f);
    LeftStick.ThumbSize = SquareRelative(0.065f);
    LeftStick.InteractionSize = SquareRelative(0.20f);
    LeftStick.InputScale = FVector2D(1.0f, 1.0f);
    LeftStick.MainInputKey = EKeys::Gamepad_LeftX;
    LeftStick.AltInputKey = EKeys::Gamepad_LeftY;

    const FTouchInputControl AttackButton =
        MakeButton(
            1,
            ActionCenter(0.105f, 0.125f),
            EKeys::Gamepad_RightTrigger,
            0.064f,
            0.038f);
    const FTouchInputControl DodgeButton =
        MakeButton(
            2,
            ActionCenter(0.185f, 0.195f),
            EKeys::Gamepad_FaceButton_Right);
    const FTouchInputControl JumpButton =
        MakeButton(
            3,
            ActionCenter(0.105f, 0.245f),
            EKeys::Gamepad_FaceButton_Bottom);
    const FTouchInputControl LockButton =
        MakeButton(
            2,
            ActionCenter(0.285f, 0.205f),
            EKeys::Gamepad_RightThumbstick,
            0.047f,
            0.032f);
    const FTouchInputControl SprintButton =
        MakeButton(
            3,
            ActionCenter(0.265f, 0.285f),
            EKeys::Gamepad_LeftThumbstick,
            0.047f,
            0.032f);

    // Keep only actions that are universally meaningful in the Foundation
    // shell. Contextual actions (Interact, traversal-down, QTE, party switch,
    // skills, Ultimate and Aim) are drawn as HUD controls only when the
    // current state actually supports them; dead permanent buttons are not
    // allowed on mobile.
    FoundationTouchInterface->Controls.Reset();
    FoundationTouchInterface->Controls.Add(LeftStick);
    FoundationTouchInterface->ActiveOpacity = TouchControlOpacity;
    FoundationTouchInterface->InactiveOpacity =
        FMath::Clamp(TouchControlOpacity * 0.45f, 0.18f, 0.50f);
    FoundationTouchInterface->TimeUntilDeactive = 0.25f;
    FoundationTouchInterface->TimeUntilReset = 0.0f;
    FoundationTouchInterface->ActivationDelay = 0.0f;
    FoundationTouchInterface->StartupDelay = 0.0f;
    FoundationTouchInterface->bPreventRecenter = true;
    LastTouchViewportSize = FIntPoint(ViewWidth, ViewHeight);

    PlayerController->ActivateTouchInterface(FoundationTouchInterface);

    RefreshFoundationInputState();

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Foundation mobile touch profile activated: fixed X/Y movement, camera drag and only universally valid actions."));
#endif
}

void AOGWorldPrototypeCharacter::RefreshFoundationTouchInterfaceIfNeeded()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    APlayerController* PlayerController =
        Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return;
    }

    int32 ViewWidth = 0;
    int32 ViewHeight = 0;
    PlayerController->GetViewportSize(ViewWidth, ViewHeight);
    const FIntPoint CurrentSize(ViewWidth, ViewHeight);
    if (ViewWidth <= 0 || ViewHeight <= 0 ||
        CurrentSize == LastTouchViewportSize)
    {
        return;
    }

    // Rebuild all aspect-correct touch geometry after portrait/landscape or
    // window-size changes. This prevents controls from retaining stale
    // landscape proportions when the player uses the sensor orientation lock.
    CreateFoundationTouchInterface();
#endif
}

float AOGWorldPrototypeCharacter::ShapeLookInput(float Value) const
{
    const float Magnitude = FMath::Clamp(FMath::Abs(Value), 0.0f, 1.0f);
    return FMath::Sign(Value) *
        FMath::Pow(Magnitude, CameraResponseExponent);
}


bool AOGWorldPrototypeCharacter::IsCameraDragTouch(
    const FVector& ScreenLocation) const
{
    const APlayerController* PlayerController =
        Cast<APlayerController>(Controller);
    if (!PlayerController)
    {
        return false;
    }

    int32 Width = 0;
    int32 Height = 0;
    PlayerController->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0)
    {
        return false;
    }

    const float NormalizedX =
        ScreenLocation.X / static_cast<float>(Width);
    const float NormalizedY =
        ScreenLocation.Y / static_cast<float>(Height);

    if (!IsFoundationWorldInputAllowed())
    {
        return false;
    }

    // Keep the top-right Settings hit target out of camera input. Action
    // controls start below the upper camera field, so the two systems never
    // compete for the same finger.
    const bool bCameraSide =
        bLeftHandedControls
            ? NormalizedX <= 0.58f
            : NormalizedX >= 0.42f;
    return bCameraSide &&
        NormalizedY <= 0.55f;
}

void AOGWorldPrototypeCharacter::TouchPressed(
    ETouchIndex::Type FingerIndex,
    FVector Location)
{
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (AHUD* Hud = PC->GetHUD())
        {
            if (Hud->GetHitBoxAtCoordinates(FVector2D(Location.X, Location.Y), true)) return;
        }
    }
    if (bCameraTouchActive ||
        !IsCameraDragTouch(Location))
    {
        return;
    }

    bCameraTouchActive = true;
    CameraTouchIndex = FingerIndex;
    LastCameraTouchPosition =
        FVector2D(Location.X, Location.Y);
    CameraTouchStartPosition =
        LastCameraTouchPosition;
}

void AOGWorldPrototypeCharacter::TouchMoved(
    ETouchIndex::Type FingerIndex,
    FVector Location)
{
    if (!IsFoundationWorldInputAllowed() || !bCameraTouchActive ||
        FingerIndex != CameraTouchIndex ||
        !Controller)
    {
        return;
    }

    const FVector2D Current(Location.X, Location.Y);
    const FVector2D Delta =
        Current - LastCameraTouchPosition;
    LastCameraTouchPosition = Current;

    // Touch uses the same precision-vs-speed intent as gamepad look: tiny
    // drags are damped for aiming, while fast swipes receive enough gain to
    // turn quickly without requiring repeated full-screen gestures.
    auto ShapeTouchDelta =
        [this](float PixelDelta)
        {
            const float Magnitude = FMath::Abs(PixelDelta);
            if (Magnitude <= FMath::Max(
                    1.0f,
                    LookDeadzone * 22.0f))
            {
                return 0.0f;
            }

            const float Normalized =
                FMath::Clamp(Magnitude / 22.0f, 0.0f, 1.0f);
            const float Gain =
                FMath::Lerp(
                    0.55f,
                    1.45f,
                    FMath::Pow(Normalized, CameraResponseExponent));
            return PixelDelta * Gain;
        };

    constexpr float YawDegreesPerPixel = 0.145f;
    constexpr float PitchDegreesPerPixel = 0.12f;

    const float YawDirection =
        bInvertCameraX ? -1.0f : 1.0f;
    const float PitchDirection =
        bInvertCameraY ? 1.0f : -1.0f;

    AddControllerYawInput(
        ShapeTouchDelta(Delta.X) *
        YawDegreesPerPixel *
        CameraHorizontalSensitivity *
        YawDirection);
    AddControllerPitchInput(
        ShapeTouchDelta(Delta.Y) *
        PitchDegreesPerPixel *
        CameraVerticalSensitivity *
        PitchDirection);
}

void AOGWorldPrototypeCharacter::TouchReleased(
    ETouchIndex::Type FingerIndex,
    FVector Location)
{
    if (!bCameraTouchActive ||
        FingerIndex != CameraTouchIndex)
    {
        return;
    }

    const FVector2D End(Location.X, Location.Y);
    const float DragDistance =
        FVector2D::Distance(
            CameraTouchStartPosition,
            End);

    bCameraTouchActive = false;

    // A short right-side tap is also the direct-touch target-selection
    // contract required by World Mode. Dragging never changes the lock.
    if (IsFoundationWorldInputAllowed() && DragDistance <= 22.0f)
    {
        if (APlayerController* PlayerController =
                Cast<APlayerController>(Controller))
        {
            FHitResult Hit;
            if (PlayerController->GetHitResultAtScreenPosition(
                    End,
                    ECC_Visibility,
                    true,
                    Hit) &&
                Hit.GetActor() &&
                Hit.GetActor()->GetClass()->ImplementsInterface(
                    UOGWorldTargetable::StaticClass()) &&
                IOGWorldTargetable::Execute_CanBeTargeted(
                    Hit.GetActor(),
                    this))
            {
                HardLockedTarget = Hit.GetActor();
                OnHardLockChanged(Hit.GetActor());
            }
        }
    }
}

AActor* AOGWorldPrototypeCharacter::FindBestTarget(
    float MaxDistance) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const FVector Origin = GetActorLocation();
    const FVector Forward = Controller
        ? Controller->GetControlRotation().Vector()
        : GetActorForwardVector();

    AActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate ||
            Candidate == this ||
            !Candidate->GetClass()->ImplementsInterface(
                UOGWorldTargetable::StaticClass()) ||
            !IOGWorldTargetable::Execute_CanBeTargeted(
                Candidate,
                const_cast<AOGWorldPrototypeCharacter*>(this)))
        {
            continue;
        }

        const FVector TargetPoint =
            IOGWorldTargetable::Execute_GetTargetPoint(
                Candidate,
                const_cast<AOGWorldPrototypeCharacter*>(this));
        const FVector ToTarget =
            TargetPoint - Origin;
        const float Distance = ToTarget.Size();
        if (Distance <= KINDA_SMALL_NUMBER ||
            Distance > MaxDistance)
        {
            continue;
        }

        const float Facing =
            FVector::DotProduct(
                Forward.GetSafeNormal(),
                ToTarget / Distance);
        if (Facing < -0.10f)
        {
            continue;
        }

        const float Score =
            Distance * (1.20f - 0.55f * Facing);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Candidate;
        }
    }

    return Best;
}

AActor* AOGWorldPrototypeCharacter::FindBestInteractable(
    float MaxDistance) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const FVector Origin = GetActorLocation();
    const FVector Forward = GetActorForwardVector();

    AActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate ||
            Candidate == this ||
            !Candidate->GetClass()->ImplementsInterface(
                UOGWorldInteractable::StaticClass()) ||
            !IOGWorldInteractable::Execute_CanInteract(
                Candidate,
                const_cast<AOGWorldPrototypeCharacter*>(this)))
        {
            continue;
        }

        const FVector ToTarget =
            Candidate->GetActorLocation() - Origin;
        const float Distance = ToTarget.Size();
        if (Distance > MaxDistance)
        {
            continue;
        }

        const float Facing =
            Distance > KINDA_SMALL_NUMBER
                ? FVector::DotProduct(
                    Forward,
                    ToTarget / Distance)
                : 1.0f;
        if (Facing < 0.15f || !IsInteractionLineClear(Candidate))
        {
            continue;
        }

        const float Score =
            Distance * (1.15f - 0.35f * Facing);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Candidate;
        }
    }

    return Best;
}

void AOGWorldPrototypeCharacter::SprintPressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    bSprinting = bSprintTogglePreference
        ? !bSprinting
        : true;

    if (UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        Movement->MaxWalkSpeed =
            bSprinting ? 980.0f : 650.0f;
    }
}

void AOGWorldPrototypeCharacter::SprintReleased()
{
    if (bSprintTogglePreference)
    {
        return;
    }

    bSprinting = false;
    if (UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = 650.0f;
    }
}

void AOGWorldPrototypeCharacter::DodgePressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    UWorld* World = GetWorld();
    UCharacterMovementComponent* Movement =
        GetCharacterMovement();
    if (!World || !Movement)
    {
        return;
    }

    if (!PrepareActionCancel(
            EOGActionCancelDestination::Dodge))
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (Now < NextDodgeAllowedSeconds)
    {
        return;
    }

    FVector Direction =
        GetLastMovementInputVector();
    Direction.Z = 0.0f;
    if (Direction.IsNearlyZero())
    {
        Direction = GetActorForwardVector();
        Direction.Z = 0.0f;
    }
    Direction.Normalize();

    bDodging = true;
    if (DiagnosticAnimations) DiagnosticAnimations->PresentTimedAction(TEXT("Dodge"), .34f);
    DodgeInvulnerableUntilSeconds = Now + 0.24;
    DodgeEndsAtSeconds = Now + 0.34;
    NextDodgeAllowedSeconds = Now + 0.48;

    const float ExistingVerticalVelocity =
        Movement->Velocity.Z;
    Movement->Velocity =
        Direction * 1125.0f;
    Movement->Velocity.Z =
        ExistingVerticalVelocity;

    if (bHapticsEnabled)
    {
        if (APlayerController* PlayerController =
                Cast<APlayerController>(Controller))
        {
            PlayerController->PlayDynamicForceFeedback(
                HapticsIntensity * 0.35f,
                0.045f,
                true,
                true,
                true,
                true);
        }
    }

    UE_LOG(
        LogOfflineGame,
        Verbose,
        TEXT("World Mode dodge started. InvulnerableUntil=%.3f"),
        DodgeInvulnerableUntilSeconds);
}

bool AOGWorldPrototypeCharacter::IsDodgeInvulnerable() const
{
    return bDodging &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() <=
            DodgeInvulnerableUntilSeconds;
}

float AOGWorldPrototypeCharacter::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (IsDodgeInvulnerable())
    {
        NotifyPerfectDodge();
        return 0.0f;
    }

    if (DiagnosticCombat && DiagnosticCombat->IsActive())
    {
        if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0) return 0;
        const double Amount = DamageAmount;
        const int32 Exponent = FMath::FloorToInt(FMath::LogX(10.0, Amount)) - 8;
        const FOGLargeNumber Resolved(FMath::RoundToInt64(Amount / FMath::Pow(10.0, Exponent)), Exponent);
        FString Error;
        if (!DiagnosticCombat->ApplyResolvedWorldDamage(Resolved, Error)) { DiagnosticCombat->SetStatus(Error); return 0; }
        DiagnosticAnimations->PresentTimedAction(TEXT("Hit"), .24f);
        return DamageAmount;
    }
    const float Applied =
        FMath::Clamp(DamageAmount, 0.0f, FoundationHealth);
    if (Applied <= 0.0f)
    {
        return 0.0f;
    }

    FoundationHealth -= Applied;
    return Applied;
}

void AOGWorldPrototypeCharacter::NotifyPerfectDodge()
{
    if (!IsDodgeInvulnerable() || !GetWorld())
    {
        return;
    }

    bFoundationSlowMotionActive = true;
    PerfectDodgeSlowEndsAtRealSeconds =
        GetWorld()->GetRealTimeSeconds() +
        PerfectDodgePerceptionSlowdownSeconds;
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.35f);

    if (bHapticsEnabled)
    {
        if (APlayerController* PlayerController =
                Cast<APlayerController>(Controller))
        {
            PlayerController->PlayDynamicForceFeedback(
                HapticsIntensity * 0.85f,
                0.12f,
                true,
                true,
                true,
                true);
        }
    }

    OnPerfectDodgeTriggered(PerfectDodgePerceptionSlowdownSeconds);

    UE_LOG(
        LogOfflineGame,
        Log,
        TEXT("Perfect dodge: damage negated; perception slowdown active for %.2f real seconds; no universal reward granted."),
        PerfectDodgePerceptionSlowdownSeconds);
}

void AOGWorldPrototypeCharacter::EndDodgeIfNeeded()
{
    if (!bDodging ||
        !GetWorld())
    {
        return;
    }

    if (GetWorld()->GetTimeSeconds() >=
        DodgeEndsAtSeconds)
    {
        bDodging = false;
    }
}

void AOGWorldPrototypeCharacter::UpdateFoundationCombatPresentation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    // Action timing is independent of the temporary visual asset. Never squash
    // or rotate the whole humanoid to fake an attack: that masks facing/rig bugs.
    if (Now > AttackVisualEndsAtSeconds &&
        ActionRuntime && ActionRuntime->HasActiveAction())
    {
        ActionRuntime->EndAuthoredAction();
    }

    if (bFoundationSlowMotionActive &&
        World->GetRealTimeSeconds() >=
            PerfectDodgeSlowEndsAtRealSeconds)
    {
        UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
        bFoundationSlowMotionActive = false;
    }

    if (RecentDamageExpiresAtSeconds > 0.0 &&
        Now > RecentDamageExpiresAtSeconds)
    {
        RecentFoundationDamage = 0.0f;
        RecentDamageExpiresAtSeconds = 0.0;
    }
}

void AOGWorldPrototypeCharacter::ConfigureFoundationParty()
{
    if (!PartyRuntime)
    {
        return;
    }

    auto MakeUnit =
        [](const FOGEntityId& EntityId,
           const TCHAR* IdentityContentId,
           bool bActive)
        {
            FOGCombatUnitState Unit;
            Unit.UnitEntityId = EntityId;
            Unit.IdentityId = FOGContentId(FString(IdentityContentId));
            Unit.TeamIndex = 0;
            Unit.Presence =
                bActive
                    ? EOGCombatPresence::Active
                    : EOGCombatPresence::Reserve;
            Unit.Stats.MaxHp = FOGLargeNumber::FromInt64(100);
            Unit.Stats.Attack = FOGLargeNumber::FromInt64(10);
            Unit.CurrentHp = FOGLargeNumber::FromInt64(100);
            return Unit;
        };

    TArray<FOGCombatUnitState> Units;
    Units.Add(
        MakeUnit(
            FoundationRulerProjectionId(),
            TEXT("foundation:protagonist"),
            true));

    // The real opening has no pre-granted companions. Synthetic companions
    // exist only behind an explicit non-shipping fixture flag for automated
    // or manual engineering checks.
    if (AreFoundationDebugFixturesEnabled())
    {
        Units.Add(
            MakeUnit(
                FOGEntityId::NewId(),
                TEXT("foundation:companion_a"),
                false));
        Units.Add(
            MakeUnit(
                FOGEntityId::NewId(),
                TEXT("foundation:companion_b"),
                false));
    }

    FString Error;
    if (!PartyRuntime->ConfigureParty(Units, Error))
    {
        UE_LOG(
            LogOfflineGame,
            Error,
            TEXT("Foundation World party setup failed. Error=%s"),
            *Error);
    }
}

int32 AOGWorldPrototypeCharacter::GetPartySlotCountForHud() const
{
    return PartyRuntime ? PartyRuntime->GetSlots().Num() : 0;
}

int32 AOGWorldPrototypeCharacter::GetControlledPartySlotForHud() const
{
    return PartyRuntime ? PartyRuntime->GetControlledSlot() : INDEX_NONE;
}

bool AOGWorldPrototypeCharacter::IsPartySlotAvailableForHud(int32 SlotIndex) const
{
    if (!PartyRuntime || !PartyRuntime->GetSlots().IsValidIndex(SlotIndex))
    {
        return false;
    }
    const FOGWorldPartySlot& Slot = PartyRuntime->GetSlots()[SlotIndex];
    return Slot.bAvailable && !Slot.bDefeated;
}

bool AOGWorldPrototypeCharacter::IsPartySlotDefeatedForHud(int32 SlotIndex) const
{
    return PartyRuntime &&
        PartyRuntime->GetSlots().IsValidIndex(SlotIndex) &&
        PartyRuntime->GetSlots()[SlotIndex].bDefeated;
}

bool AOGWorldPrototypeCharacter::IsPartySlotQteReadyForHud(int32 SlotIndex) const
{
    return PartyRuntime &&
        PartyRuntime->GetSlots().IsValidIndex(SlotIndex) &&
        PartyRuntime->GetSlots()[SlotIndex].bQteReady &&
        PartyRuntime->GetSlots()[SlotIndex].bAvailable &&
        !PartyRuntime->GetSlots()[SlotIndex].bDefeated;
}

void AOGWorldPrototypeCharacter::TriggerPartySwitchFromHud(int32 SlotIndex)
{
    if (!IsFoundationWorldInputAllowed()) return;
    RequestPartySwitch(SlotIndex);
}

void AOGWorldPrototypeCharacter::TriggerQteFromHud(int32 SlotIndex)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!PartyRuntime || !IsPartySlotQteReadyForHud(SlotIndex) ||
        !PrepareActionCancel(EOGActionCancelDestination::Switch))
    {
        return;
    }

    AActor* Target = HardLockedTarget.IsValid() ? HardLockedTarget.Get() : FindBestTarget();
    UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>();
    FString Error;
    if (Combat && Combat->IsActive() && !Combat->CanAcceptQte(SlotIndex, Target, Error))
    { Combat->SetStatus(Error); return; }
    if (PartyRuntime->TryTriggerQte(SlotIndex) && Combat) Combat->OnQteAccepted(SlotIndex, Target);
}

void AOGWorldPrototypeCharacter::SetPartySlotQteReadyFromContent(
    int32 SlotIndex,
    bool bReady)
{
    if (PartyRuntime)
    {
        PartyRuntime->SetQteReady(SlotIndex, bReady);
    }
}

bool AOGWorldPrototypeCharacter::HasSkillSlotForHud(int32 SkillSlot) const
{
    if (!PartyRuntime || SkillSlot < 0)
    {
        return false;
    }

    const int32 ControlledSlot = PartyRuntime->GetControlledSlot();
    const TArray<FOGWorldPartySlot>& Slots = PartyRuntime->GetSlots();
    return Slots.IsValidIndex(ControlledSlot) &&
        Slots[ControlledSlot].bAvailable &&
        !Slots[ControlledSlot].bDefeated &&
        Slots[ControlledSlot].Unit.SkillSet.ActiveSkills.IsValidIndex(
            SkillSlot);
}

bool AOGWorldPrototypeCharacter::HasUltimateForHud() const
{
    if (!PartyRuntime)
    {
        return false;
    }

    const int32 ControlledSlot = PartyRuntime->GetControlledSlot();
    const TArray<FOGWorldPartySlot>& Slots = PartyRuntime->GetSlots();
    return Slots.IsValidIndex(ControlledSlot) &&
        Slots[ControlledSlot].bAvailable &&
        !Slots[ControlledSlot].bDefeated &&
        Slots[ControlledSlot].Unit.SkillSet.UltimateSkill.IsValid();
}

void AOGWorldPrototypeCharacter::TriggerSkillFromHud(int32 SkillSlot)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (HasSkillSlotForHud(SkillSlot))
    {
        RequestSkillSlot(SkillSlot);
    }
}

void AOGWorldPrototypeCharacter::TriggerUltimateFromHud()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (HasUltimateForHud())
    {
        UltimatePressed();
    }
}

void AOGWorldPrototypeCharacter::ToggleManualAimFromHud()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (bManualAim)
    {
        ManualAimReleased();
    }
    else
    {
        ManualAimPressed();
    }
}

void AOGWorldPrototypeCharacter::TriggerTraversalDownPulse()
{
    TraversalDownPressed();

    TWeakObjectPtr<AOGWorldPrototypeCharacter> WeakThis(this);
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(
        TimerHandle,
        [WeakThis]()
        {
            if (AOGWorldPrototypeCharacter* Character = WeakThis.Get())
            {
                Character->TraversalDownReleased();
            }
        },
        0.30f,
        false);
}

bool AOGWorldPrototypeCharacter::ShouldShowTraversalDownForHud() const
{
    if (!TraversalCapabilities)
    {
        return false;
    }

    const EOGTraversalMode Mode =
        TraversalCapabilities->GetTraversalMode();
    return Mode == EOGTraversalMode::Swimming ||
        Mode == EOGTraversalMode::Diving ||
        Mode == EOGTraversalMode::Flying ||
        Mode == EOGTraversalMode::Climbing;
}

FString AOGWorldPrototypeCharacter::GetTraversalModeLabelForHud() const
{
    if (!TraversalCapabilities)
    {
        return TEXT("Ground");
    }

    switch (TraversalCapabilities->GetTraversalMode())
    {
    case EOGTraversalMode::Climbing: return TEXT("Climbing");
    case EOGTraversalMode::Swimming: return TEXT("Swimming");
    case EOGTraversalMode::Diving: return TEXT("Diving");
    case EOGTraversalMode::Flying: return TEXT("Flying");
    case EOGTraversalMode::Mounted: return TEXT("Mounted");
    case EOGTraversalMode::Vehicle: return TEXT("Vehicle");
    default: return TEXT("Ground");
    }
}

bool AOGWorldPrototypeCharacter::HasRecentFoundationDamage() const
{
    return GetWorld() &&
        RecentFoundationDamage > 0.0f &&
        GetWorld()->GetTimeSeconds() <= RecentDamageExpiresAtSeconds;
}

void AOGWorldPrototypeCharacter::SetControlledResourcePresentation(
    FName ResourceLabel,
    float CurrentValue,
    float MaxValue,
    bool bVisible)
{
    ControlledResourceLabel = ResourceLabel;
    ControlledResourceCurrent = FMath::Max(0.0f, CurrentValue);
    ControlledResourceMax = FMath::Max(0.0f, MaxValue);
    bControlledResourceVisible =
        bVisible &&
        !ResourceLabel.IsNone() &&
        ControlledResourceMax > KINDA_SMALL_NUMBER;
}

void AOGWorldPrototypeCharacter::SetTargetHudProjection(
    const FOGWorldTargetViewModel& Projection)
{
    TargetHudProjection = Projection;
    bHasTargetHudProjection =
        Projection.TargetEntityId.IsValid();
}

void AOGWorldPrototypeCharacter::SetSecondaryTargetHudProjections(
    const TArray<FOGWorldTargetViewModel>& Projections)
{
    SecondaryTargetHudProjections = Projections;
    if (SecondaryTargetHudProjections.Num() > 6)
    {
        SecondaryTargetHudProjections.SetNum(6);
    }
}

void AOGWorldPrototypeCharacter::ClearTargetHudProjection()
{
    bHasTargetHudProjection = false;
    TargetHudProjection = FOGWorldTargetViewModel();
    SecondaryTargetHudProjections.Reset();
}

void AOGWorldPrototypeCharacter::TriggerContextInteract()
{
    InteractPressed();
}

void AOGWorldPrototypeCharacter::InteractPressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (CurrentCarrier.IsValid())
    {
        DismountCarrier();
        return;
    }

    if (AActor* Interactable =
            FindBestInteractable())
    {
        IOGWorldInteractable::Execute_Interact(
            Interactable,
            this);
        if (DiagnosticAnimations && (!InteractionHost || !InteractionHost->IsActive()))
            DiagnosticAnimations->PresentTimedAction(TEXT("Interact"), .45f);
        return;
    }

    if (TryBeginClimb())
    {
        return;
    }

    if (!GetWorld())
    {
        return;
    }

    AActor* BestCarrier = nullptr;
    float BestDistanceSq = FMath::Square(500.0f);
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate ||
            !Candidate->GetClass()->ImplementsInterface(
                UOGTraversalCarrier::StaticClass()))
        {
            continue;
        }

        const float DistanceSq =
            FVector::DistSquared(
                GetActorLocation(),
                Candidate->GetActorLocation());
        if (DistanceSq < BestDistanceSq &&
            IOGTraversalCarrier::Execute_CanBoard(
                Candidate,
                this) && IsInteractionLineClear(Candidate))
        {
            BestDistanceSq = DistanceSq;
            BestCarrier = Candidate;
        }
    }

    if (BestCarrier)
    {
        TryBoardCarrier(BestCarrier);
    }
}

void AOGWorldPrototypeCharacter::ToggleLockOn()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (HardLockedTarget.IsValid())
    {
        HardLockedTarget.Reset();
        OnHardLockChanged(nullptr);
        return;
    }

    if (AActor* Target = FindBestTarget())
    {
        HardLockedTarget = Target;
        OnHardLockChanged(Target);
    }
}

void AOGWorldPrototypeCharacter::PrimaryAttackPressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!GetWorld())
    {
        return;
    }

    if (UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>())
    {
        if (Combat->IsActive())
        {
            AActor* DiagnosticTarget = HardLockedTarget.IsValid() ? HardLockedTarget.Get() : FindBestTarget();
            FString Error;
            if (Combat->TryWorldAction(EOGDiagnosticCommand::Basic, DiagnosticTarget, Error))
            { OnPrimaryAttackRequested(DiagnosticTarget); }
            else { Combat->SetStatus(Error); }
            return;
        }
    }

    FOGWorldActionState AttackState;
    AttackState.ActionId = FName(TEXT("foundation.primary_attack"));
    FOGActionCancelWindow CancelWindow;
    CancelWindow.OpensAtSeconds = 0.08f;
    CancelWindow.ClosesAtSeconds = 0.28f;
    CancelWindow.Destinations =
    {
        EOGActionCancelDestination::Dodge,
        EOGActionCancelDestination::Jump,
        EOGActionCancelDestination::Switch,
        EOGActionCancelDestination::Skill,
        EOGActionCancelDestination::Ultimate
    };
    AttackState.CancelWindows.Add(CancelWindow);
    AttackState.bAllowWhileAirborne = true;
    AttackState.bAllowWhileSwimming = true;
    AttackState.bAllowWhileDiving = true;
    AttackState.bAllowWhileFlying = true;
    AttackState.bAllowWhileClimbing = false;
    AttackState.bAllowWhileMounted = true;
    if (ActionRuntime)
    {
        ActionRuntime->BeginAuthoredAction(AttackState);
    }

    AttackVisualEndsAtSeconds =
        GetWorld()->GetTimeSeconds() + 0.24;

    if (bHapticsEnabled)
    {
        if (APlayerController* PlayerController =
                Cast<APlayerController>(Controller))
        {
            PlayerController->PlayDynamicForceFeedback(
                HapticsIntensity * 0.22f,
                0.035f,
                true,
                true,
                true,
                true);
        }
    }

    AActor* Target =
        HardLockedTarget.IsValid()
            ? HardLockedTarget.Get()
            : FindBestTarget();

    if (Target)
    {
        const FVector TargetPoint =
            Target->GetClass()->ImplementsInterface(
                UOGWorldTargetable::StaticClass())
                ? IOGWorldTargetable::Execute_GetTargetPoint(
                    Target,
                    this)
                : Target->GetActorLocation();

        const float Distance =
            FVector::Dist(GetActorLocation(), TargetPoint);
        if (Distance <= 360.0f)
        {
            const float Applied =
                UGameplayStatics::ApplyDamage(
                    Target,
                    12.0f,
                    GetController(),
                    this,
                    nullptr);
            if (Applied > 0.0f)
            {
                RecentFoundationDamage = Applied;
                RecentDamageExpiresAtSeconds =
                    GetWorld()->GetTimeSeconds() + 0.85;
            }
        }
    }

    OnPrimaryAttackRequested(Target);
}


bool AOGWorldPrototypeCharacter::PrepareActionCancel(
    EOGActionCancelDestination Destination)
{
    if (DiagnosticCombat && DiagnosticCombat->IsWorldDefeated()) return false;
    if (!ActionRuntime ||
        !ActionRuntime->HasActiveAction())
    {
        return true;
    }

    if (!ActionRuntime->CanCancelTo(Destination))
    {
        return false;
    }

    ActionRuntime->EndAuthoredAction();
    return true;
}

bool AOGWorldPrototypeCharacter::CanContinueActionInTraversalMode(
    EOGTraversalMode Mode) const
{
    return !ActionRuntime ||
        !ActionRuntime->HasActiveAction() ||
        ActionRuntime->CanContinueInTraversalMode(
            static_cast<uint8>(Mode));
}

void AOGWorldPrototypeCharacter::RequestSkillSlot(
    int32 SkillSlot)
{
    if (!PrepareActionCancel(
            EOGActionCancelDestination::Skill))
    {
        return;
    }

    AActor* Target =
        HardLockedTarget.IsValid()
            ? HardLockedTarget.Get()
            : FindBestTarget();

    if (UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>())
    {
        FString Error;
        if (!Combat->IsActive()) { OnSkillRequested(SkillSlot, Target); return; }
        if (Combat->TryWorldAction(SkillSlot == 0 ? EOGDiagnosticCommand::Skill1 : EOGDiagnosticCommand::Skill2, Target, Error))
        { OnSkillRequested(SkillSlot, Target); }
        else { Combat->SetStatus(Error); }
        return;
    }
    OnSkillRequested(SkillSlot, Target);
}

void AOGWorldPrototypeCharacter::Skill1Pressed()
{
    RequestSkillSlot(0);
}

void AOGWorldPrototypeCharacter::Skill2Pressed()
{
    RequestSkillSlot(1);
}

void AOGWorldPrototypeCharacter::UltimatePressed()
{
    if (!PrepareActionCancel(
            EOGActionCancelDestination::Ultimate))
    {
        return;
    }

    AActor* Target =
        HardLockedTarget.IsValid()
            ? HardLockedTarget.Get()
            : FindBestTarget();

    if (UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>())
    {
        FString Error;
        if (!Combat->IsActive()) { OnUltimateRequested(Target); return; }
        if (Combat->TryWorldAction(EOGDiagnosticCommand::Ultimate, Target, Error))
        { OnUltimateRequested(Target); }
        else { Combat->SetStatus(Error); }
        return;
    }
    OnUltimateRequested(Target);
}

void AOGWorldPrototypeCharacter::ManualAimPressed()
{
    if (bManualAim)
    {
        return;
    }

    bManualAim = true;
    OnManualAimChanged(true);
}

void AOGWorldPrototypeCharacter::ManualAimReleased()
{
    if (!bManualAim)
    {
        return;
    }

    bManualAim = false;
    OnManualAimChanged(false);
}

void AOGWorldPrototypeCharacter::RequestPartySwitch(
    int32 SlotIndex)
{
    if (!PartyRuntime ||
        !PrepareActionCancel(
            EOGActionCancelDestination::Switch))
    {
        return;
    }

    UOGDiagnosticCombatComponent* Combat = FindComponentByClass<UOGDiagnosticCombatComponent>();
    if (Combat && Combat->IsActive() && Combat->IsSuspended()) return;
    if (PartyRuntime->TrySwitchTo(SlotIndex, true) && Combat) Combat->OnPartyChanged();
}

void AOGWorldPrototypeCharacter::SwitchSlot1Pressed()
{
    RequestPartySwitch(0);
}

void AOGWorldPrototypeCharacter::SwitchSlot2Pressed()
{
    RequestPartySwitch(1);
}

void AOGWorldPrototypeCharacter::SwitchSlot3Pressed()
{
    RequestPartySwitch(2);
}

void AOGWorldPrototypeCharacter::QtePressed()
{
    if (!PartyRuntime)
    {
        return;
    }

    const TArray<FOGWorldPartySlot>& Slots =
        PartyRuntime->GetSlots();
    for (int32 SlotIndex = 0;
         SlotIndex < Slots.Num();
         ++SlotIndex)
    {
        if (IsPartySlotQteReadyForHud(SlotIndex))
        {
            TriggerQteFromHud(SlotIndex);
            return;
        }
    }
}

void AOGWorldPrototypeCharacter::PausePressed()
{
    SetFoundationPauseMenuOpen(!bFoundationPauseMenuOpen);
}

void AOGWorldPrototypeCharacter::UpdateHardLock(
    float DeltaSeconds)
{
    AActor* Target = HardLockedTarget.Get();
    if (!Target || !Controller)
    {
        return;
    }

    if (!Target->GetClass()->ImplementsInterface(
            UOGWorldTargetable::StaticClass()) ||
        !IOGWorldTargetable::Execute_CanBeTargeted(
            Target,
            this))
    {
        HardLockedTarget.Reset();
        OnHardLockChanged(nullptr);
        return;
    }

    const FVector TargetPoint =
        IOGWorldTargetable::Execute_GetTargetPoint(
            Target,
            this);
    const FVector ToTarget =
        TargetPoint - GetActorLocation();

    if (ToTarget.SizeSquared() >
        FMath::Square(5000.0f))
    {
        HardLockedTarget.Reset();
        OnHardLockChanged(nullptr);
        return;
    }

    const FRotator Desired =
        ToTarget.Rotation();
    const FRotator Current =
        Controller->GetControlRotation();
    const FRotator Smoothed =
        FMath::RInterpTo(
            Current,
            Desired,
            DeltaSeconds,
            9.0f);

    Controller->SetControlRotation(
        FRotator(
            FMath::Clamp(
                Smoothed.Pitch,
                WorldCameraPitchMin,
                WorldCameraPitchMax),
            Smoothed.Yaw,
            0.0f));
}

void AOGWorldPrototypeCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    RefreshFoundationInputState();
    if (!IntegrationWorld) InitializeFoundationIntegration();
    UpdateDiagnosticAnimationState();

    if (TraversalCapabilities && GetCharacterMovement())
    {
        const bool bCanSwim = TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Swim);
        GetCharacterMovement()->NavAgentProps.bCanSwim = bCanSwim;
        if (!bCanSwim && GetCharacterMovement()->MovementMode == MOVE_Swimming)
        {
            GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            TraversalCapabilities->SetTraversalMode(EOGTraversalMode::Ground);
        }
        if (TraversalCapabilities->GetTraversalMode() == EOGTraversalMode::Diving &&
            !TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Dive))
        {
            SetDiving(false);
        }
        if (TraversalCapabilities->GetTraversalMode() == EOGTraversalMode::Flying &&
            !TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Flight))
        {
            EndFlight();
        }
    }
    UpdateClimbSurface();

    if (TraversalCapabilities)
    {
        const EOGTraversalMode Mode =
            TraversalCapabilities->GetTraversalMode();
        const float VerticalInput =
            (bTraversalAscendHeld ? 1.0f : 0.0f) -
            (bTraversalDescendHeld ? 1.0f : 0.0f);

        if (CurrentCarrier.IsValid() &&
            !FMath::IsNearlyZero(VerticalInput) &&
            CurrentCarrier->GetClass()->ImplementsInterface(
                UOGTraversalCarrier::StaticClass()))
        {
            IOGTraversalCarrier::Execute_AddCarrierVerticalInput(
                CurrentCarrier.Get(),
                this,
                VerticalInput);
        }
        else if ((Mode == EOGTraversalMode::Flying ||
                  Mode == EOGTraversalMode::Swimming ||
                  Mode == EOGTraversalMode::Diving) &&
                 !FMath::IsNearlyZero(VerticalInput))
        {
            AddMovementInput(
                FVector::UpVector,
                VerticalInput);
        }
    }

    const FVector Location = GetActorLocation();
    const FVector2D MapPoint(Location.X, Location.Y);
    if (!bHasMapTrailSample ||
        FVector2D::Distance(
            MapPoint,
            LastMapTrailSample) >= 280.0f)
    {
        VisitedMapTrail.Add(MapPoint);
        LastMapTrailSample = MapPoint;
        bHasMapTrailSample = true;
        if (VisitedMapTrail.Num() > 96)
        {
            VisitedMapTrail.RemoveAt(
                0,
                VisitedMapTrail.Num() - 96,
                EAllowShrinking::No);
        }
    }

    RefreshFoundationTouchInterfaceIfNeeded();
    EndDodgeIfNeeded();
    UpdateFoundationCombatPresentation();
    UpdateHardLock(DeltaSeconds);
    ValidateWorldSafety();
}

void AOGWorldPrototypeCharacter::OnMovementModeChanged(
    EMovementMode PrevMovementMode,
    uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(
        PrevMovementMode,
        PreviousCustomMode);

    if (!TraversalCapabilities)
    {
        return;
    }

    EMovementMode NewMode = MOVE_None;
    if (const UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        NewMode = Movement->MovementMode;
    }

    if (NewMode == MOVE_Swimming)
    {
        if (CurrentCarrier.IsValid())
        {
            GetCharacterMovement()->DisableMovement();
            return;
        }
        if (!TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Swim))
        {
            GetCharacterMovement()->NavAgentProps.bCanSwim = false;
            GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            TraversalCapabilities->SetTraversalMode(EOGTraversalMode::Ground);
            return;
        }
        ClimbSurfaceNormal = FVector::ZeroVector;
        CurrentClimbSurfaceActor.Reset();
        CurrentClimbSurfaceComponent.Reset();
        GetCharacterMovement()->bOrientRotationToMovement = true;
        if (!CanContinueActionInTraversalMode(
                EOGTraversalMode::Swimming) &&
            ActionRuntime)
        {
            ActionRuntime->EndAuthoredAction();
        }

        if (TraversalCapabilities->GetTraversalMode() !=
            EOGTraversalMode::Diving)
        {
            TraversalCapabilities->SetTraversalMode(
                EOGTraversalMode::Swimming);
        }
        return;
    }

    const EOGTraversalMode Current =
        TraversalCapabilities->GetTraversalMode();

    if ((Current == EOGTraversalMode::Swimming ||
         Current == EOGTraversalMode::Diving) &&
        NewMode != MOVE_Swimming)
    {
        TraversalCapabilities->SetTraversalMode(
            EOGTraversalMode::Ground);
    }
}

bool AOGWorldPrototypeCharacter::CanTraverseGate(
    AActor* Gate) const
{
    if (!Gate ||
        !TraversalCapabilities ||
        !Gate->GetClass()->ImplementsInterface(
            UOGTraversalGate::StaticClass()))
    {
        return false;
    }

    const FOGTraversalRequirement Requirement =
        IOGTraversalGate::Execute_GetTraversalRequirement(
            Gate,
            const_cast<AOGWorldPrototypeCharacter*>(this));

    return TraversalCapabilities->SatisfiesRequirement(
        Requirement);
}

bool AOGWorldPrototypeCharacter::TryBeginClimb()
{
    if (!TraversalCapabilities ||
        !TraversalCapabilities->HasCapability(
            FOGTraversalCapabilityIds::Climb) ||
        CurrentCarrier.IsValid() ||
        !CanContinueActionInTraversalMode(
            EOGTraversalMode::Climbing))
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    GetActorEyesViewPoint(
        ViewLocation,
        ViewRotation);

    FHitResult Hit;
    FCollisionQueryParams Query(
        FName(TEXT("OGClimbProbe")),
        false,
        this);

    if (!GetWorld()) { return false; }
    const FVector Forward = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Vector();
    bool bHitSurface = false;
    for (float Offset : {GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.65f,
        0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f})
    {
        const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, Offset);
        if (GetWorld()->LineTraceSingleByChannel(Hit, From, From + Forward * 150.0f,
            ECC_Visibility, Query) && FMath::Abs(Hit.ImpactNormal.Z) <= 0.65f)
        {
            bHitSurface = true;
            break;
        }
    }
    if (!bHitSurface) { return false; }

    UObject* Climbable = nullptr;
    if (Hit.GetComponent() &&
        Hit.GetComponent()->GetClass()->ImplementsInterface(
            UOGClimbableSurface::StaticClass()))
    {
        Climbable = Hit.GetComponent();
    }
    else if (Hit.GetActor() &&
             Hit.GetActor()->GetClass()->ImplementsInterface(
                 UOGClimbableSurface::StaticClass()))
    {
        Climbable = Hit.GetActor();
    }

    if (!Climbable ||
        !IOGClimbableSurface::Execute_CanClimb(
            Climbable,
            this,
            Hit.ImpactPoint,
            Hit.ImpactNormal))
    {
        return false;
    }

    if (FMath::Abs(Hit.ImpactNormal.Z) > 0.65f)
    {
        return false;
    }
    ClimbSurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
    bClimbTopBlocked = false;
    CurrentClimbSurfaceActor = Hit.GetActor();
    CurrentClimbSurfaceComponent = Hit.GetComponent();
    FVector Anchor = Hit.ImpactPoint + ClimbSurfaceNormal *
        (GetCapsuleComponent()->GetScaledCapsuleRadius() + 4.0f);
    Anchor.Z = GetActorLocation().Z;
    SetActorLocation(Anchor, true);

    UCharacterMovementComponent* Movement =
        GetCharacterMovement();
    Movement->StopMovementImmediately();
    Movement->bOrientRotationToMovement = false;
    Movement->MaxFlySpeed = 360.0f;
    Movement->SetMovementMode(MOVE_Flying);

    TraversalCapabilities->SetTraversalMode(
        EOGTraversalMode::Climbing);

    const FRotator FaceWall =
        (-ClimbSurfaceNormal).Rotation();
    SetActorRotation(
        FRotator(
            0.0f,
            FaceWall.Yaw,
            0.0f));

    return true;
}

void AOGWorldPrototypeCharacter::EndClimb()
{
    if (!TraversalCapabilities ||
        TraversalCapabilities->GetTraversalMode() !=
            EOGTraversalMode::Climbing)
    {
        return;
    }

    bClimbTopBlocked = false;
    ClimbSurfaceNormal = FVector::ZeroVector;
    CurrentClimbSurfaceActor.Reset();
    CurrentClimbSurfaceComponent.Reset();

    if (UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        Movement->MaxFlySpeed = 900.0f;
        Movement->bOrientRotationToMovement = true;
        Movement->SetMovementMode(MOVE_Falling);
    }

    TraversalCapabilities->SetTraversalMode(
        EOGTraversalMode::Ground);
}

bool AOGWorldPrototypeCharacter::BeginFlight()
{
    if (!TraversalCapabilities ||
        !TraversalCapabilities->HasCapability(
            FOGTraversalCapabilityIds::Flight) ||
        CurrentCarrier.IsValid() ||
        !CanContinueActionInTraversalMode(
            EOGTraversalMode::Flying))
    {
        return false;
    }

    EndClimb();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxFlySpeed = 900.0f;
        Movement->SetMovementMode(MOVE_Flying);
    }
    TraversalCapabilities->SetTraversalMode(EOGTraversalMode::Flying);
    return true;
}

void AOGWorldPrototypeCharacter::EndFlight()
{
    if (!TraversalCapabilities ||
        TraversalCapabilities->GetTraversalMode() !=
            EOGTraversalMode::Flying)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Falling);
    }

    TraversalCapabilities->SetTraversalMode(
        EOGTraversalMode::Ground);
}

bool AOGWorldPrototypeCharacter::SetDiving(bool bDiving)
{
    if (!TraversalCapabilities ||
        !GetCharacterMovement() ||
        GetCharacterMovement()->MovementMode !=
            MOVE_Swimming)
    {
        return false;
    }

    if (bDiving &&
        (!TraversalCapabilities->HasCapability(
             FOGTraversalCapabilityIds::Dive) ||
         !CanContinueActionInTraversalMode(
             EOGTraversalMode::Diving)))
    {
        return false;
    }

    TraversalCapabilities->SetTraversalMode(
        bDiving
            ? EOGTraversalMode::Diving
            : EOGTraversalMode::Swimming);
    return true;
}

bool AOGWorldPrototypeCharacter::TryBoardCarrier(
    AActor* Carrier)
{
    if (!Carrier ||
        !TraversalCapabilities ||
        CurrentCarrier.IsValid() ||
        !Carrier->GetClass()->ImplementsInterface(
            UOGTraversalCarrier::StaticClass()) ||
        !IOGTraversalCarrier::Execute_CanBoard(Carrier, this) ||
        FVector::DistSquared(GetActorLocation(), Carrier->GetActorLocation()) > FMath::Square(500.0f) ||
        !IsInteractionLineClear(Carrier))
    {
        return false;
    }

    const EOGTraversalMode CarrierMode =
        IOGTraversalCarrier::Execute_GetCarrierTraversalMode(
            Carrier);

    if (CarrierMode != EOGTraversalMode::Mounted &&
        CarrierMode != EOGTraversalMode::Vehicle)
    {
        return false;
    }

    const FName RequiredCapability =
        UOGTraversalCapabilityComponent::RequiredCapabilityForMode(
            CarrierMode);

    if (!TraversalCapabilities->HasCapability(
            RequiredCapability) ||
        !CanContinueActionInTraversalMode(
            CarrierMode))
    {
        return false;
    }

    const FTransform RiderTransform = IOGTraversalCarrier::Execute_GetRiderTransform(Carrier, this);
    if (!HasClearCapsuleAt(RiderTransform.GetLocation(), Carrier)) { return false; }
    EndClimb();
    EndFlight();
    bTraversalAscendHeld = false;
    bTraversalDescendHeld = false;
    CurrentCarrier = Carrier;
    GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier, true);

    if (UCharacterMovementComponent* Movement =
            GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }

    AttachToActor(
        Carrier,
        FAttachmentTransformRules::KeepWorldTransform);
    SetActorTransform(
        RiderTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    TraversalCapabilities->SetTraversalMode(
        CarrierMode);

    IOGTraversalCarrier::Execute_OnBoarded(
        Carrier,
        this);

    return true;
}

void AOGWorldPrototypeCharacter::DismountCarrier()
{
    AActor* Carrier = CurrentCarrier.Get();
    FVector Destination;
    if (!Carrier) return;
    if (!FindDismountLocation(Carrier, Destination))
    {
        if (DiagnosticCombat)
            DiagnosticCombat->SetStatus(TEXT("Exit blocked. Move the mount/vehicle to clear ground and try again."));
        return; // Keep the rider attached when every physical exit is blocked.
    }
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    CurrentCarrier.Reset();
    SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
    GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier, false);
    if (Carrier->GetClass()->ImplementsInterface(UOGTraversalCarrier::StaticClass()))
    {
        IOGTraversalCarrier::Execute_OnUnboarded(Carrier, this);
    }
    bTraversalAscendHeld = false;
    bTraversalDescendHeld = false;
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_Falling);
    }
    if (TraversalCapabilities)
    {
        TraversalCapabilities->SetTraversalMode(EOGTraversalMode::Ground);
    }
}

bool AOGWorldPrototypeCharacter::TryTeleportWithinCurrentMap(FVector Destination)
{
    if (!TraversalCapabilities ||
        !TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Teleport) ||
        CurrentCarrier.IsValid() ||
        !IsWithinDeclaredPlayableBoundary(Destination) || !HasClearCapsuleAt(Destination))
    {
        return false;
    }
    ResetTraversalForRelocation();
    return SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
}

void AOGWorldPrototypeCharacter::JumpPressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!PrepareActionCancel(
            EOGActionCancelDestination::Jump))
    {
        return;
    }

    if (!TraversalCapabilities)
    {
        Jump();
        return;
    }

    switch (TraversalCapabilities->GetTraversalMode())
    {
        case EOGTraversalMode::Climbing:
        {
            const FVector Away =
                ClimbSurfaceNormal.IsNearlyZero()
                    ? -GetActorForwardVector()
                    : ClimbSurfaceNormal;

            EndClimb();
            LaunchCharacter(
                Away * 360.0f +
                    FVector::UpVector * 430.0f,
                true,
                true);
            return;
        }

        case EOGTraversalMode::Swimming:
        case EOGTraversalMode::Diving:
        case EOGTraversalMode::Flying:
            bTraversalAscendHeld = true;
            return;

        default:
            Jump();
            return;
    }
}

void AOGWorldPrototypeCharacter::JumpReleased()
{
    bTraversalAscendHeld = false;
    StopJumping();
}

void AOGWorldPrototypeCharacter::TraversalDownPressed()
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (TraversalCapabilities &&
        TraversalCapabilities->GetTraversalMode() == EOGTraversalMode::Climbing)
    {
        EndClimb();
        return;
    }
    bTraversalDescendHeld = true;

    if (TraversalCapabilities &&
        TraversalCapabilities->GetTraversalMode() ==
            EOGTraversalMode::Swimming)
    {
        SetDiving(true);
    }
}

void AOGWorldPrototypeCharacter::TraversalDownReleased()
{
    bTraversalDescendHeld = false;

    if (TraversalCapabilities &&
        TraversalCapabilities->GetTraversalMode() ==
            EOGTraversalMode::Diving)
    {
        SetDiving(false);
    }
}

void AOGWorldPrototypeCharacter::CacheStartingRegion()
{
    if (StartingRegion.IsValid())
    {
        return;
    }

    TActorIterator<AOGStartingRegionGenerator> It(GetWorld());
    if (It)
    {
        StartingRegion = *It;
    }
}

bool AOGWorldPrototypeCharacter::IsWithinDeclaredPlayableBoundary(
    const FVector& WorldLocation,
    bool* bOutStableGroundRegion) const
{
    if (bOutStableGroundRegion)
    {
        *bOutStableGroundRegion = false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return true;
    }

    bool bFoundBoundaryProvider = false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate ||
            !Candidate->GetClass()->ImplementsInterface(
                UOGPlayableBoundaryProvider::StaticClass()))
        {
            continue;
        }

        bFoundBoundaryProvider = true;
        if (!IOGPlayableBoundaryProvider::Execute_IsInsidePlayableBoundary(
                Candidate,
                WorldLocation))
        {
            continue;
        }

        if (bOutStableGroundRegion)
        {
            *bOutStableGroundRegion =
                IOGPlayableBoundaryProvider::Execute_IsInsideStableGroundRegion(
                    Candidate,
                    WorldLocation);
        }
        return true;
    }

    // No authored boundary means Foundation imposes no universal fall/depth rule.
    return !bFoundBoundaryProvider;
}


bool AOGWorldPrototypeCharacter::HasClearCapsuleAt(const FVector& Location,
    const AActor* IgnoredActor) const
{
    if (!GetWorld() || !GetCapsuleComponent()) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OGTraversalLanding), false, this);
    if (IgnoredActor) { Query.AddIgnoredActor(IgnoredActor); }
    return !GetWorld()->OverlapBlockingTestByChannel(Location, GetActorQuat(),
        GetCapsuleComponent()->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(),
            GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query);
}

bool AOGWorldPrototypeCharacter::IsInteractionLineClear(AActor* Candidate) const
{
    if (!GetWorld() || !Candidate) { return false; }
    FVector Eyes;
    FRotator View;
    GetActorEyesViewPoint(Eyes, View);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OGContextVisibility), false, this);
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes,
        Candidate->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Candidate;
}

void AOGWorldPrototypeCharacter::ResetTraversalForRelocation()
{
    if (AActor* Carrier = CurrentCarrier.Get())
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier, false);
        CurrentCarrier.Reset();
        if (Carrier->GetClass()->ImplementsInterface(UOGTraversalCarrier::StaticClass()))
        {
            IOGTraversalCarrier::Execute_OnUnboarded(Carrier, this);
        }
    }
    bClimbTopBlocked = false;
    ClimbSurfaceNormal = FVector::ZeroVector;
    CurrentClimbSurfaceActor.Reset();
    CurrentClimbSurfaceComponent.Reset();
    bTraversalAscendHeld = false;
    bTraversalDescendHeld = false;
    ConsumeMovementInputVector();
    StopJumping();
    if (ActionRuntime) { ActionRuntime->EndAuthoredAction(); }
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->bOrientRotationToMovement = true;
        Movement->MaxFlySpeed = 900.0f;
        Movement->SetMovementMode(MOVE_Falling);
    }
    if (TraversalCapabilities) { TraversalCapabilities->SetTraversalMode(EOGTraversalMode::Ground); }
}

bool AOGWorldPrototypeCharacter::FindDismountLocation(AActor* Carrier,
    FVector& OutLocation) const
{
    if (!Carrier || !GetWorld()) { return false; }
    const FBox Bounds = Carrier->GetComponentsBoundingBox();
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float ExitDistance = FMath::Max(Bounds.GetExtent().X, Bounds.GetExtent().Y) + Radius + 60.0f;
    const FVector Origin = GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OGCarrierExit), false, this);
    Query.AddIgnoredActor(Carrier);
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    for (int32 Index = 0; Index != 8; ++Index)
    {
        const FVector Direction = FRotator(0.0f, Carrier->GetActorRotation().Yaw + Index * 45.0f, 0.0f).Vector();
        FVector Candidate = Origin + Direction * ExitDistance;
        FHitResult Ground;
        if (GetWorld()->LineTraceSingleByChannel(Ground, Candidate + FVector(0.0f, 0.0f, 300.0f),
            Candidate - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Query) &&
            Ground.ImpactNormal.Z >= GetCharacterMovement()->GetWalkableFloorZ())
        {
            Candidate.Z = Ground.ImpactPoint.Z + HalfHeight + 4.0f;
        }
        FHitResult Exit;
        if (IsWithinDeclaredPlayableBoundary(Candidate) && HasClearCapsuleAt(Candidate, Carrier) &&
            !GetWorld()->SweepSingleByChannel(Exit, Origin, Candidate, GetActorQuat(),
                GetCapsuleComponent()->GetCollisionObjectType(), Capsule, Query))
        {
            OutLocation = Candidate;
            return true;
        }
    }
    return false;
}

void AOGWorldPrototypeCharacter::UpdateClimbSurface()
{
    if (!TraversalCapabilities ||
        TraversalCapabilities->GetTraversalMode() != EOGTraversalMode::Climbing) { return; }
    UWorld* World = GetWorld();
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!World || !Movement || !CurrentClimbSurfaceActor.IsValid() ||
        !CurrentClimbSurfaceComponent.IsValid() ||
        !TraversalCapabilities->HasCapability(FOGTraversalCapabilityIds::Climb))
    {
        EndClimb();
        return;
    }
    const FVector Center = GetActorLocation();
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector TowardWall = -ClimbSurfaceNormal;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(OGClimbContinuation), false, this);
    auto Probe = [World, &Query, this, TowardWall, Radius](const FVector& From, FHitResult& Hit)
    {
        if (!World->LineTraceSingleByChannel(Hit, From, From + TowardWall * (Radius + 180.0f),
            ECC_Visibility, Query)) { return false; }
        UObject* Surface = Hit.GetComponent() &&
            Hit.GetComponent()->GetClass()->ImplementsInterface(UOGClimbableSurface::StaticClass())
            ? static_cast<UObject*>(Hit.GetComponent()) : static_cast<UObject*>(Hit.GetActor());
        return Surface && Surface->GetClass()->ImplementsInterface(UOGClimbableSurface::StaticClass()) &&
            FMath::Abs(Hit.ImpactNormal.Z) <= 0.65f &&
            IOGClimbableSurface::Execute_CanClimb(Surface, this, Hit.ImpactPoint, Hit.ImpactNormal);
    };
    FHitResult Upper;
    FHitResult Lower;
    bClimbTopBlocked = false;
    const bool bUpper = Probe(Center + FVector(0.0f, 0.0f, HalfHeight * 0.55f), Upper);
    const bool bLower = Probe(Center - FVector(0.0f, 0.0f, HalfHeight * 0.40f), Lower);
    if (!bUpper && bLower && (Movement->Velocity.Z > 0.0f || GetLastMovementInputVector().Z > 0.0f))
    {
        const FVector Ahead = Center + TowardWall * (Radius + 40.0f);
        FHitResult Floor;
        if (World->LineTraceSingleByChannel(Floor, Ahead + FVector(0.0f, 0.0f, HalfHeight + 180.0f),
            Ahead - FVector(0.0f, 0.0f, HalfHeight), ECC_Visibility, Query) &&
            Floor.ImpactNormal.Z >= Movement->GetWalkableFloorZ())
        {
            const FVector Landing = Floor.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 4.0f);
            const FVector Lift(Center.X, Center.Y, Landing.Z);
            const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
            FHitResult Block;
            if (HasClearCapsuleAt(Lift) && HasClearCapsuleAt(Landing) &&
                !World->SweepSingleByChannel(Block, Center, Lift, GetActorQuat(), GetCapsuleComponent()->GetCollisionObjectType(), Capsule, Query) &&
                !World->SweepSingleByChannel(Block, Lift, Landing, GetActorQuat(), GetCapsuleComponent()->GetCollisionObjectType(), Capsule, Query))
            {
                EndClimb();
                Movement->StopMovementImmediately();
                SetActorLocation(Landing, false, nullptr, ETeleportType::TeleportPhysics);
                Movement->SetMovementMode(MOVE_Walking);
                return;
            }
        }
        // Suppress positive climb input until this blocked top becomes clear.
        bClimbTopBlocked = true;
        if (Movement->Velocity.Z > 0.0f) { Movement->Velocity.Z = 0.0f; }
    }
    if (!bUpper && !bLower) { EndClimb(); return; }
    const FHitResult& Support = bLower ? Lower : Upper;
    CurrentClimbSurfaceActor = Support.GetActor();
    CurrentClimbSurfaceComponent = Support.GetComponent();
    ClimbSurfaceNormal = Support.ImpactNormal.GetSafeNormal();
    const FRotator Facing = (-ClimbSurfaceNormal).Rotation();
    SetActorRotation(FRotator(0.0f, Facing.Yaw, 0.0f));
}

void AOGWorldPrototypeCharacter::ValidateWorldSafety()
{
    const FVector WorldLocation = GetActorLocation();
    bool bStableGroundRegion = false;
    if (!IsWithinDeclaredPlayableBoundary(
            WorldLocation,
            &bStableGroundRegion))
    {
        RecoverToLastSafeGround(TEXT("declared_map_boundary"));
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (Movement &&
        Movement->IsMovingOnGround() &&
        bStableGroundRegion)
    {
        LastSafeGroundedLocation = WorldLocation;
        bHasLastSafeGroundedLocation = true;
    }
}

void AOGWorldPrototypeCharacter::RecoverToLastSafeGround(const TCHAR* Reason)
{
    ResetTraversalForRelocation();
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!bHasLastSafeGroundedLocation)
    {
        CacheStartingRegion();
        if (AOGStartingRegionGenerator* Region = StartingRegion.Get())
        {
            SetActorLocation(
                Region->GetActorLocation() + FVector(0.0f, 0.0f, 2500.0f),
                false,
                nullptr,
                ETeleportType::TeleportPhysics);
            GroundSnapAttempts = 0;
            if (Movement)
            {
                Movement->StopMovementImmediately();
                Movement->SetMovementMode(MOVE_Flying);
            }
            GetWorldTimerManager().SetTimerForNextTick(
                this,
                &AOGWorldPrototypeCharacter::TrySnapToGeneratedGround);
        }
        return;
    }

    SetActorLocation(
        LastSafeGroundedLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);

    if (Movement)
    {
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_Walking);
    }

    UE_LOG(
        LogOfflineGame,
        Warning,
        TEXT("World Mode safety recovery applied. Reason=%s Location=(%.1f, %.1f, %.1f)"),
        Reason ? Reason : TEXT("unknown"),
        LastSafeGroundedLocation.X,
        LastSafeGroundedLocation.Y,
        LastSafeGroundedLocation.Z);
}

void AOGWorldPrototypeCharacter::TrySnapToGeneratedGround()
{
    UWorld* World = GetWorld();
    if (!World || IntegrationWorld)
    {
        return;
    }

    ++GroundSnapAttempts;

    const FVector Current = GetActorLocation();
    const FVector TraceStart(Current.X, Current.Y, 12000.0f);
    const FVector TraceEnd(Current.X, Current.Y, -12000.0f);

    FHitResult Hit;
    FCollisionQueryParams QueryParams(
        FName(TEXT("OGStartingRegionGroundSnap")),
        false,
        this);

    if (World->LineTraceSingleByChannel(
            Hit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams))
    {
        const float HalfHeight =
            GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector GroundedLocation =
            Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 8.0f);
        SetActorLocation(
            GroundedLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
        LastSafeGroundedLocation = GroundedLocation;
        bHasLastSafeGroundedLocation = true;

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->SetMovementMode(MOVE_Walking);
        }

        UE_LOG(
            LogOfflineGame,
            Log,
            TEXT("World Mode pawn grounded after generated terrain became available. Attempts=%d GroundZ=%.1f"),
            GroundSnapAttempts,
            Hit.Location.Z);
        return;
    }

    if (GroundSnapAttempts < 30)
    {
        FTimerHandle RetryHandle;
        GetWorldTimerManager().SetTimer(
            RetryHandle,
            this,
            &AOGWorldPrototypeCharacter::TrySnapToGeneratedGround,
            0.1f,
            false);
        return;
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Falling);
    }

    UE_LOG(
        LogOfflineGame,
        Warning,
        TEXT("World Mode pawn could not find generated terrain after %d attempts."),
        GroundSnapAttempts);
}

void AOGWorldPrototypeCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis(
        TEXT("MoveForward"),
        this,
        &AOGWorldPrototypeCharacter::MoveForward);
    PlayerInputComponent->BindAxis(
        TEXT("MoveRight"),
        this,
        &AOGWorldPrototypeCharacter::MoveRight);
    PlayerInputComponent->BindAxis(
        TEXT("Turn"),
        this,
        &AOGWorldPrototypeCharacter::Turn);
    PlayerInputComponent->BindAxis(
        TEXT("LookUp"),
        this,
        &AOGWorldPrototypeCharacter::LookUp);
    PlayerInputComponent->BindAxis(
        TEXT("TurnRate"),
        this,
        &AOGWorldPrototypeCharacter::TurnRate);
    PlayerInputComponent->BindAxis(
        TEXT("LookUpRate"),
        this,
        &AOGWorldPrototypeCharacter::LookUpRate);

    PlayerInputComponent->BindAction(
        TEXT("Jump"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::JumpPressed);
    PlayerInputComponent->BindAction(
        TEXT("Jump"),
        IE_Released,
        this,
        &AOGWorldPrototypeCharacter::JumpReleased);
    PlayerInputComponent->BindAction(
        TEXT("TraversalDown"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::TraversalDownPressed);
    PlayerInputComponent->BindAction(
        TEXT("TraversalDown"),
        IE_Released,
        this,
        &AOGWorldPrototypeCharacter::TraversalDownReleased);
    PlayerInputComponent->BindAction(
        TEXT("Sprint"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::SprintPressed);
    PlayerInputComponent->BindAction(
        TEXT("Sprint"),
        IE_Released,
        this,
        &AOGWorldPrototypeCharacter::SprintReleased);
    PlayerInputComponent->BindAction(
        TEXT("Dodge"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::DodgePressed);
    PlayerInputComponent->BindAction(
        TEXT("Interact"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::InteractPressed);
    PlayerInputComponent->BindAction(
        TEXT("LockOn"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::ToggleLockOn);
    PlayerInputComponent->BindAction(
        TEXT("PrimaryAttack"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::PrimaryAttackPressed);
    PlayerInputComponent->BindAction(
        TEXT("Skill1"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::Skill1Pressed);
    PlayerInputComponent->BindAction(
        TEXT("Skill2"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::Skill2Pressed);
    PlayerInputComponent->BindAction(
        TEXT("Ultimate"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::UltimatePressed);
    PlayerInputComponent->BindAction(
        TEXT("ManualAim"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::ManualAimPressed);
    PlayerInputComponent->BindAction(
        TEXT("ManualAim"),
        IE_Released,
        this,
        &AOGWorldPrototypeCharacter::ManualAimReleased);
    PlayerInputComponent->BindAction(
        TEXT("SwitchSlot1"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::SwitchSlot1Pressed);
    PlayerInputComponent->BindAction(
        TEXT("SwitchSlot2"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::SwitchSlot2Pressed);
    PlayerInputComponent->BindAction(
        TEXT("SwitchSlot3"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::SwitchSlot3Pressed);
    PlayerInputComponent->BindAction(
        TEXT("QTE"),
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::QtePressed);
    FInputActionBinding& PauseBinding =
        PlayerInputComponent->BindAction(
            TEXT("Pause"),
            IE_Pressed,
            this,
            &AOGWorldPrototypeCharacter::PausePressed);
    PauseBinding.bExecuteWhenPaused = true;

    PlayerInputComponent->BindTouch(
        IE_Pressed,
        this,
        &AOGWorldPrototypeCharacter::TouchPressed).bExecuteWhenPaused = true;
    PlayerInputComponent->BindTouch(
        IE_Repeat,
        this,
        &AOGWorldPrototypeCharacter::TouchMoved).bExecuteWhenPaused = true;
    PlayerInputComponent->BindTouch(
        IE_Released,
        this,
        &AOGWorldPrototypeCharacter::TouchReleased).bExecuteWhenPaused = true;
}

void AOGWorldPrototypeCharacter::MoveForward(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!Controller || FMath::Abs(Value) < MovementDeadzone ||
        (DiagnosticCombat && DiagnosticCombat->IsWorldDefeated()))
    {
        return;
    }

    if (CurrentCarrier.IsValid() &&
        CurrentCarrier->GetClass()->ImplementsInterface(
            UOGTraversalCarrier::StaticClass()))
    {
        IOGTraversalCarrier::Execute_AddCarrierMovementInput(
            CurrentCarrier.Get(),
            this,
            FVector2D(0.0f, Value));
        return;
    }

    const EOGTraversalMode Mode = TraversalCapabilities
        ? TraversalCapabilities->GetTraversalMode()
        : EOGTraversalMode::Ground;

    FVector Direction = FVector::ZeroVector;
    if (Mode == EOGTraversalMode::Climbing)
    {
        if (Value > 0.0f && bClimbTopBlocked) { return; }
        Direction = FVector::UpVector;
    }
    else if (Mode == EOGTraversalMode::Flying ||
             Mode == EOGTraversalMode::Swimming ||
             Mode == EOGTraversalMode::Diving)
    {
        Direction = Controller->GetControlRotation().Vector();
    }
    else
    {
        const FRotator ControlRotation = Controller->GetControlRotation();
        const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
        Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    }

    if (!Direction.IsNearlyZero())
    {
        LastNonZeroMoveDirection = Direction.GetSafeNormal();
        AddMovementInput(Direction, Value);
    }
}

void AOGWorldPrototypeCharacter::MoveRight(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!Controller || FMath::Abs(Value) < MovementDeadzone ||
        (DiagnosticCombat && DiagnosticCombat->IsWorldDefeated()))
    {
        return;
    }

    if (CurrentCarrier.IsValid() &&
        CurrentCarrier->GetClass()->ImplementsInterface(
            UOGTraversalCarrier::StaticClass()))
    {
        IOGTraversalCarrier::Execute_AddCarrierMovementInput(
            CurrentCarrier.Get(),
            this,
            FVector2D(Value, 0.0f));
        return;
    }

    const EOGTraversalMode Mode = TraversalCapabilities
        ? TraversalCapabilities->GetTraversalMode()
        : EOGTraversalMode::Ground;

    FVector Direction = FVector::ZeroVector;
    if (Mode == EOGTraversalMode::Climbing &&
        !ClimbSurfaceNormal.IsNearlyZero())
    {
        Direction = FVector::CrossProduct(
            FVector::UpVector,
            ClimbSurfaceNormal).GetSafeNormal();
    }
    else
    {
        const FRotator ControlRotation = Controller->GetControlRotation();
        const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
        Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
    }

    if (!Direction.IsNearlyZero())
    {
        LastNonZeroMoveDirection = Direction.GetSafeNormal();
        AddMovementInput(Direction, Value);
    }
}

void AOGWorldPrototypeCharacter::Turn(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    AddControllerYawInput(Value);
}

void AOGWorldPrototypeCharacter::LookUp(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    AddControllerPitchInput(Value);
}

void AOGWorldPrototypeCharacter::TurnRate(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!GetWorld() || FMath::Abs(Value) < LookDeadzone)
    {
        return;
    }

    constexpr float BaseYawDegreesPerSecond = 165.0f;
    const float Direction = bInvertCameraX ? -1.0f : 1.0f;
    AddControllerYawInput(
        ShapeLookInput(Value) *
        Direction *
        CameraHorizontalSensitivity *
        BaseYawDegreesPerSecond *
        GetWorld()->GetDeltaSeconds());
}

void AOGWorldPrototypeCharacter::LookUpRate(float Value)
{
    if (!IsFoundationWorldInputAllowed()) return;
    if (!GetWorld() || FMath::Abs(Value) < LookDeadzone)
    {
        return;
    }

    constexpr float BasePitchDegreesPerSecond = 120.0f;
    const float Direction = bInvertCameraY ? -1.0f : 1.0f;
    AddControllerPitchInput(
        ShapeLookInput(Value) *
        Direction *
        CameraVerticalSensitivity *
        BasePitchDegreesPerSecond *
        GetWorld()->GetDeltaSeconds());
}

AOGWorldPresentationGameMode::AOGWorldPresentationGameMode()
{
    DefaultPawnClass = AOGWorldPrototypeCharacter::StaticClass();
    HUDClass = AOGWorldPresentationHud::StaticClass();
}

void AOGWorldPresentationGameMode::StartPlay()
{
    // The title/opening surface is the first player-visible state. It uses the
    // Ruler presentation boundary so the 3D world is suspended and portrait
    // orientation is applied before the player chooses Continue.
    if (GetWorld() && GetWorld()->GetWorldSettings()) GetWorld()->GetWorldSettings()->bEnableWorldBoundsChecks = false;
    Super::StartPlay();

    if (UWorld* World = GetWorld())
    {
        // StartingWorld is the disposable Foundation integration course.
        if (AOGWorldPrototypeCharacter* Character = Cast<AOGWorldPrototypeCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
        { Character->InitializeFoundationIntegration(); }
        if (AreFoundationDebugFixturesEnabled() && !UGameplayStatics::GetPlayerCharacter(World, 0))
        {
            if (AOGWorldPrototypeCharacter* Character =
                    Cast<AOGWorldPrototypeCharacter>(
                        UGameplayStatics::GetPlayerCharacter(World, 0)))
            {
            auto GroundedPoint =
                [World](
                    const FVector& Desired,
                    float HalfHeight)
                {
                    FHitResult Hit;
                    const FVector Start =
                        Desired + FVector(0.0f, 0.0f, 3000.0f);
                    const FVector End =
                        Desired - FVector(0.0f, 0.0f, 5000.0f);
                    FCollisionQueryParams Params(
                        SCENE_QUERY_STAT(FoundationFixtureGround),
                        false);
                    if (World->LineTraceSingleByChannel(
                            Hit,
                            Start,
                            End,
                            ECC_Visibility,
                            Params))
                    {
                        return Hit.Location +
                            FVector(0.0f, 0.0f, HalfHeight);
                    }
                    return Desired;
                };

            const FVector Origin = Character->GetActorLocation();
            const FVector Forward = Character->GetActorForwardVector();
            const FVector Right = Character->GetActorRightVector();

            World->SpawnActor<AOGFoundationCombatTarget>(
                AOGFoundationCombatTarget::StaticClass(),
                GroundedPoint(
                    Origin + Forward * 720.0f + Right * 230.0f,
                    62.5f),
                FRotator::ZeroRotator);

            World->SpawnActor<AOGFoundationCombatTarget>(
                AOGFoundationCombatTarget::StaticClass(),
                GroundedPoint(
                    Origin + Forward * 850.0f - Right * 260.0f,
                    62.5f),
                FRotator::ZeroRotator);

            World->SpawnActor<AOGFoundationInteractableMarker>(
                AOGFoundationInteractableMarker::StaticClass(),
                GroundedPoint(
                    Origin + Right * 330.0f + Forward * 180.0f,
                    42.5f),
                FRotator::ZeroRotator);

            World->SpawnActor<AOGFoundationClimbableWall>(
                AOGFoundationClimbableWall::StaticClass(),
                GroundedPoint(
                    Origin + Forward * 620.0f - Right * 520.0f,
                    160.0f),
                FRotator(
                    0.0f,
                    Character->GetActorRotation().Yaw,
                    0.0f));

            AOGFoundationTraversalCarrier* Mount =
                World->SpawnActor<AOGFoundationTraversalCarrier>(
                    AOGFoundationTraversalCarrier::StaticClass(),
                    GroundedPoint(
                        Origin + Right * 720.0f + Forward * 140.0f,
                        30.0f),
                    FRotator(
                        0.0f,
                        Character->GetActorRotation().Yaw,
                        0.0f));
            if (Mount)
            {
                Mount->SetFoundationCarrierMode(
                    EOGTraversalMode::Mounted);
            }

            AOGFoundationTraversalCarrier* Vehicle =
                World->SpawnActor<AOGFoundationTraversalCarrier>(
                    AOGFoundationTraversalCarrier::StaticClass(),
                    GroundedPoint(
                        Origin + Right * 720.0f - Forward * 420.0f,
                        30.0f),
                    FRotator(
                        0.0f,
                        Character->GetActorRotation().Yaw,
                        0.0f));
            if (Vehicle)
            {
                Vehicle->SetFoundationCarrierMode(
                    EOGTraversalMode::Vehicle);
            }

                World->SpawnActor<AOGFoundationSwimStation>(
                    AOGFoundationSwimStation::StaticClass(),
                    GroundedPoint(
                        Origin - Right * 620.0f + Forward * 160.0f,
                        17.5f),
                    FRotator::ZeroRotator);
            }
        }

        if (APlayerController* PlayerController =
                World->GetFirstPlayerController())
        {
            // UE 5.8 exposes this protected flag as a writable reflected property.
            FindFieldChecked<FBoolProperty>(APlayerController::StaticClass(),
                TEXT("bShouldPerformFullTickWhenPaused"))->SetPropertyValue_InContainer(PlayerController, true);
            PlayerController->SetTickableWhenPaused(true);
            PlayerController->bEnableTouchEvents = true;
            PlayerController->bEnableClickEvents = true;
            PlayerController->SetVirtualJoystickVisibility(false);
        }
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGPresentationModeSubsystem* Presentation =
                GameInstance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            Presentation->SetMode(EOGPresentationMode::Ruler);
        }
    }
}

static FString FoundationIdentityLabel(const FOGContentId& Identity)
{
    const FString Id = Identity.ToString();
    if (Id == TEXT("diagnostic:identity.kit_0")) return TEXT("Protagonist");
    if (Id == TEXT("diagnostic:identity.kit_1")) return TEXT("Guardian");
    if (Id == TEXT("diagnostic:identity.kit_2")) return TEXT("Striker");
    return Id;
}

void AOGWorldPresentationHud::DrawFoundationText(const FString& Text, FLinearColor Color,
    float X, float Y, UFont* Font, float TextScale, bool bScalePosition)
{
    UFont* ReadableFont = Font ? Font : GEngine ? GEngine->GetLargeFont() : nullptr;
    const float Height = ReadableFont ? FMath::Max(1.0f, ReadableFont->GetMaxCharHeight()) : 1.0f;
    const float Minimum = Canvas ? FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f : 40.0f;
    FString Display = Text;
    Display.ReplaceInline(TEXT("diagnostic:identity.kit_0"), TEXT("Protagonist"));
    Display.ReplaceInline(TEXT("diagnostic:identity.kit_1"), TEXT("Guardian"));
    Display.ReplaceInline(TEXT("diagnostic:identity.kit_2"), TEXT("Striker"));
    Display.ReplaceInline(TEXT("diagnostic:banner.companions"), TEXT("Companion training"));
    Display.ReplaceInline(TEXT("diagnostic:ticket.companions"), TEXT("Summon ticket"));
    const float EffectiveScale = FMath::Max(TextScale, Minimum / Height);
    const float ShadowOffset = FMath::Max(1.0f, Minimum * 0.05f);
    Super::DrawText(Display, FLinearColor(0.0f, 0.0f, 0.0f, Color.A),
        X + ShadowOffset, Y + ShadowOffset, ReadableFont, EffectiveScale, bScalePosition);
    Super::DrawText(Display, Color, X, Y, ReadableFont, EffectiveScale, bScalePosition);
}

FOGEntityId AOGWorldPresentationHud::GetFoundationRulerPresentationOwner() const
{
    return FoundationRulerOwner.IsValid()
        ? FoundationRulerOwner
        : FoundationRulerProjectionId();
}

void AOGWorldPresentationHud::SetFoundationRulerPresentationOwner(
    const FOGEntityId& RulerId)
{
    FoundationRulerOwner = RulerId;
    SelectedRosterIndex = INDEX_NONE;
    SelectedManifestationIndex = INDEX_NONE;
    SelectedTerritoryIndex = INDEX_NONE;
    // A banner command context cannot silently move between owners.
    ClearActiveGachaPresentation();
}

void AOGWorldPresentationHud::SetActiveGachaPresentation(
    const FOGGachaBannerDefinition& Banner,
    int64 CanonicalWorldTick,
    int64 DeterministicSeed)
{
    FString Error;
    if (!FOGGachaService::ValidateBanner(Banner, Error) ||
        CanonicalWorldTick < 0)
    {
        bHasActiveGachaPresentation = false;
        ActiveGachaBanner = FOGGachaBannerDefinition();
        GachaCommandWorldTick = 0;
        GachaCommandSeed = 0;
        FoundationStatusMessage =
            Error.IsEmpty()
                ? TEXT("Gacha presentation rejected: invalid command context.")
                : FString::Printf(
                    TEXT("Gacha presentation rejected: %s"),
                    *Error);
        return;
    }

    ActiveGachaBanner = Banner;
    GachaCommandWorldTick = CanonicalWorldTick;
    GachaCommandSeed = DeterministicSeed;
    bHasActiveGachaPresentation = true;
    FoundationStatusMessage.Reset();
}

void AOGWorldPresentationHud::ClearActiveGachaPresentation()
{
    bHasActiveGachaPresentation = false;
    LastGachaResults.Reset();
    ActiveGachaBanner = FOGGachaBannerDefinition();
    GachaCommandWorldTick = 0;
    GachaCommandSeed = 0;
}

void AOGWorldPresentationHud::AddFoundationHitBox(
    const FVector2D& Position,
    const FVector2D& Size,
    FName Name,
    bool bConsumesInput,
    int32 Priority)
{
    if (Size.X <= 0.0f || Size.Y <= 0.0f)
    {
        return;
    }

    // Match UCanvas item coordinates and the active FCanvas drawing stack.
    // Safe-zone/DPI transforms may move or scale drawing without changing Org.
    // Derive both edges each frame; never retain an orientation's old geometry.
    const FVector2D Origin = Canvas
        ? FVector2D(Canvas->OrgX, Canvas->OrgY)
        : FVector2D::ZeroVector;
    const FMatrix DrawTransform = Canvas && Canvas->Canvas
        ? Canvas->Canvas->GetTransform()
        : FMatrix::Identity;
    const FVector2D Corners[] = {
        Position + Origin,
        Position + Origin + FVector2D(Size.X, 0.0f),
        Position + Origin + FVector2D(0.0f, Size.Y),
        Position + Origin + Size
    };
    FVector2D Minimum(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
    FVector2D Maximum(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
    for (const FVector2D& Corner : Corners)
    {
        const FVector4 Screen = DrawTransform.TransformFVector4(
            FVector4(Corner.X, Corner.Y, 0.0, 1.0));
        if (!FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y))
        {
            return;
        }
        Minimum.X = FMath::Min(Minimum.X, Screen.X);
        Minimum.Y = FMath::Min(Minimum.Y, Screen.Y);
        Maximum.X = FMath::Max(Maximum.X, Screen.X);
        Maximum.Y = FMath::Max(Maximum.Y, Screen.Y);
    }

    // AHUD performs its own local-player viewport-offset mapping on input.
    // Registration stays in that player's drawing space; do not add it twice.
    AHUD::AddHitBox(Minimum, Maximum - Minimum, Name, bConsumesInput, Priority);
}

void AOGWorldPresentationHud::SetFoundationSurface(FName Surface)
{
    FoundationSurface = Surface;
    GachaOverlay = NAME_None;
    PressedWorldControls.Reset();

    AOGWorldPrototypeCharacter* Character =
        PlayerOwner
            ? Cast<AOGWorldPrototypeCharacter>(PlayerOwner->GetPawn())
            : nullptr;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGPresentationModeSubsystem* Presentation =
                GameInstance->GetSubsystem<UOGPresentationModeSubsystem>())
        {
            const bool bWorldSurface =
                Surface == FName(TEXT("world"));
            Presentation->SetMode(
                bWorldSurface
                    ? EOGPresentationMode::World
                    : EOGPresentationMode::Ruler);
        }
    }

    if (Character)
    {
        Character->HandleWorldHudControl(TEXT("OG.World.Jump"), false);
        Character->HandleWorldHudControl(TEXT("OG.World.Descend"), false);
        Character->HandleWorldHudControl(TEXT("OG.World.Sprint"), false);
        Character->FindComponentByClass<UOGSemanticAudioRuntime>()->SetWorldPresentationActive(Surface == TEXT("world"));
    }
    if (Character) Character->RefreshFoundationInputState();
}

void AOGWorldPresentationHud::DrawFoundationOpening(
    float Scale,
    const FLinearColor& MainText,
    const FLinearColor& SubText,
    const FLinearColor& Panel,
    const FLinearColor& Button)
{
    if (!Canvas)
    {
        return;
    }

    // Fit the logical menu to the viewport; the world HUD scale cap must not
    // shrink a full portrait screen to a small desktop diagnostic panel.
    Scale = FMath::Min(Canvas->ClipX / 480.0f, Canvas->ClipY / 900.0f);

    const FString DatabasePath =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("WorldState.db"));
    const FString SnapshotDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("Snapshots"));
    const FString RecoveryDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("MigrationRecovery"));
    const FString ImportDirectory =
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("OfflineGame"),
            TEXT("Import"));

    const bool bWorldExists =
        IFileManager::Get().FileExists(*DatabasePath);

    TArray<FString> SnapshotFiles;
    IFileManager::Get().FindFilesRecursive(
        SnapshotFiles,
        *SnapshotDirectory,
        TEXT("*.db"),
        true,
        false);
    TArray<FString> RecoveryFiles;
    IFileManager::Get().FindFilesRecursive(
        RecoveryFiles,
        *RecoveryDirectory,
        TEXT("*.db"),
        true,
        false);
    const bool bRecoverable =
        !SnapshotFiles.IsEmpty() ||
        !RecoveryFiles.IsEmpty();

    TArray<FString> ImportFiles;
    IFileManager::Get().FindFilesRecursive(
        ImportFiles,
        *ImportDirectory,
        TEXT("*"),
        true,
        false);
    const bool bImportAvailable =
        !ImportFiles.IsEmpty();

    FOGPlayerProfileSettings Profile;
    FString ProfileError;
    FOGPlayerProfileSettingsService::Load(
        FOGPlayerProfileSettingsService::DefaultProfilePath(),
        Profile,
        ProfileError);

    const FOGOpeningViewModel Opening =
        FOGUiViewModelService::BuildOpening(
            bWorldExists,
            bRecoverable,
            bImportAvailable,
            Profile.bReducedMotion);

    const float PanelW =
        FMath::Min(560.0f * Scale, Canvas->ClipX * 0.88f);
    const float PanelH =
        FMath::Min(650.0f * Scale, Canvas->ClipY * 0.84f);
    const float X = (Canvas->ClipX - PanelW) * 0.5f;
    const float Y = (Canvas->ClipY - PanelH) * 0.5f;
    const float MotionPhase =
        Opening.bReducedMotionPresentation || !GetWorld()
            ? 0.0f
            : static_cast<float>(
                GetWorld()->GetRealTimeSeconds());
    const float TitleFloat =
        Opening.bReducedMotionPresentation
            ? 0.0f
            : FMath::Sin(MotionPhase * 0.85f) * 5.0f * Scale;
    DrawGameBackdrop(0, 0, Canvas->ClipX, Canvas->ClipY);
    DrawGamePanel(Panel, X, Y, PanelW, PanelH);

    if (!Opening.bReducedMotionPresentation)
    {
        const float Sweep =
            FMath::Fmod(MotionPhase * 38.0f, PanelW);
        DrawRect(
            FLinearColor(0.34f, 0.40f, 0.46f, 0.20f),
            X + Sweep,
            Y + 10.0f * Scale,
            2.0f * Scale,
            72.0f * Scale);
    }

    DrawFoundationText(
        TEXT("Your journey"),
        MainText,
        X + 32.0f * Scale,
        Y + 30.0f * Scale + TitleFloat,
        nullptr,
        1.05f * Scale,
        false);
    DrawFoundationText(
        TEXT("Continue exploring your world"),
        SubText,
        X + 32.0f * Scale,
        Y + 64.0f * Scale,
        nullptr,
        0.62f * Scale,
        false);

    auto DrawOpeningButton =
        [&](const FString& Label,
            FName Name,
            float ButtonY,
            bool bEnabled,
            bool bPrimary = false,
            bool bDestructive = false)
        {
            const FLinearColor Fill =
                bEnabled
                    ? (bDestructive ? FLinearColor(0.23f, 0.08f, 0.09f, 0.98f)
                        : (bPrimary ? FLinearColor(0.16f, 0.29f, 0.37f, 0.98f) : Button))
                    : FLinearColor(0.06f, 0.06f, 0.06f, 0.62f);
            const float ButtonX = X + 32.0f * Scale;
            const float ButtonW = PanelW - 64.0f * Scale;
            const float ButtonH = (bPrimary ? 62.0f : 48.0f) * Scale;
            DrawRect(
                Fill,
                ButtonX,
                ButtonY,
                ButtonW,
                ButtonH);
            DrawFoundationText(
                Label,
                bEnabled ? MainText : SubText,
                ButtonX + 16.0f * Scale,
                ButtonY + 14.0f * Scale,
                nullptr,
                0.66f * Scale,
                false);
            if (bEnabled && OpeningPage != FName(TEXT("confirm_clear")))
            {
                AddFoundationHitBox(
                    FVector2D(ButtonX, ButtonY),
                    FVector2D(ButtonW, ButtonH),
                    Name,
                    true,
                    120);
            }
        };

    float ButtonY = Y + 120.0f * Scale;
    DrawOpeningButton(
        TEXT("Continue"),
        FName(TEXT("OG.Opening.Continue")),
        ButtonY,
        Opening.bContinueAvailable,
        true);
    ButtonY += 78.0f * Scale;
    DrawOpeningButton(
        TEXT("Recover Existing World"),
        FName(TEXT("OG.Opening.Recover")),
        ButtonY,
        Opening.bRecoverExistingWorldAvailable);
    ButtonY += 64.0f * Scale;
    DrawOpeningButton(
        TEXT("Import Backup"),
        FName(TEXT("OG.Opening.Import")),
        ButtonY,
        Opening.bImportBackupAvailable);
    ButtonY += 64.0f * Scale;
    DrawOpeningButton(
        TEXT("Settings"),
        FName(TEXT("OG.Settings.Toggle")),
        ButtonY,
        true);
    // Secondary utilities stay below the primary title actions.
    ButtonY += 68.0f * Scale;
    DrawFoundationText(TEXT("World utilities"), SubText,
        X + 32.0f * Scale, ButtonY - 22.0f * Scale,
        nullptr, 0.46f * Scale, false);
    DrawOpeningButton(
        TEXT("Ruler Mode"),
        FName(TEXT("OG.Opening.Ruler")),
        ButtonY,
        true);
    ButtonY += 64.0f * Scale;
    DrawOpeningButton(
        TEXT("Clear World... (creates recovery copy)"),
        FName(TEXT("OG.Opening.Clear")),
        ButtonY,
        Opening.bContinueAvailable,
        false,
        true);

    if (OpeningPage == FName(TEXT("confirm_clear")))
    {
        const float ConfirmY =
            Y + PanelH - 142.0f * Scale;
        DrawRect(
            FLinearColor(0.12f, 0.045f, 0.045f, 0.97f),
            X + 24.0f * Scale,
            ConfirmY - 50.0f * Scale,
            PanelW - 48.0f * Scale,
            132.0f * Scale);
        AddFoundationHitBox(FVector2D(X, Y), FVector2D(PanelW, PanelH),
            FName(TEXT("OG.Opening.ClearModal")), true, 124);
        DrawFoundationText(
            TEXT("Clear world? A recovery copy is created first."),
            MainText,
            X + 38.0f * Scale,
            ConfirmY - 34.0f * Scale,
            nullptr,
            0.54f * Scale,
            false);

        const float ConfirmW =
            (PanelW - 86.0f * Scale) * 0.5f;
        DrawRect(
            Button,
            X + 32.0f * Scale,
            ConfirmY,
            ConfirmW,
            44.0f * Scale);
        DrawFoundationText(
            TEXT("Cancel"),
            MainText,
            X + 48.0f * Scale,
            ConfirmY + 11.0f * Scale,
            nullptr,
            0.58f * Scale,
            false);
        AddFoundationHitBox(
            FVector2D(X + 32.0f * Scale, ConfirmY),
            FVector2D(ConfirmW, 44.0f * Scale),
            FName(TEXT("OG.Opening.ClearCancel")),
            true,
            125);

        const float ConfirmX =
            X + 44.0f * Scale + ConfirmW;
        DrawRect(
            FLinearColor(0.28f, 0.08f, 0.08f, 0.98f),
            ConfirmX,
            ConfirmY,
            ConfirmW,
            44.0f * Scale);
        DrawFoundationText(
            TEXT("Confirm Clear"),
            MainText,
            ConfirmX + 16.0f * Scale,
            ConfirmY + 11.0f * Scale,
            nullptr,
            0.58f * Scale,
            false);
        AddFoundationHitBox(
            FVector2D(ConfirmX, ConfirmY),
            FVector2D(ConfirmW, 44.0f * Scale),
            FName(TEXT("OG.Opening.ClearConfirm")),
            true,
            126);
    }

    if (!FoundationStatusMessage.IsEmpty())
    {
        DrawFoundationText(
            FoundationStatusMessage,
            SubText,
            X + 32.0f * Scale,
            Y + PanelH - 24.0f * Scale,
            nullptr,
            0.48f * Scale,
            false);
    }
    else if (OpeningPage == FName(TEXT("recover")))
    {
        DrawFoundationText(
            TEXT("Recovery data detected. Restore/import operations are exposed from Records > Recovery."),
            SubText,
            X + 32.0f * Scale,
            Y + PanelH - 72.0f * Scale,
            nullptr,
            0.52f * Scale,
            false);
    }
    else if (OpeningPage == FName(TEXT("import")))
    {
        DrawFoundationText(
            TEXT("Backup package detected in the import area. Open Records > Recovery to validate it before restore."),
            SubText,
            X + 32.0f * Scale,
            Y + PanelH - 72.0f * Scale,
            nullptr,
            0.52f * Scale,
            false);
    }
}

void AOGWorldPresentationHud::DrawFoundationRuler(
    float Scale,
    const FLinearColor& MainText,
    const FLinearColor& SubText,
    const FLinearColor& Panel,
    const FLinearColor& Button)
{
    if (!Canvas)
    {
        return;
    }

    // Fit the logical menu to the viewport; the world HUD scale cap must not
    // shrink a full portrait screen to a small desktop diagnostic panel.
    Scale = FMath::Min(Canvas->ClipX / 480.0f, Canvas->ClipY / 900.0f);

    bool bProjectionReady = false;
    bool bGachaUnlocked = false;
    int32 UnacknowledgedReports = 0;
    int32 RosterIdentityCount = 0;
    int32 TerritoryClaimCount = 0;
    TArray<FOGRosterIdentityViewModel> Roster;
    FOGTerritoryViewModel TerritoryView;
    FOGRecordsHubViewModel RecordsHub;
    TArray<FOGReportRecord> Reports;
    TArray<FOGChronicleEntryViewModel> ChronicleEntries;
    TArray<FOGIntelligenceEntryViewModel> IntelligenceEntries;
    FOGCodexViewModel CodexView;
    TArray<FOGGachaHistoryEntryViewModel> GachaHistory;
    FOGGachaViewModel GachaView;
    bool bGachaViewReady = false;
    TArray<FOGBackupCatalogEntry> BackupCatalog;
    FOGBackupManagerViewModel BackupManager;
    FOGPackageStorageViewModel PackageStorage;
    FString ProjectionError;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOGGameCoreSubsystem* Core =
                GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
        {
            if (IOGWorldStore* Store = Core->GetWorldStore())
            {
                FOGUiViewModelService Ui(*Store);
                FOGRulerShellViewModel Shell;
                if (Ui.BuildRulerShell(
                        GetFoundationRulerPresentationOwner(),
                        Shell,
                        ProjectionError))
                {
                    bProjectionReady = true;
                    bGachaUnlocked = Shell.bGachaUnlocked;
                    UnacknowledgedReports =
                        Shell.UnacknowledgedReportCount;

                    FString RosterError;
                    if (Ui.BuildRoster(
                            GetFoundationRulerPresentationOwner(),
                            Roster,
                            RosterError))
                    {
                        RosterIdentityCount = Roster.Num();
                    }

                    FString TerritoryError;
                    if (Ui.BuildTerritory(
                            GetFoundationRulerPresentationOwner(),
                            FOGEntityId(),
                            TerritoryView,
                            TerritoryError))
                    {
                        TerritoryClaimCount =
                            TerritoryView.Territories.Num();
                    }

                    FString RecordsError;
                    Ui.BuildRecordsHub(
                        GetFoundationRulerPresentationOwner(),
                        RecordsHub,
                        RecordsError);
                    Store->ListReportsByOwner(
                        GetFoundationRulerPresentationOwner(),
                        Reports,
                        RecordsError);
                    Ui.BuildChronicle(
                        GetFoundationRulerPresentationOwner(),
                        20,
                        ChronicleEntries,
                        RecordsError);
                    Ui.BuildIntelligence(
                        GetFoundationRulerPresentationOwner(),
                        IntelligenceEntries,
                        RecordsError);

                    TArray<FOGCodexEntryViewModel> KnownCodexEntries;
                    KnownCodexEntries.Reserve(
                        IntelligenceEntries.Num());
                    for (const FOGIntelligenceEntryViewModel& Intelligence :
                         IntelligenceEntries)
                    {
                        if (Intelligence.FactKey.IsNone())
                        {
                            continue;
                        }

                        FOGCodexEntryViewModel CodexEntry;
                        CodexEntry.EntryId =
                            FOGContentId(
                                Intelligence.FactKey.ToString());
                        CodexEntry.Category =
                            FName(TEXT("other"));
                        CodexEntry.DisplayNameKey =
                            Intelligence.FactKey.ToString();
                        CodexEntry.KnowledgeState =
                            Intelligence.Knowledge.KnowledgeState;
                        KnownCodexEntries.Add(
                            MoveTemp(CodexEntry));
                    }
                    CodexView =
                        FOGUiViewModelService::BuildCodex(
                            KnownCodexEntries,
                            FString(),
                            NAME_None);

                    if (bGachaUnlocked)
                    {
                        Ui.BuildGachaHistory(
                            GetFoundationRulerPresentationOwner(),
                            20,
                            GachaHistory,
                            RecordsError);

                        if (bHasActiveGachaPresentation)
                        {
                            FString GachaError;
                            bGachaViewReady =
                                Ui.BuildGacha(
                                    GetFoundationRulerPresentationOwner(),
                                    ActiveGachaBanner,
                                    GachaView,
                                    GachaError);
                            if (!bGachaViewReady &&
                                ProjectionError.IsEmpty())
                            {
                                ProjectionError = GachaError;
                            }
                        }
                    }

                    const FString RecoveryCatalogPath =
                        FPaths::Combine(
                            FPaths::ProjectSavedDir(),
                            TEXT("OfflineGame"),
                            TEXT("RecoveryCatalog.json"));
                    FString BackupError;
                    if (FOGRecoveryCatalogService::LoadEntries(
                            RecoveryCatalogPath,
                            BackupCatalog,
                            BackupError))
                    {
                        BackupManager =
                            FOGUiViewModelService::BuildBackupManager(
                                BackupCatalog);
                    }

                    FString PackageError;
                    Ui.BuildPackageStorage(
                        [](const FOGContentPackageRecord& Package,
                           bool& bOutKnown,
                           int64& OutSizeBytes,
                           FString& OutError)
                        {
                            OutError.Reset();
                            bOutKnown = false;
                            OutSizeBytes = 0;
                            if (!Package.InstallUri.IsEmpty() &&
                                IFileManager::Get().FileExists(
                                    *Package.InstallUri))
                            {
                                OutSizeBytes =
                                    IFileManager::Get().FileSize(
                                        *Package.InstallUri);
                                bOutKnown = OutSizeBytes >= 0;
                            }
                            return true;
                        },
                        [](const FOGContentPackageRecord& Package,
                           bool& bOutMoveAllowed,
                           bool& bOutArchiveAllowed,
                           FString& OutError)
                        {
                            OutError.Reset();
                            const bool bFile =
                                !Package.InstallUri.IsEmpty() &&
                                IFileManager::Get().FileExists(
                                    *Package.InstallUri);
                            const bool bDirectory =
                                !Package.InstallUri.IsEmpty() &&
                                IFileManager::Get().DirectoryExists(
                                    *Package.InstallUri);
                            const bool bStorageSafe =
                                Package.bInstalled &&
                                Package.bValidated &&
                                !Package.bActivated;
                            bOutMoveAllowed =
                                bStorageSafe &&
                                (bFile || bDirectory);
                            // Android SAF export is file-backed; directories
                            // are deliberately not advertised as archivable.
#if PLATFORM_ANDROID
                            bOutArchiveAllowed =
                                bStorageSafe && bFile;
#else
                            bOutArchiveAllowed = false;
#endif
                            return true;
                        },
                        PackageStorage,
                        PackageError);
                    CachedPackageEntries =
                        PackageStorage.Packages;
                }
            }
        }
    }

    // Game-first surfaces use the same canonical projections and command IDs.
    // Recovery/storage detail screens below retain their full management flows.
    if (RulerDestination != TEXT("records") || RecordsDestination == TEXT("hub"))
    {
        const float S=FMath::Min(Canvas->ClipX,Canvas->ClipY)/1080.f;
        const float W=Canvas->ClipX,H=Canvas->ClipY,M=32*S,F=32*S;
        const bool Portrait=H>W;
        const FLinearColor White(.94f,.94f,.89f,1), Muted(.58f,.68f,.75f,1), Gold(.83f,.69f,.4f,1), Teal(.38f,.72f,.75f,1);
        DrawGameBackdrop(0,0,W,H);
        FString Page=RulerDestination==TEXT("home")?TEXT("Your journey"):
            RulerDestination==TEXT("characters")?TEXT("Characters"):
            RulerDestination==TEXT("gacha")?TEXT("Summon"):
            RulerDestination==TEXT("territory")?TEXT("World map"):TEXT("Journal");
        DrawGameText(Page,M,28*S,48*S,White,W*.48f);
        DrawGameButton(TEXT("Menu"),TEXT("OG.Diagnostics.Open"),W-390*S,26*S,150*S,66*S);
        DrawGameButton(TEXT("Settings"),TEXT("OG.Settings.Toggle"),W-220*S,26*S,188*S,66*S);
        const float Top=125*S,NavH=118*S,Bottom=H-NavH-96*S;
        const float AreaH=Bottom-Top,AreaW=W-2*M;
        auto NameOf=[&](const FOGRosterIdentityViewModel& I)
        {return I.DisplayNameKey.IsEmpty()?FoundationIdentityLabel(I.IdentityId):I.DisplayNameKey;};
        const TCHAR* Dest[]={TEXT("home"),TEXT("characters"),TEXT("gacha"),TEXT("territory"),TEXT("records")};
        const TCHAR* Titles[]={TEXT("Home"),TEXT("Characters"),TEXT("Summon"),TEXT("World"),TEXT("Journal")};
        DrawRect(FLinearColor(.022f,.035f,.055f,.97f),0,H-NavH,W,NavH);
        for(int32 I=0;I<5;++I)
        {
            const float NX=I*W/5,NW=W/5;
            const bool Selected=RulerDestination==Dest[I];
            if(Selected) DrawRect(Gold,NX+NW*.15f,H-NavH,NW*.7f,3*S);
            DrawGameIcon(Dest[I],NX+NW*.5f-23*S,H-NavH+15*S,46*S,Selected?Gold:Muted);
            DrawGameText(Titles[I],NX+16*S,H-42*S,28*S,Selected?White:Muted,NW-24*S);
            AddFoundationHitBox({NX,H-NavH},{NW,NavH},FName(*FString::Printf(TEXT("OG.Ruler.%s"),Dest[I])),true,119);
        }
        if(RulerDestination==TEXT("home"))
        {
            const int32 I=Roster.IsValidIndex(SelectedRosterIndex)?SelectedRosterIndex:0;
            const float ArtW=Portrait?AreaW:AreaW*.57f;
            DrawFoundationCharacterStandIn(M,Top,ArtW,AreaH,Teal);
            DrawGameText(TEXT("YOUR COMPANIONS"),M+28*S,Top+24*S,26*S,Gold,ArtW-56*S);
            DrawGameText(Roster.IsValidIndex(I)?NameOf(Roster[I]):TEXT("Your journey"),
                M+28*S,Top+AreaH-110*S,52*S,White,ArtW-56*S);
            const float PX=Portrait?M+24*S:M+ArtW+34*S;
            const float PW=Portrait?AreaW-48*S:AreaW-ArtW-34*S;
            const float PY=Portrait?Top+AreaH*.60f:Top+AreaH*.24f;
            DrawGamePanel(FLinearColor(.025f,.045f,.075f,.90f),PX,PY,PW,240*S);
            DrawGameText(TEXT("CONTINUE YOUR ADVENTURE"),PX+24*S,PY+22*S,28*S,Gold,PW-48*S);
            DrawGameText(FString::Printf(TEXT("%d characters  /  %d holdings"),RosterIdentityCount,TerritoryClaimCount),
                PX+24*S,PY+68*S,F,Muted,PW-48*S);
            DrawGameButton(TEXT("Enter world"),TEXT("OG.Ruler.World"),PX+24*S,PY+125*S,PW-48*S,82*S,true);
            if(!Portrait)
            {
                DrawGameButton(TEXT("Build your party"),TEXT("OG.Ruler.characters"),PX,PY+266*S,PW,76*S);
                DrawGameButton(TEXT("Summon companions"),TEXT("OG.Ruler.gacha"),PX,PY+356*S,PW,76*S);
            }
        }
        else if(RulerDestination==TEXT("characters"))
        {
            if(!Roster.IsValidIndex(SelectedRosterIndex))
            {
                const int32 Columns=Portrait?2:4;
                const float Gap=24*S,CW=(AreaW-Gap*(Columns-1))/Columns;
                const float CH=FMath::Min(CW*1.34f,AreaH*.65f);
                const int32 Rows=FMath::Max(1,FMath::FloorToInt((AreaH-80*S)/(CH+Gap)));
                const int32 Count=Columns*Rows;
                RosterPage=FMath::Clamp(RosterPage,0,FMath::Max(0,(Roster.Num()-1)/Count));
                for(int32 I=RosterPage*Count;I<Roster.Num() && I<(RosterPage+1)*Count;++I)
                {
                    int32 Slot=I-RosterPage*Count;
                    const float X=M+(Slot%Columns)*(CW+Gap),Y=Top+(Slot/Columns)*(CH+Gap);
                    const auto& Identity=Roster[I];
                    FName Rarity=Identity.Manifestations.IsEmpty()?NAME_None:Identity.Manifestations[0].CurrentRarity;
                    const FLinearColor Accent=I%2==0?Teal:FLinearColor(.65f,.48f,.76f,1);
                    DrawFoundationCharacterStandIn(X,Y,CW,CH,Accent);
                    DrawRect(Accent,X,Y+CH-4*S,CW,4*S);
                    DrawGameText(Rarity.IsNone()?TEXT("COMPANION"):Rarity.ToString(),X+18*S,Y+16*S,26*S,Gold,CW-36*S);
                    DrawGameText(NameOf(Identity),X+18*S,Y+CH-96*S,38*S,White,CW-36*S);
                    DrawGameText(FString::Printf(TEXT("%d owned"),Identity.ManifestationCount),X+18*S,Y+CH-48*S,28*S,Muted,CW-36*S);
                    AddFoundationHitBox({X,Y},{CW,CH},FName(*FString::Printf(TEXT("OG.Characters.Identity.%d"),I)),true,122);
                }
                if(Roster.IsEmpty()) DrawGameText(TEXT("Your companions will appear here."),M,Top+60*S,F,Muted,AreaW);
                if(Roster.Num()>Count)
                {
                    DrawGameButton(TEXT("Previous"),TEXT("OG.Presentation.RosterPrevious"),M,Bottom-72*S,210*S,64*S);
                    DrawGameButton(TEXT("Next"),TEXT("OG.Presentation.RosterNext"),W-M-210*S,Bottom-72*S,210*S,64*S);
                }
            }
            else
            {
                const auto& Identity=Roster[SelectedRosterIndex];
                const float PW=Portrait?AreaW*.43f:AreaW*.42f;
                const float PH=Portrait?FMath::Min(AreaH*.32f,420*S):AreaH;
                DrawFoundationCharacterStandIn(M,Top,PW,PH,Teal);
                const float DX=M+PW+28*S,DW=AreaW-PW-28*S;
                DrawGameText(NameOf(Identity),DX,Top+18*S,44*S,White,DW);
                DrawGameText(TEXT("Choose a copy for your party"),DX,Top+86*S,28*S,Muted,DW);
                DrawGameButton(TEXT("Back"),TEXT("OG.Characters.Back"),DX,Top+142*S,DW,68*S);
                const float LX=Portrait?M:DX,LW=Portrait?AreaW:DW;
                float LY=Portrait?Top+PH+28*S:Top+240*S;
                if(Identity.Manifestations.IsValidIndex(SelectedManifestationIndex))
                {
                    const float BW=(LW-20*S)*.5f;
                    DrawGameButton(TEXT("Party slot 1"),TEXT("OG.Characters.Assign.1"),LX,LY,BW,70*S,true);
                    DrawGameButton(TEXT("Party slot 2"),TEXT("OG.Characters.Assign.2"),LX+BW+20*S,LY,BW,70*S,true);
                    LY+=92*S;
                }
                ManifestationsPerPage=FMath::Max(1,FMath::FloorToInt((Bottom-LY-82*S)/(104*S)));
                const int32 LastPage=FMath::Max(0,(Identity.Manifestations.Num()-1)/ManifestationsPerPage);
                ManifestationPage=FMath::Clamp(ManifestationPage,0,LastPage);
                const int32 First=ManifestationPage*ManifestationsPerPage;
                for(int32 I=First;I<Identity.Manifestations.Num() && I<First+ManifestationsPerPage;++I)
                {
                    const auto& Copy=Identity.Manifestations[I];
                    DrawGamePanel(FLinearColor(.06f,.10f,.16f,.96f),LX,LY,LW,88*S);
                    if(I==SelectedManifestationIndex) DrawRect(Gold,LX,LY,4*S,88*S);
                    DrawGameIcon(TEXT("party"),LX+12*S,LY+12*S,56*S,I==SelectedManifestationIndex?Gold:Teal);
                    DrawGameText(Copy.BuildLabel.IsEmpty()?FString::Printf(TEXT("Companion %d"),I+1):Copy.BuildLabel,
                        LX+86*S,LY+10*S,32*S,White,LW-104*S);
                    DrawGameText(FString::Printf(TEXT("Lv %d   %s   %s"),Copy.Level,*Copy.CurrentRarity.ToString(),*Copy.RankId.ToString()),
                        LX+86*S,LY+50*S,26*S,Muted,LW-104*S);
                    AddFoundationHitBox({LX,LY},{LW,88*S},FName(*FString::Printf(TEXT("OG.Characters.Manifestation.%d"),I)),true,122);
                    LY+=104*S;
                }
                if(LastPage>0)
                {
                    DrawGameButton(TEXT("Previous"),TEXT("OG.Characters.Previous"),LX,Bottom-70*S,LW*.45f,64*S);
                    DrawGameButton(TEXT("Next"),TEXT("OG.Characters.Next"),LX+LW*.55f,Bottom-70*S,LW*.45f,64*S);
                }
            }
        }
        else if(RulerDestination==TEXT("gacha"))
        {
            if(!bGachaUnlocked || !bGachaViewReady)
            {
                DrawGameIcon(TEXT("summon"),W*.5f-70*S,Top+AreaH*.2f,140*S,Gold);
                DrawGameText(bGachaUnlocked?TEXT("No summon banner available"):TEXT("Summoning is locked"),
                    M,Top+AreaH*.5f,44*S,White,AreaW);
            }
            else
            {
                int64 Tickets=0;for(const auto& T:GachaView.CompatibleTickets)Tickets+=T.Balance;
                DrawGameText(FString::Printf(TEXT("Currency %lld   Tickets %lld"),(long long)GachaView.CurrencyBalance,(long long)Tickets),
                    M,Top,30*S,Gold,AreaW);
                const float ArtY=Top+60*S,ArtH=FMath::Max(160*S,AreaH-460*S);
                const float ArtW=Portrait?AreaW:AreaW*.58f;
                DrawFoundationCharacterStandIn(M,ArtY,ArtW,ArtH,Teal);
                DrawGameText(TEXT("COMPANION SUMMON"),M+24*S,ArtY+24*S,36*S,White,ArtW-48*S);
                DrawGameText(TEXT("Discover your next companion"),M+24*S,ArtY+ArtH-72*S,32*S,White,ArtW-48*S);
                const float InfoX=Portrait?M:M+ArtW+32*S,InfoW=Portrait?AreaW:AreaW-ArtW-32*S;
                const float InfoY=Portrait?ArtY+ArtH+22*S:ArtY+72*S;
                DrawGameText(FString::Printf(TEXT("Guarantee progress  %d / %d"),GachaView.PityCount,GachaView.HardPity),
                    InfoX,InfoY,30*S,Muted,InfoW);
                DrawRect(FLinearColor(.07f,.12f,.18f,1),InfoX,InfoY+46*S,InfoW,8*S);
                DrawRect(Gold,InfoX,InfoY+46*S,InfoW*FMath::Clamp(GachaView.HardPity>0?
                    static_cast<float>(GachaView.PityCount)/GachaView.HardPity:0.f,0.f,1.f),8*S);
                if(GachaView.bFeaturedGuarantee) DrawGameText(TEXT("Featured guarantee active"),InfoX,InfoY+70*S,28*S,Gold,InfoW);
                const float BWidth=(AreaW-20*S)*.5f;
                DrawGameButton(TEXT("Summon x1"),TEXT("OG.Gacha.Pull"),M,Bottom-96*S,BWidth,88*S,true);
                DrawGameButton(TEXT("Summon x10"),TEXT("OG.Gacha.Pull10"),M+BWidth+20*S,Bottom-96*S,BWidth,88*S,true);
                DrawGameText(FString::Printf(TEXT("%lld currency each / %s"),(long long)GachaView.PullCost,
                    GachaView.bWillUseTicketFirst?TEXT("tickets used first"):TEXT("currency payment")),
                    M,Bottom-142*S,28*S,Muted,AreaW);
                DrawGameButton(TEXT("Rates"),TEXT("OG.Presentation.Rates"),InfoX,Portrait?Bottom-228*S:InfoY+132*S,InfoW*.47f,62*S);
                DrawGameButton(TEXT("History"),TEXT("OG.Presentation.History"),InfoX+InfoW*.53f,Portrait?Bottom-228*S:InfoY+132*S,InfoW*.47f,62*S);
                if(GachaOverlay!=NAME_None)
                {
                    DrawRect(FLinearColor(.012f,.02f,.035f,.98f),M,Top,AreaW,AreaH);
                    AddFoundationHitBox({M,Top},{AreaW,AreaH},TEXT("OG.Presentation.Modal"),true,130);
                    const bool Results=GachaOverlay==TEXT("results"),History=GachaOverlay==TEXT("history");
                    DrawGameText(Results?TEXT("Summon complete"):History?TEXT("Summon history"):TEXT("Banner rates"),M+24*S,Top+18*S,46*S,White,AreaW-48*S);
                    DrawGameButton(TEXT("Close"),TEXT("OG.Presentation.Close"),W-M-190*S,Bottom-70*S,166*S,62*S,false,true,140);
                    if(Results || History)
                    {
                        const int32 N=Results?LastGachaResults.Num():FMath::Min(10,GachaHistory.Num());
                        const int32 Cols=N==1?1:(Portrait?2:5),Rows=FMath::Max(1,FMath::DivideAndRoundUp(N,Cols));
                        const float CW=(AreaW-48*S-18*S*(Cols-1))/Cols,CH=(AreaH-190*S-18*S*(Rows-1))/Rows;
                        for(int32 I=0;I<N;++I)
                        {
                            const auto Id=Results?LastGachaResults[I].IdentityId:GachaHistory[I].IdentityId;
                            const FName Rarity=Results?LastGachaResults[I].Rarity:GachaHistory[I].Rarity;
                            float X=M+24*S+(I%Cols)*(CW+18*S),Y=Top+90*S+(I/Cols)*(CH+18*S);
                            DrawFoundationCharacterStandIn(X,Y,CW,CH,I%2?Gold:Teal);
                            DrawGameText(FoundationIdentityLabel(Id),X+12*S,Y+CH-74*S,30*S,White,CW-24*S);
                            DrawGameText(Rarity.ToString(),X+12*S,Y+CH-36*S,25*S,Gold,CW-24*S);
                        }
                        if(N==0) DrawGameText(TEXT("No summons yet."),M+24*S,Top+116*S,F,Muted,AreaW-48*S);
                    }
                    else
                    {
                        float Y=Top+96*S;
                        const float RowH=88*S;
                        const int32 Count=FMath::Max(1,FMath::FloorToInt((AreaH-190*S)/RowH));
                        GachaDetailsPage=GachaDetailsPage%FMath::Max(1,FMath::DivideAndRoundUp(GachaView.BasePool.Num(),Count));
                        for(int32 I=GachaDetailsPage*Count;I<GachaView.BasePool.Num() && I<(GachaDetailsPage+1)*Count;++I)
                        {
                            const auto& E=GachaView.BasePool[I];
                            DrawGameText(FoundationIdentityLabel(E.IdentityId),M+24*S,Y,32*S,White,AreaW*.60f);
                            DrawGameText(FString::Printf(TEXT("%s  %.2f%% %s"),*E.Rarity.ToString(),E.BaseProbabilityBps/100.,
                                E.bFeatured?TEXT("Featured"):TEXT("")),M+24*S,Y+40*S,28*S,Gold,AreaW-48*S);
                            Y+=RowH;
                        }
                        if(GachaView.BasePool.Num()>Count)
                        {
                            DrawGameButton(TEXT("Next page"),TEXT("OG.Presentation.RatesNext"),M+24*S,Bottom-70*S,240*S,62*S,false,true,140);
                        }
                    }
                }
            }
        }
        else if(RulerDestination==TEXT("territory"))
        {
            DrawGameBackdrop(M,Top,AreaW,AreaH);
            DrawGameText(TEXT("KNOWN HOLDINGS"),M+24*S,Top+24*S,28*S,Gold,AreaW-48*S);
            const int32 Count=Portrait?8:12;
            const int32 PageCount=FMath::Max(1,FMath::DivideAndRoundUp(TerritoryView.Territories.Num(),Count));
            TerritoryPage=TerritoryPage%PageCount;
            const int32 Cols=Portrait?2:4;
            const float CW=(AreaW-48*S)/Cols,CH=FMath::Min(220*S,(AreaH-290*S)/FMath::Max(1,FMath::DivideAndRoundUp(Count,Cols)));
            for(int32 I=TerritoryPage*Count;I<TerritoryView.Territories.Num() && I<(TerritoryPage+1)*Count;++I)
            {
                const auto& T=TerritoryView.Territories[I];
                FVector2D C(M+24*S+CW*((I-TerritoryPage*Count)%Cols+.5f),Top+100*S+CH*((I-TerritoryPage*Count)/Cols+.4f));
                Canvas->K2_DrawPolygon(nullptr,C,{36*S,36*S},6,I==SelectedTerritoryIndex?Gold:Teal);
                DrawGameText(T.bMainTerritory?TEXT("Main territory"):FString::Printf(TEXT("Holding %d"),I+1),
                    C.X-CW*.43f,C.Y+52*S,30*S,White,CW*.86f);
                DrawGameText(T.bContested?TEXT("Contested"):T.ControlState.ToString(),C.X-CW*.43f,C.Y+91*S,26*S,Muted,CW*.86f);
                AddFoundationHitBox({C.X-CW*.45f,C.Y-40*S},{CW*.9f,160*S},FName(*FString::Printf(TEXT("OG.Territory.%d"),I)),true,122);
            }
            if(PageCount>1) DrawGameButton(TEXT("More holdings"),TEXT("OG.Presentation.TerritoryNext"),W-M-260*S,Bottom-166*S,260*S,62*S);
            if(TerritoryView.Territories.IsEmpty())DrawGameText(TEXT("Explore to establish your first holding."),M+24*S,Top+160*S,F,White,AreaW-48*S);
            if(TerritoryView.Territories.IsValidIndex(SelectedTerritoryIndex))
            {
                const auto& T=TerritoryView.Territories[SelectedTerritoryIndex];
                DrawGamePanel(FLinearColor(.03f,.06f,.10f,.98f),M,Bottom-96*S,AreaW,96*S);
                DrawGameText(FString::Printf(TEXT("%s  /  %s"),T.bMainTerritory?TEXT("Main territory"):TEXT("Selected holding"),*T.ControlState.ToString()),
                    M+24*S,Bottom-70*S,32*S,White,AreaW-48*S);
            }
        }
        else
        {
            const TCHAR* Names[]={TEXT("Reports"),TEXT("Chronicle"),TEXT("Codex"),TEXT("Intelligence"),TEXT("Recovery"),TEXT("Content")};
            const TCHAR* Ids[]={TEXT("reports"),TEXT("chronicle"),TEXT("codex"),TEXT("intelligence"),TEXT("recovery"),TEXT("packages")};
            const int32 Cols=Portrait?2:3;
            const float Gap=24*S,CW=(AreaW-(Cols-1)*Gap)/Cols,CH=FMath::Min(260*S,(AreaH-40*S)/(Portrait?3:2)-Gap);
            for(int32 I=0;I<6;++I)
            {
                float X=M+(I%Cols)*(CW+Gap),Y=Top+(I/Cols)*(CH+Gap);
                DrawGamePanel(FLinearColor(.04f,.075f,.12f,.95f),X,Y,CW,CH);
                DrawGameIcon(I==2?TEXT("codex"):TEXT("records"),X+24*S,Y+24*S,64*S,Gold);
                DrawGameText(Names[I],X+24*S,Y+CH-64*S,36*S,White,CW-48*S);
                if(I==0)DrawGameText(FString::Printf(TEXT("%d unread"),UnacknowledgedReports),X+104*S,Y+42*S,28*S,Muted,CW-120*S);
                AddFoundationHitBox({X,Y},{CW,CH},FName(*FString::Printf(TEXT("OG.Records.%s"),Ids[I])),true,122);
            }
        }
        if(!FoundationStatusMessage.IsEmpty() && GachaOverlay==NAME_None)
        {
            DrawGamePanel(FLinearColor(.035f,.055f,.075f,.96f),M,H-NavH-70*S,AreaW,54*S);
            DrawGameText(FoundationStatusMessage,M+16*S,H-NavH-59*S,28*S,Muted,AreaW-32*S);
        }
        return;
    }


    const float RulerReadableHeight = FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f;
    const float TopH = 108.0f * Scale;
    const float BottomH = 74.0f * Scale;
    DrawGamePanel(Panel, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    DrawFoundationText(
        TEXT("JOURNAL"),
        MainText,
        24.0f * Scale,
        18.0f * Scale,
        nullptr,
        0.82f * Scale,
        false);

    AOGWorldPrototypeCharacter* Character =
        PlayerOwner
            ? Cast<AOGWorldPrototypeCharacter>(PlayerOwner->GetPawn())
            : nullptr;
    if (Character && Character->GetSfwPresentation())
    {
        DrawRect(
            FLinearColor(0.12f, 0.18f, 0.22f, 0.94f),
            FMath::Max(152.0f * Scale, RulerReadableHeight * 8.0f),
            14.0f * Scale,
            FMath::Max(126.0f * Scale, RulerReadableHeight * 7.0f),
            FMath::Max(34.0f * Scale, RulerReadableHeight + 20.0f));
        DrawFoundationText(
            TEXT("SFW / Privacy"),
            MainText,
            FMath::Max(152.0f * Scale, RulerReadableHeight * 8.0f) + 12.0f * Scale,
            23.0f * Scale,
            nullptr,
            0.48f * Scale,
            false);
    }

    const float EnterWorldW = FMath::Max(132.0f * Scale, RulerReadableHeight * 7.0f);
    const float EnterWorldX =
        Canvas->ClipX - EnterWorldW - 20.0f * Scale;
    DrawRect(
        Button,
        EnterWorldX,
        58.0f * Scale,
        EnterWorldW,
        FMath::Max(42.0f * Scale, RulerReadableHeight + 20.0f));
    DrawFoundationText(
        TEXT("Enter World"),
        MainText,
        EnterWorldX + 14.0f * Scale,
        69.0f * Scale,
        nullptr,
        0.56f * Scale,
        false);
    AddFoundationHitBox(
        FVector2D(EnterWorldX, 58.0f * Scale),
        FVector2D(EnterWorldW, FMath::Max(42.0f * Scale, RulerReadableHeight + 20.0f)),
        FName(TEXT("OG.Ruler.World")),
        true,
        120);

    const float SettingsW = FMath::Max(112.0f * Scale, RulerReadableHeight * 6.0f);
    const float SettingsX =
        EnterWorldX - SettingsW - 10.0f * Scale;
    DrawRect(
        Button,
        SettingsX,
        58.0f * Scale,
        SettingsW,
        FMath::Max(42.0f * Scale, RulerReadableHeight + 20.0f));
    DrawFoundationText(
        TEXT("Settings"),
        MainText,
        SettingsX + 16.0f * Scale,
        69.0f * Scale,
        nullptr,
        0.56f * Scale,
        false);
    AddFoundationHitBox(
        FVector2D(SettingsX, 58.0f * Scale),
        FVector2D(SettingsW, FMath::Max(42.0f * Scale, RulerReadableHeight + 20.0f)),
        FName(TEXT("OG.Settings.Toggle")),
        true,
        120);

    const float ContentY = TopH + 12.0f * Scale;
    const float ContentH =
        Canvas->ClipY - TopH - BottomH - 24.0f * Scale;
    const float ContentX = 18.0f * Scale;
    const float ContentW = Canvas->ClipX - 36.0f * Scale;
    DrawRect(
        FLinearColor(0.06f, 0.075f, 0.09f, 0.96f),
        ContentX,
        ContentY,
        ContentW,
        ContentH);

    FString Title(TEXT("Home"));
    FString Body = bProjectionReady
        ? TEXT("Your character lobby")
        : TEXT("World projection unavailable");

    if (RulerDestination == FName(TEXT("characters")))
    {
        Title = TEXT("Characters");
        Body = FString::Printf(
            TEXT("%d owned identities / grouped manifestations"),
            RosterIdentityCount);
    }
    else if (RulerDestination == FName(TEXT("gacha")))
    {
        Title = TEXT("Gacha");
        Body = FString::Printf(
            TEXT("Banner / details / history / access: %s"),
            bGachaUnlocked ? TEXT("Unlocked") : TEXT("Locked"));
    }
    else if (RulerDestination == FName(TEXT("territory")))
    {
        Title = TEXT("Territory");
        Body = FString::Printf(
            TEXT("%d claims / select a holding for details"),
            TerritoryClaimCount);
    }
    else if (RulerDestination == FName(TEXT("records")))
    {
        Title = TEXT("Records");
        if (RecordsDestination == FName(TEXT("recovery")))
        {
            Body = TEXT("Recovery / validate before confirmed restore");
        }
        else if (RecordsDestination == FName(TEXT("chronicle")))
        {
            Body = TEXT("Chronicle: canonical historical events and reports.");
        }
        else if (RecordsDestination == FName(TEXT("codex")))
        {
            Body = TEXT("Codex: known identities, locations, systems and discovered information.");
        }
        else if (RecordsDestination == FName(TEXT("intelligence")))
        {
            Body = TEXT("Intelligence / knowledge and confidence");
        }
        else if (RecordsDestination == FName(TEXT("packages")))
        {
            Body = FString::Printf(
                TEXT("Package/storage management. Installed package records visible: %d."),
                PackageStorage.Packages.Num());
        }
        else
        {
            Body = FString::Printf(
                TEXT("%d unread reports / choose a record"),
                UnacknowledgedReports);
        }
    }

    DrawFoundationText(
        Title,
        MainText,
        ContentX + 24.0f * Scale,
        ContentY + 22.0f * Scale,
        nullptr,
        0.90f * Scale,
        false);
    DrawFoundationText(
        Body,
        SubText,
        ContentX + 24.0f * Scale,
        ContentY + 66.0f * Scale,
        nullptr,
        0.58f * Scale,
        false);

    const float DetailY = ContentY + 112.0f * Scale;

    if (RulerDestination == FName(TEXT("home")))
    {
        const int32 LobbyIndex = Roster.IsValidIndex(SelectedRosterIndex)
            ? SelectedRosterIndex : (Roster.IsEmpty() ? INDEX_NONE : 0);
        const float ArtX = ContentX + 48.0f * Scale;
        const float ArtW = ContentW - 96.0f * Scale;
        const float ArtH = FMath::Max(120.0f * Scale, ContentH - 210.0f * Scale);
        DrawFoundationCharacterStandIn(ArtX, DetailY, ArtW, ArtH,
            FLinearColor(0.20f, 0.37f, 0.46f, 0.94f));
        const FString LobbyName = Roster.IsValidIndex(LobbyIndex)
            ? (Roster[LobbyIndex].DisplayNameKey.IsEmpty()
                ? FoundationIdentityLabel(Roster[LobbyIndex].IdentityId)
                : Roster[LobbyIndex].DisplayNameKey)
            : TEXT("Your journey");
        DrawFoundationText(LobbyName.Left(32), MainText, ArtX + 16.0f * Scale,
            DetailY + ArtH - 58.0f * Scale, nullptr, 0.64f * Scale, false);
        DrawFoundationText(TEXT("Temporary character stand-in"), SubText,
            ArtX + 16.0f * Scale, DetailY + ArtH - 30.0f * Scale,
            nullptr, 0.44f * Scale, false);
    }
    else if (RulerDestination == FName(TEXT("characters")))
    {
        if (Roster.IsEmpty())
        {
            DrawFoundationText(
                TEXT("No owned Character Identities yet."),
                MainText,
                ContentX + 24.0f * Scale,
                DetailY,
                nullptr,
                0.62f * Scale,
                false);
        }
        else
        {
            const int32 Columns = 3;
            const float Gap = 10.0f * Scale;
            const float CardW =
                (ContentW - 48.0f * Scale - Gap * (Columns - 1)) /
                Columns;
            const float CardH = 110.0f * Scale;
            const int32 VisibleCount =
                FMath::Min(Roster.Num(), 9);

            if (!Roster.IsValidIndex(SelectedRosterIndex))
            {
                for (int32 Index = 0; Index < VisibleCount; ++Index)
                {
                    const int32 Column = Index % Columns;
                    const int32 Row = Index / Columns;
                    const float X =
                        ContentX + 24.0f * Scale +
                        Column * (CardW + Gap);
                    const float Y =
                        DetailY + Row * (CardH + Gap);
                    const FOGRosterIdentityViewModel& Identity =
                        Roster[Index];
                    const FString Label =
                        Identity.DisplayNameKey.IsEmpty()
                            ? FoundationIdentityLabel(Identity.IdentityId)
                            : Identity.DisplayNameKey;

                    const FName Rarity = Identity.Manifestations.IsEmpty()
                        ? NAME_None : Identity.Manifestations[0].CurrentRarity;
                    // Deterministic rarity frame, always backed by its text label.
                    const uint32 RarityHash = GetTypeHash(Rarity.ToString());
                    const FLinearColor Frame = Rarity.IsNone()
                        ? FLinearColor(0.25f, 0.30f, 0.34f, 1.0f)
                        : FLinearColor(0.30f + (RarityHash % 3) * 0.14f,
                            0.38f + ((RarityHash / 3) % 3) * 0.12f, 0.58f, 1.0f);
                    DrawRect(Frame, X - 2.0f * Scale, Y - 2.0f * Scale,
                        CardW + 4.0f * Scale, CardH + 4.0f * Scale);
                    DrawRect(
                        SelectedRosterIndex == Index
                            ? FLinearColor(0.18f, 0.22f, 0.27f, 0.98f)
                            : Button,
                        X,
                        Y,
                        CardW,
                        CardH);
                    DrawFoundationCharacterStandIn(X + 5.0f * Scale,
                        Y + 5.0f * Scale, CardW - 10.0f * Scale, 58.0f * Scale, Frame);
                    DrawFoundationText(
                        Label.Left(18),
                        MainText,
                        X + 8.0f * Scale,
                        Y + 67.0f * Scale,
                        nullptr,
                        0.48f * Scale,
                        false);
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s / %d owned"),
                            *Rarity.ToString(), Identity.ManifestationCount),
                        SubText,
                        X + 8.0f * Scale,
                        Y + 89.0f * Scale,
                        nullptr,
                        0.38f * Scale,
                        false);
                    AddFoundationHitBox(
                        FVector2D(X, Y),
                        FVector2D(CardW, CardH),
                        FName(*FString::Printf(
                            TEXT("OG.Characters.Identity.%d"),
                            Index)),
                        true,
                        122);
                }

            }
            if (Roster.IsValidIndex(SelectedRosterIndex))
            {
                const FOGRosterIdentityViewModel& Identity =
                    Roster[SelectedRosterIndex];
                const float PortraitH = 170.0f * Scale;
                DrawFoundationCharacterStandIn(ContentX + 24.0f * Scale,
                    DetailY, ContentW * 0.45f, PortraitH,
                    FLinearColor(0.28f, 0.42f, 0.52f, 1.0f));
                const FString IdentityLabel = Identity.DisplayNameKey.IsEmpty()
                    ? FoundationIdentityLabel(Identity.IdentityId) : Identity.DisplayNameKey;
                DrawFoundationText(IdentityLabel.Left(24), MainText,
                    ContentX + ContentW * 0.52f, DetailY + 16.0f * Scale,
                    nullptr, 0.56f * Scale, false);
                DrawFoundationText(TEXT("Character Identity"), SubText,
                    ContentX + ContentW * 0.52f, DetailY + 48.0f * Scale,
                    nullptr, 0.44f * Scale, false);
                const float BackX = ContentX + ContentW * 0.52f;
                const float BackY = DetailY + 108.0f * Scale;
                const float BackW = ContentW * 0.42f;
                DrawGamePanel(Button, BackX, BackY, BackW, 44.0f * Scale);
                DrawFoundationText(TEXT("Back to roster"), MainText,
                    BackX + 8.0f * Scale, BackY + 12.0f * Scale,
                    nullptr, 0.46f * Scale, false);
                AddFoundationHitBox(FVector2D(BackX, BackY),
                    FVector2D(BackW, 44.0f * Scale),
                    FName(TEXT("OG.Characters.Back")), true, 123);
                const float ListY = DetailY + PortraitH + 18.0f * Scale;
                DrawFoundationText(
                    TEXT("Manifestations"),
                    MainText,
                    ContentX + 24.0f * Scale,
                    ListY,
                    nullptr,
                    0.60f * Scale,
                    false);

                if (Identity.Manifestations.IsValidIndex(SelectedManifestationIndex))
                {
                    const FOGRosterManifestationViewModel& Selected =
                        Identity.Manifestations[SelectedManifestationIndex];
                    DrawFoundationText(FString::Printf(TEXT("Selected: %s / Lv %d / %s / %s"),
                        *(Selected.BuildLabel.IsEmpty()
                            ? FString::Printf(TEXT("Copy %d"), SelectedManifestationIndex + 1)
                            : Selected.BuildLabel.Left(18)), Selected.Level,
                        *Selected.CurrentRarity.ToString(), *Selected.RankId.ToString()),
                        SubText, ContentX + 24.0f * Scale, ListY + 28.0f * Scale,
                        nullptr, 0.42f * Scale, false);
                }
                if (Identity.Manifestations.IsValidIndex(SelectedManifestationIndex))
                {
                    const float AssignW = (ContentW - 60 * Scale) * .5f;
                    for (int32 Slot = 1; Slot <= 2; ++Slot)
                    {
                        const float AssignX = ContentX + 24 * Scale + (Slot - 1) * (AssignW + 12 * Scale);
                        DrawGamePanel(Button, AssignX, ListY + 52 * Scale, AssignW, 40 * Scale);
                        DrawFoundationText(FString::Printf(TEXT("Assign companion %d"), Slot), MainText, AssignX + 8 * Scale, ListY + 64 * Scale, nullptr, .42f * Scale);
                        AddFoundationHitBox(FVector2D(AssignX, ListY + 52 * Scale), FVector2D(AssignW, 40 * Scale),
                            FName(*FString::Printf(TEXT("OG.Characters.Assign.%d"), Slot)), true, 124);
                    }
                }
                float Y = ListY + 106.0f * Scale;
                ManifestationsPerPage = FMath::Clamp(FMath::FloorToInt(
                    (ContentY + ContentH - 62.0f * Scale - Y) / (50.0f * Scale)), 1, 5);
                const int32 LastPage = FMath::Max(0, (Identity.Manifestations.Num() - 1) / ManifestationsPerPage);
                ManifestationPage = FMath::Clamp(ManifestationPage, 0, LastPage);
                const int32 FirstCopy = ManifestationPage * ManifestationsPerPage;
                if (LastPage > 0)
                {
                    const float PageY = ContentY + ContentH - 54.0f * Scale;
                    for (int32 Direction : {-1, 1})
                    {
                        const float X = ContentX + 24.0f * Scale +
                            (Direction > 0 ? ContentW - 158.0f * Scale : 0.0f);
                        DrawGamePanel(Button, X, PageY, 110.0f * Scale, 44.0f * Scale);
                        DrawFoundationText(Direction < 0 ? TEXT("Previous") : TEXT("Next"), MainText,
                            X + 8.0f * Scale, PageY + 12.0f * Scale, nullptr, 0.46f * Scale);
                        AddFoundationHitBox(FVector2D(X, PageY), FVector2D(110.0f * Scale, 44.0f * Scale),
                            Direction < 0 ? TEXT("OG.Characters.Previous") : TEXT("OG.Characters.Next"), true, 123);
                    }
                    DrawFoundationText(FString::Printf(TEXT("%d / %d"), ManifestationPage + 1, LastPage + 1),
                        SubText, ContentX + ContentW * 0.43f, PageY + 12.0f * Scale, nullptr, 0.46f * Scale);
                }
                for (int32 Index = FirstCopy;
                     Index < Identity.Manifestations.Num() && Index < FirstCopy + ManifestationsPerPage &&
                         Y + 44.0f * Scale <= ContentY + ContentH - 62.0f * Scale;
                     ++Index)
                {
                    const FOGRosterManifestationViewModel& Manifestation =
                        Identity.Manifestations[Index];
                    const FString Label =
                        Manifestation.BuildLabel.IsEmpty()
                            ? FString::Printf(TEXT("Copy %d"), Index + 1)
                            : Manifestation.BuildLabel.Left(18);
                    DrawRect(
                        SelectedManifestationIndex == Index
                            ? FLinearColor(0.16f, 0.20f, 0.25f, 0.96f)
                            : Button,
                        ContentX + 24.0f * Scale,
                        Y,
                        ContentW - 48.0f * Scale,
                        44.0f * Scale);
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s  |  Lv %d  |  %s  |  %s"),
                            *Label,
                            Manifestation.Level,
                            *Manifestation.CurrentRarity.ToString(),
                            *Manifestation.RankId.ToString()),
                        MainText,
                        ContentX + 36.0f * Scale,
                        Y + 11.0f * Scale,
                        nullptr,
                        0.46f * Scale,
                        false);
                    AddFoundationHitBox(
                        FVector2D(
                            ContentX + 24.0f * Scale,
                            Y),
                        FVector2D(
                            ContentW - 48.0f * Scale,
                            44.0f * Scale),
                        FName(*FString::Printf(
                            TEXT("OG.Characters.Manifestation.%d"),
                            Index)),
                        true,
                        122);
                    Y += 50.0f * Scale;
                }
            }
        }
    }
    else if (RulerDestination == FName(TEXT("gacha")))
    {
        if (!bGachaUnlocked)
        {
            DrawFoundationText(
                TEXT("Gacha is canonically locked. No pull controls are exposed."),
                MainText,
                ContentX + 24.0f * Scale,
                DetailY,
                nullptr,
                0.60f * Scale,
                false);
        }
        else if (!bGachaViewReady)
        {
            DrawFoundationText(
                TEXT("Gacha is unlocked, but no valid installed banner is active."),
                MainText,
                ContentX + 24.0f * Scale,
                DetailY,
                nullptr,
                0.56f * Scale,
                false);
            DrawFoundationText(
                TEXT("A content package may provide the banner without changing this Foundation UI."),
                SubText,
                ContentX + 24.0f * Scale,
                DetailY + 30.0f * Scale,
                nullptr,
                0.46f * Scale,
                false);
        }
        else
        {
            const float GachaRowStep = FMath::Max(28.0f * Scale, RulerReadableHeight + 8.0f);
            const bool bSideResults = Canvas->ClipX > Canvas->ClipY;
            const float GachaDetailW = bSideResults ? ContentW * 0.50f : ContentW;
            DrawFoundationText(
                FString::Printf(
                    TEXT("Banner: %s"),
                    *GachaView.BannerId.ToString()),
                MainText,
                ContentX + 24.0f * Scale,
                DetailY,
                nullptr,
                0.58f * Scale,
                false);
            DrawFoundationText(
                FString::Printf(
                    TEXT("Balance %lld | Each pull %lld"),
                    static_cast<long long>(GachaView.CurrencyBalance),
                    static_cast<long long>(GachaView.PullCost)),
                SubText,
                ContentX + 24.0f * Scale,
                DetailY + GachaRowStep,
                nullptr,
                0.48f * Scale,
                false);

            DrawFoundationText(FString::Printf(TEXT("Pity %d / %d | Guarantee %s"),
                GachaView.PityCount, GachaView.HardPity,
                GachaView.bFeaturedGuarantee ? TEXT("Yes") : TEXT("No")),
                SubText, ContentX + 24.0f * Scale, DetailY + 2.0f * GachaRowStep,
                nullptr, 0.48f * Scale);
            float TicketY = DetailY + 3.0f * GachaRowStep;
            for (const FOGGachaTicketViewModel& Ticket :
                 GachaView.CompatibleTickets)
            {
                DrawFoundationText(
                    FString::Printf(
                        TEXT("Ticket %s: %lld%s"),
                        *Ticket.TicketId.ToString(),
                        static_cast<long long>(Ticket.Balance),
                        Ticket.bWillConsumeBeforeCurrency
                            ? TEXT("  (next)")
                            : TEXT("")),
                    SubText,
                    ContentX + 36.0f * Scale,
                    TicketY,
                    nullptr,
                    0.42f * Scale,
                    false);
                TicketY += GachaRowStep;
            }

            const float PullW = (GachaDetailW - 60.0f * Scale) * 0.5f;
            const float PullH = FMath::Max(44.0f * Scale, RulerReadableHeight + 20.0f);
            const float PullX = ContentX + 24.0f * Scale;
            const float PullY = FMath::Max(DetailY + 58.0f * Scale, TicketY);
            DrawRect(
                Button,
                PullX,
                PullY,
                PullW,
                PullH);
            DrawFoundationText(
                GachaView.bWillUseTicketFirst
                    ? TEXT("Pull x1 (ticket)")
                    : TEXT("Pull x1"),
                MainText,
                PullX + 14.0f * Scale,
                PullY + 12.0f * Scale,
                nullptr,
                0.52f * Scale,
                false);
            AddFoundationHitBox(
                FVector2D(PullX, PullY),
                FVector2D(PullW, PullH),
                FName(TEXT("OG.Gacha.Pull")),
                true,
                124);
            const float TenX = PullX + PullW + 12.0f * Scale;
            DrawGamePanel(Button, TenX, PullY, PullW, PullH);
            DrawFoundationText(TEXT("Pull x10"), MainText, TenX + 14.0f * Scale,
                PullY + 12.0f * Scale, nullptr, 0.52f * Scale);
            AddFoundationHitBox(FVector2D(TenX, PullY), FVector2D(PullW, PullH),
                TEXT("OG.Gacha.Pull10"), true, 124);

            DrawFoundationText(
                TEXT("Base pool details"),
                MainText,
                ContentX + 24.0f * Scale,
                PullY + PullH + 16.0f * Scale,
                nullptr,
                0.54f * Scale,
                false);
            float PoolY = PullY + PullH + 16.0f * Scale + GachaRowStep;
            for (int32 Index = 0;
                 Index < GachaView.BasePool.Num() && Index < 5;
                 ++Index)
            {
                const FOGGachaProbabilityViewModel& Entry =
                    GachaView.BasePool[Index];
                DrawFoundationText(
                    FString::Printf(
                        TEXT("%s  |  %s  |  %.2f%%%s"),
                        *Entry.IdentityId.ToString(),
                        *Entry.Rarity.ToString(),
                        static_cast<double>(Entry.BaseProbabilityBps) / 100.0,
                        Entry.bFeatured
                            ? TEXT("  Featured")
                            : TEXT("")),
                    SubText,
                    ContentX + 36.0f * Scale,
                    PoolY,
                    nullptr,
                    0.42f * Scale,
                    false);
                PoolY += GachaRowStep;
            }

            const float HistoryX = ContentX + (bSideResults ? ContentW * 0.54f : 24.0f * Scale);
            float HistoryY = bSideResults ? DetailY : PoolY + 18.0f * Scale;
            DrawFoundationText(
                LastGachaResults.IsEmpty() ? TEXT("Recent history") : TEXT("Latest summon results"),
                MainText,
                HistoryX,
                HistoryY,
                nullptr,
                0.54f * Scale,
                false);
            HistoryY += FMath::Max(28.0f * Scale, RulerReadableHeight + 12.0f);
            const int32 ResultCount = LastGachaResults.IsEmpty() ? FMath::Min(10, GachaHistory.Num()) : LastGachaResults.Num();
            for (int32 Index = 0; Index < ResultCount; ++Index)
            {
                const FOGContentId Identity = LastGachaResults.IsEmpty()
                    ? GachaHistory[Index].IdentityId : LastGachaResults[Index].IdentityId;
                const FName Rarity = LastGachaResults.IsEmpty()
                    ? GachaHistory[Index].Rarity : LastGachaResults[Index].Rarity;
                const float ResultWidth = bSideResults ? ContentW * 0.42f : ContentW - 60.0f * Scale;
                const float CellW = (ResultWidth - 12.0f * Scale) * 0.5f;
                const float CellH = FMath::Max(42.0f * Scale, RulerReadableHeight + 20.0f);
                const float X = HistoryX + (Index % 2) * (CellW + 12.0f * Scale);
                const float Y = HistoryY + (Index / 2) * (CellH + 8.0f * Scale);
                DrawGamePanel(Button, X, Y, CellW, CellH);
                DrawFoundationText(FString::Printf(TEXT("%s | %s"),
                    *FoundationIdentityLabel(Identity), *Rarity.ToString()), MainText,
                    X + 8.0f * Scale, Y + 10.0f * Scale, nullptr, 0.46f * Scale);
            }
        }
    }
    else if (RulerDestination == FName(TEXT("territory")))
    {
        DrawFoundationText(TEXT("Claim schematic / known holdings"), SubText,
            ContentX + 24.0f * Scale, DetailY,
            nullptr, 0.46f * Scale, false);

        const float MapY = DetailY + 36.0f * Scale;
        const float MapH =
            FMath::Max(120.0f * Scale, ContentH - 290.0f * Scale);
        const float MapX = ContentX + 24.0f * Scale;
        const float MapW = ContentW - 48.0f * Scale;
        DrawRect(
            FLinearColor(0.035f, 0.055f, 0.065f, 0.95f),
            ContentX + 24.0f * Scale,
            MapY,
            ContentW - 48.0f * Scale,
            MapH);

        for (int32 Grid = 1; Grid < 5; ++Grid)
        {
            const FLinearColor GridColor(0.10f, 0.20f, 0.24f, 0.50f);
            DrawLine(MapX + MapW * Grid / 5.0f, MapY,
                MapX + MapW * Grid / 5.0f, MapY + MapH, GridColor, 1.0f);
            DrawLine(MapX, MapY + MapH * Grid / 5.0f,
                MapX + MapW, MapY + MapH * Grid / 5.0f, GridColor, 1.0f);
        }

        if (TerritoryView.Territories.IsEmpty())
        {
            DrawFoundationText(
                TEXT("No territory claims yet."),
                MainText,
                ContentX + 42.0f * Scale,
                MapY + 22.0f * Scale,
                nullptr,
                0.56f * Scale,
                false);
        }
        else
        {
            const float NodeW = 180.0f * Scale;
            const float NodeH = 62.0f * Scale;
            const int32 Columns =
                FMath::Max(
                    1,
                    FMath::FloorToInt(
                        (ContentW - 70.0f * Scale) /
                        (NodeW + 12.0f * Scale)));
            for (int32 Index = 0;
                 Index < TerritoryView.Territories.Num() && Index < 12 &&
                     22.0f * Scale + (Index / Columns + 1) * (NodeH + 14.0f * Scale) <= MapH;
                 ++Index)
            {
                const int32 Column = Index % Columns;
                const int32 Row = Index / Columns;
                const float X =
                    ContentX + 42.0f * Scale +
                    Column * (NodeW + 12.0f * Scale);
                const float Y =
                    MapY + 22.0f * Scale +
                    Row * (NodeH + 14.0f * Scale);
                const FOGTerritorySummaryViewModel& Territory =
                    TerritoryView.Territories[Index];

                DrawRect(
                    SelectedTerritoryIndex == Index
                        ? FLinearColor(0.18f, 0.22f, 0.27f, 0.98f)
                        : Button,
                    X,
                    Y,
                    NodeW,
                    NodeH);
                DrawFoundationText(
                    Territory.bMainTerritory
                        ? TEXT("Main Territory")
                        : TEXT("Territory"),
                    MainText,
                    X + 10.0f * Scale,
                    Y + 8.0f * Scale,
                    nullptr,
                    0.48f * Scale,
                    false);
                DrawFoundationText(
                    FString::Printf(
                        TEXT("%s%s"),
                        *Territory.ControlState.ToString(),
                        Territory.bContested
                            ? TEXT(" / contested")
                            : TEXT("")),
                    SubText,
                    X + 10.0f * Scale,
                    Y + 34.0f * Scale,
                    nullptr,
                    0.42f * Scale,
                    false);
                AddFoundationHitBox(
                    FVector2D(X, Y),
                    FVector2D(NodeW, NodeH),
                    FName(*FString::Printf(
                        TEXT("OG.Territory.%d"),
                        Index)),
                    true,
                    122);
            }
        }
    }

    if (RulerDestination == FName(TEXT("territory")) &&
        TerritoryView.Territories.IsValidIndex(SelectedTerritoryIndex))
    {
        const FOGTerritorySummaryViewModel& Selected =
            TerritoryView.Territories[SelectedTerritoryIndex];
        const float SheetY = ContentY + ContentH - 94.0f * Scale;
        DrawGamePanel(Button, ContentX + 24.0f * Scale, SheetY,
            ContentW - 48.0f * Scale, 78.0f * Scale);
        DrawFoundationText(FString::Printf(TEXT("%s / %s%s"),
            Selected.bMainTerritory ? TEXT("Main Territory") : TEXT("Holding"),
            *Selected.ControlState.ToString(),
            Selected.bContested ? TEXT(" / contested") : TEXT("")),
            MainText, ContentX + 36.0f * Scale, SheetY + 10.0f * Scale,
            nullptr, 0.48f * Scale, false);
        FString Sections;
        for (const FName& Section : TerritoryView.QuickSections)
        {
            if (!Sections.IsEmpty()) { Sections += TEXT(" / "); }
            Sections += Section.ToString();
        }
        DrawFoundationText(Sections.Left(68), SubText,
            ContentX + 36.0f * Scale, SheetY + 35.0f * Scale,
            nullptr, 0.38f * Scale, false);
    }

    if (RulerDestination == FName(TEXT("records")))
    {
        const TCHAR* Names[] =
        {
            TEXT("Hub"),
            TEXT("Reports"),
            TEXT("Chronicle"),
            TEXT("Codex"),
            TEXT("Intelligence"),
            TEXT("Recovery"),
            TEXT("Packages")
        };
        const TCHAR* Ids[] =
        {
            TEXT("hub"),
            TEXT("reports"),
            TEXT("chronicle"),
            TEXT("codex"),
            TEXT("intelligence"),
            TEXT("recovery"),
            TEXT("packages")
        };
        const bool bRecordsHub = RecordsDestination == FName(TEXT("hub"));
        const float RecordGap = 10.0f * Scale;
        const int32 RecordColumns = bRecordsHub ? 2 : 3;
        const float W = (ContentW - 48.0f * Scale -
            RecordGap * (RecordColumns - 1)) / RecordColumns;
        const float H = (bRecordsHub ? 70.0f : 36.0f) * Scale;
        const float NavigationY = DetailY;
        for (int32 Index = 0; Index < 7; ++Index)
        {
            if (bRecordsHub && Index == 0) { continue; }
            const int32 Slot = bRecordsHub ? Index - 1 : Index;
            const float X = ContentX + 24.0f * Scale +
                (Slot % RecordColumns) * (W + RecordGap);
            const float RecordY = NavigationY +
                (Slot / RecordColumns) * (H + RecordGap);
            DrawRect(RecordsDestination == FName(Ids[Index])
                ? FLinearColor(0.18f, 0.27f, 0.33f, 0.98f) : Button,
                X, RecordY, W, H);
            DrawFoundationText(
                Names[Index],
                MainText,
                X + 14.0f * Scale,
                RecordY + 11.0f * Scale,
                nullptr,
                0.56f * Scale,
                false);
            AddFoundationHitBox(
                FVector2D(X, RecordY),
                FVector2D(W, H),
                FName(*FString::Printf(
                    TEXT("OG.Records.%s"),
                    Ids[Index])),
                true,
                121);
        }
        float RecordY = NavigationY + 3.0f * (H + RecordGap) + 12.0f * Scale;

        if (RecordsDestination == FName(TEXT("reports")))
        {
            if (Reports.IsEmpty())
            {
                DrawFoundationText(
                    TEXT("No reports yet."),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 8.0f * Scale,
                    nullptr,
                    0.50f * Scale,
                    false);
            }
            else
            {
                for (int32 Index = 0;
                     Index < Reports.Num() && Index < 6 &&
                         RecordY + 46.0f * Scale <= ContentY + ContentH - 8.0f * Scale;
                     ++Index)
                {
                    const FOGReportRecord& Report = Reports[Index];
                    const float RowH = 46.0f * Scale;
                    DrawRect(
                        Report.bAcknowledged
                            ? FLinearColor(0.07f, 0.08f, 0.09f, 0.82f)
                            : Button,
                        ContentX + 24.0f * Scale,
                        RecordY,
                        ContentW - 48.0f * Scale,
                        RowH);
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s / P%d / T%lld / %s"),
                            *Report.Category.ToString(),
                            Report.Priority,
                            static_cast<long long>(Report.CreatedWorldTick),
                            Report.bAcknowledged
                                ? TEXT("acknowledged")
                                : TEXT("unread")),
                        MainText,
                        ContentX + 36.0f * Scale,
                        RecordY + 12.0f * Scale,
                        nullptr,
                        0.45f * Scale,
                        false);
                    if (!Report.bAcknowledged)
                    {
                        AddFoundationHitBox(
                            FVector2D(
                                ContentX + 24.0f * Scale,
                                RecordY),
                            FVector2D(
                                ContentW - 48.0f * Scale,
                                RowH),
                            FName(*FString::Printf(
                                TEXT("OG.Reports.Acknowledge.%d"),
                                Index)),
                            true,
                            124);
                    }
                    RecordY += 52.0f * Scale;
                }
            }
        }
        else if (RecordsDestination == FName(TEXT("chronicle")))
        {
            if (ChronicleEntries.IsEmpty())
            {
                DrawFoundationText(
                    TEXT("No chronicle entries yet."),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 8.0f * Scale,
                    nullptr,
                    0.50f * Scale,
                    false);
            }
            else
            {
                for (int32 Index = 0;
                     Index < ChronicleEntries.Num() && Index < 8 &&
                         RecordY + 32.0f * Scale <= ContentY + ContentH - 8.0f * Scale;
                     ++Index)
                {
                    const FOGChronicleEntryViewModel& Entry =
                        ChronicleEntries[Index];
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s  |  tick %lld  |  %s"),
                            *Entry.EventType.ToString(),
                            static_cast<long long>(Entry.WorldTick),
                            *Entry.Importance.ToString()),
                        MainText,
                        ContentX + 36.0f * Scale,
                        RecordY + 8.0f * Scale,
                        nullptr,
                        0.46f * Scale,
                        false);
                    RecordY += 32.0f * Scale;
                }
            }
        }
        else if (RecordsDestination == FName(TEXT("codex")))
        {
            DrawFoundationText(
                TEXT("Known fact index"),
                MainText,
                ContentX + 24.0f * Scale,
                RecordY + 4.0f * Scale,
                nullptr,
                0.54f * Scale,
                false);
            RecordY += 32.0f * Scale;
            if (CodexView.VisibleEntries.IsEmpty())
            {
                DrawFoundationText(
                    TEXT("No discovered codex knowledge yet."),
                    SubText,
                    ContentX + 36.0f * Scale,
                    RecordY,
                    nullptr,
                    0.48f * Scale,
                    false);
            }
            else
            {
                for (int32 Index = 0;
                     Index < CodexView.VisibleEntries.Num() && Index < 8 &&
                         RecordY + 30.0f * Scale <= ContentY + ContentH - 8.0f * Scale;
                     ++Index)
                {
                    const FOGCodexEntryViewModel& Entry =
                        CodexView.VisibleEntries[Index];
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s  |  %s"),
                            *Entry.DisplayNameKey,
                            *UEnum::GetValueAsString(
                                Entry.KnowledgeState)),
                        MainText,
                        ContentX + 36.0f * Scale,
                        RecordY,
                        nullptr,
                        0.46f * Scale,
                        false);
                    RecordY += 30.0f * Scale;
                }
            }
        }
        else if (RecordsDestination == FName(TEXT("intelligence")))
        {
            auto KnowledgeLabel =
                [](EOGUiKnowledgeState State)
                {
                    switch (State)
                    {
                    case EOGUiKnowledgeState::Confirmed: return TEXT("confirmed");
                    case EOGUiKnowledgeState::Estimated: return TEXT("estimated");
                    case EOGUiKnowledgeState::Rumor: return TEXT("rumor");
                    case EOGUiKnowledgeState::Contradicted: return TEXT("contradicted");
                    case EOGUiKnowledgeState::Outdated: return TEXT("outdated");
                    default: return TEXT("unknown");
                    }
                };

            if (IntelligenceEntries.IsEmpty())
            {
                DrawFoundationText(
                    TEXT("No intelligence facts yet."),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 8.0f * Scale,
                    nullptr,
                    0.50f * Scale,
                    false);
            }
            else
            {
                for (int32 Index = 0;
                     Index < IntelligenceEntries.Num() && Index < 8 &&
                         RecordY + 32.0f * Scale <= ContentY + ContentH - 8.0f * Scale;
                     ++Index)
                {
                    const FOGIntelligenceEntryViewModel& Entry =
                        IntelligenceEntries[Index];
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s  |  %s  |  confidence %d%%"),
                            *Entry.FactKey.ToString(),
                            KnowledgeLabel(Entry.Knowledge.KnowledgeState),
                            Entry.Knowledge.ConfidenceBps / 100),
                        MainText,
                        ContentX + 36.0f * Scale,
                        RecordY + 8.0f * Scale,
                        nullptr,
                        0.45f * Scale,
                        false);
                    RecordY += 32.0f * Scale;
                }
            }
        }
        else if (RecordsDestination == FName(TEXT("recovery")))
        {
            const FString Base =
                FPaths::Combine(
                    FPaths::ProjectSavedDir(),
                    TEXT("OfflineGame"));
            const FString ImportedBackupPath =
                FPaths::Combine(
                    Base,
                    TEXT("Import"),
                    TEXT("selected_import.db"));

            FString TransferDetail;
            const FName TransferState =
                FOGAndroidBackupDocumentBridge::GetTransferState(
                    TransferDetail);

            auto RecoveryAction =
                [&](const FString& Label,
                    FName Name,
                    bool bEnabled)
                {
                    const float ActionWidth = ContentW - 48.0f * Scale;
                    const float H = 40.0f * Scale;
                    const float X = ContentX + 24.0f * Scale;
                    DrawRect(
                        bEnabled
                            ? Button
                            : FLinearColor(0.05f, 0.05f, 0.05f, 0.62f),
                        X,
                        RecordY,
                        ActionWidth,
                        H);
                    DrawFoundationText(
                        Label,
                        bEnabled ? MainText : SubText,
                        X + 14.0f * Scale,
                        RecordY + 10.0f * Scale,
                        nullptr,
                        0.50f * Scale,
                        false);
                    if (bEnabled)
                    {
                        AddFoundationHitBox(
                            FVector2D(X, RecordY),
                            FVector2D(ActionWidth, H),
                            Name,
                            true,
                            123);
                    }
                    RecordY += 48.0f * Scale;
                };

            RecoveryAction(
                TEXT("Create local recovery snapshot"),
                FName(TEXT("OG.Recovery.CreateBackup")),
                true);
            RecoveryAction(
                TEXT("Export current world to Files..."),
                FName(TEXT("OG.Recovery.ExportCurrent")),
                true);
            RecoveryAction(
                TEXT("Import backup from Files..."),
                FName(TEXT("OG.Recovery.PickImport")),
                true);
            RecoveryAction(
                bPendingImportedBackupRestore
                    ? TEXT("Confirm restore selected import")
                    : TEXT("Validate and restore selected import"),
                FName(TEXT("OG.Recovery.RestorePickedImport")),
                TransferState == FName(TEXT("imported")) &&
                    IFileManager::Get().FileExists(
                        *ImportedBackupPath));

            DrawFoundationText(
                FString::Printf(
                    TEXT("Backups: %d"),
                    BackupManager.Backups.Num()),
                MainText,
                ContentX + 24.0f * Scale,
                RecordY + 6.0f * Scale,
                nullptr,
                0.52f * Scale,
                false);
            RecordY += 34.0f * Scale;

            const int32 VisibleBackups =
                FMath::Min(BackupManager.Backups.Num(), 4);
            for (int32 Index = 0;
                 Index < VisibleBackups &&
                     RecordY + 72.0f * Scale <= ContentY + ContentH - 8.0f * Scale;
                 ++Index)
            {
                const FOGBackupEntryViewModel& Backup =
                    BackupManager.Backups[Index];
                const float RowH = 72.0f * Scale;
                const float X = ContentX + 24.0f * Scale;
                const float BackupRowWidth = ContentW - 48.0f * Scale;
                DrawRect(
                    FLinearColor(0.07f, 0.085f, 0.10f, 0.94f),
                    X,
                    RecordY,
                    BackupRowWidth,
                    RowH);
                DrawFoundationText(
                    FString::Printf(
                        TEXT("%s  |  schema %d  |  %s  |  %s"),
                        *Backup.SnapshotKind.ToString(),
                        Backup.SchemaVersion,
                        *Backup.ValidationState.ToString(),
                        *Backup.CreatedUtc),
                    MainText,
                    X + 10.0f * Scale,
                    RecordY + 8.0f * Scale,
                    nullptr,
                    0.43f * Scale,
                    false);

                const float ActionW = 104.0f * Scale;
                const float ActionH = 30.0f * Scale;
                const float ActionY = RecordY + 36.0f * Scale;
                const float ExportX = X + BackupRowWidth - 2.0f * ActionW - 18.0f * Scale;
                const float RestoreX = X + BackupRowWidth - ActionW - 10.0f * Scale;

                DrawGamePanel(Button, ExportX, ActionY, ActionW, ActionH);
                DrawFoundationText(
                    TEXT("Export"),
                    MainText,
                    ExportX + 18.0f * Scale,
                    ActionY + 7.0f * Scale,
                    nullptr,
                    0.43f * Scale,
                    false);
                AddFoundationHitBox(
                    FVector2D(ExportX, ActionY),
                    FVector2D(ActionW, ActionH),
                    FName(*FString::Printf(
                        TEXT("OG.Recovery.Export.%d"),
                        Index)),
                    true,
                    124);

                DrawRect(
                    PendingBackupRestoreIndex == Index
                        ? FLinearColor(0.30f, 0.10f, 0.10f, 0.98f)
                        : Button,
                    RestoreX,
                    ActionY,
                    ActionW,
                    ActionH);
                DrawFoundationText(
                    PendingBackupRestoreIndex == Index
                        ? TEXT("Confirm")
                        : TEXT("Restore"),
                    MainText,
                    RestoreX + 14.0f * Scale,
                    ActionY + 7.0f * Scale,
                    nullptr,
                    0.43f * Scale,
                    false);
                AddFoundationHitBox(
                    FVector2D(RestoreX, ActionY),
                    FVector2D(ActionW, ActionH),
                    FName(*FString::Printf(
                        TEXT("OG.Recovery.Restore.%d"),
                        Index)),
                    true,
                    124);

                RecordY += RowH + 8.0f * Scale;
            }

            if (!FoundationStatusMessage.IsEmpty())
            {
                DrawFoundationText(
                    FoundationStatusMessage,
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 4.0f * Scale,
                    nullptr,
                    0.46f * Scale,
                    false);
            }
            else if (TransferState == FName(TEXT("pending_export")) ||
                     TransferState == FName(TEXT("pending_import")))
            {
                DrawFoundationText(
                    TEXT("Waiting for Android document picker..."),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 4.0f * Scale,
                    nullptr,
                    0.46f * Scale,
                    false);
            }
            else if (TransferState == FName(TEXT("error")))
            {
                DrawFoundationText(
                    FString::Printf(
                        TEXT("Document transfer failed: %s"),
                        *TransferDetail),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 4.0f * Scale,
                    nullptr,
                    0.46f * Scale,
                    false);
            }
        }
        else if (RecordsDestination == FName(TEXT("packages")))
        {
            int32 Visible = 0;
            for (const FOGPackageStorageEntryViewModel& Entry :
                 PackageStorage.Packages)
            {
                if (Visible >= 5)
                {
                    break;
                }

                FString SizeText(TEXT("size unknown"));
                if (Entry.bSizeKnown)
                {
                    SizeText = FString::Printf(
                        TEXT("%.1f MB"),
                        static_cast<double>(Entry.SizeBytes) /
                            (1024.0 * 1024.0));
                }

                DrawFoundationText(
                    FString::Printf(
                        TEXT("%s  v%d  |  %s  |  %s  |  update %s  |  %s"),
                        *Entry.PackageId.ToString(),
                        Entry.Version,
                        *Entry.StorageClass.ToString(),
                        *Entry.DownloadState.ToString(),
                        *Entry.UpdateState.ToString(),
                        *SizeText),
                    MainText,
                    ContentX + 24.0f * Scale,
                    RecordY + 6.0f * Scale,
                    nullptr,
                    0.44f * Scale,
                    false);
                DrawFoundationText(
                    Entry.InstallUri.IsEmpty()
                        ? TEXT("Location unavailable")
                        : FString::Printf(
                            TEXT("Location: %s"),
                            *Entry.InstallUri),
                    SubText,
                    ContentX + 36.0f * Scale,
                    RecordY + 27.0f * Scale,
                    nullptr,
                    0.40f * Scale,
                    false);
                const bool bCanMove =
                    Entry.Actions.Contains(FName(TEXT("move")));
                const bool bCanArchive =
                    Entry.Actions.Contains(FName(TEXT("archive")));

                if (bCanMove || bCanArchive)
                {
                    const float ActionY =
                        RecordY + 46.0f * Scale;
                    const float ActionW =
                        96.0f * Scale;
                    const float ActionH =
                        28.0f * Scale;
                    float ActionX =
                        ContentX + 36.0f * Scale;

                    if (bCanMove)
                    {
                        DrawRect(
                            Button,
                            ActionX,
                            ActionY,
                            ActionW,
                            ActionH);
                        DrawFoundationText(
                            Entry.StorageClass == FName(TEXT("managed_external"))
                                ? TEXT("Move local")
                                : TEXT("Move"),
                            MainText,
                            ActionX + 12.0f * Scale,
                            ActionY + 6.0f * Scale,
                            nullptr,
                            0.40f * Scale,
                            false);
                        AddFoundationHitBox(
                            FVector2D(ActionX, ActionY),
                            FVector2D(ActionW, ActionH),
                            FName(*FString::Printf(
                                TEXT("OG.Package.Move.%d"),
                                Visible)),
                            true,
                            124);
                        ActionX += ActionW + 8.0f * Scale;
                    }

                    if (bCanArchive)
                    {
                        DrawRect(
                            Button,
                            ActionX,
                            ActionY,
                            ActionW,
                            ActionH);
                        DrawFoundationText(
                            TEXT("Archive..."),
                            MainText,
                            ActionX + 12.0f * Scale,
                            ActionY + 6.0f * Scale,
                            nullptr,
                            0.40f * Scale,
                            false);
                        AddFoundationHitBox(
                            FVector2D(ActionX, ActionY),
                            FVector2D(ActionW, ActionH),
                            FName(*FString::Printf(
                                TEXT("OG.Package.Archive.%d"),
                                Visible)),
                            true,
                            124);
                    }
                }
                else
                {
                    DrawFoundationText(
                        TEXT("Move/archive unavailable while active or unsupported."),
                        SubText,
                        ContentX + 36.0f * Scale,
                        RecordY + 46.0f * Scale,
                        nullptr,
                        0.38f * Scale,
                        false);
                }

                RecordY += 82.0f * Scale;
                ++Visible;
            }

            if (PackageStorage.Packages.IsEmpty())
            {
                DrawFoundationText(
                    TEXT("No content packages are currently installed."),
                    SubText,
                    ContentX + 24.0f * Scale,
                    RecordY + 8.0f * Scale,
                    nullptr,
                    0.50f * Scale,
                    false);
            }
        }
    }

    if (!FoundationStatusMessage.IsEmpty())
    {
        DrawRect(
            FLinearColor(0.04f, 0.05f, 0.06f, 0.90f),
            18.0f * Scale,
            Canvas->ClipY - BottomH - 38.0f * Scale,
            Canvas->ClipX - 36.0f * Scale,
            30.0f * Scale);
        DrawFoundationText(
            FoundationStatusMessage,
            SubText,
            28.0f * Scale,
            Canvas->ClipY - BottomH - 31.0f * Scale,
            nullptr,
            0.44f * Scale,
            false);
    }

    const TCHAR* Labels[] =
    {
        TEXT("Home"),
        TEXT("Characters"),
        TEXT("Gacha"),
        TEXT("Territory"),
        TEXT("Records")
    };
    const TCHAR* Destinations[] =
    {
        TEXT("home"),
        TEXT("characters"),
        TEXT("gacha"),
        TEXT("territory"),
        TEXT("records")
    };

    const float NavW = Canvas->ClipX / 5.0f;
    const float NavY = Canvas->ClipY - BottomH;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const float X = Index * NavW;
        DrawRect(
            RulerDestination == FName(Destinations[Index])
                ? FLinearColor(0.18f, 0.22f, 0.27f, 0.98f)
                : Button,
            X,
            NavY,
            NavW,
            BottomH);
        DrawFoundationText(
            Labels[Index],
            MainText,
            X + 10.0f * Scale,
            NavY + 24.0f * Scale,
            nullptr,
            0.48f * Scale,
            false);
        AddFoundationHitBox(
            FVector2D(X, NavY),
            FVector2D(NavW, BottomH),
            FName(*FString::Printf(
                TEXT("OG.Ruler.%s"),
                Destinations[Index])),
            true,
            119);
    }
}

void AOGWorldPresentationHud::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas)
    {
        return;
    }

    AOGWorldPrototypeCharacter* Character =
        PlayerOwner
            ? Cast<AOGWorldPrototypeCharacter>(PlayerOwner->GetPawn())
            : nullptr;
    if (Character) Character->RefreshFoundationInputState();
    const bool bSettingsOpen =
        Character && Character->IsFoundationSettingsOpen();
    const bool bPauseOpen =
        Character && Character->IsFoundationPauseMenuOpen();

    float Scale =
        FMath::Clamp(Canvas->ClipY / 1080.0f, 0.72f, 1.2f);
    FLinearColor MainText(0.94f, 0.94f, 0.89f, 1.0f);
    FLinearColor SubText(0.63f, 0.73f, 0.80f, 1.0f);
    FLinearColor Panel(0.025f, 0.045f, 0.075f, 0.97f);
    FLinearColor Button(0.065f, 0.11f, 0.17f, 0.97f);

    if (Character)
    {
        if (Character->GetUiReadabilityProfile() ==
            FName(TEXT("large_text")))
        {
            Scale = FMath::Min(Scale * 1.12f, 1.32f);
        }
        else if (Character->GetUiReadabilityProfile() ==
                 FName(TEXT("high_contrast")))
        {
            MainText = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
            SubText = FLinearColor(0.86f, 0.88f, 0.90f, 1.0f);
            Panel = FLinearColor(0.005f, 0.008f, 0.012f, 0.985f);
            Button = FLinearColor(0.22f, 0.24f, 0.28f, 1.0f);
        }

        const FName Vision = Character->GetColorVisionProfile();
        if (Vision == FName(TEXT("deuteranopia")))
        {
            Button = FLinearColor(0.12f, 0.22f, 0.32f, Button.A);
        }
        else if (Vision == FName(TEXT("protanopia")))
        {
            Button = FLinearColor(0.14f, 0.22f, 0.30f, Button.A);
        }
        else if (Vision == FName(TEXT("tritanopia")))
        {
            Button = FLinearColor(0.24f, 0.16f, 0.22f, Button.A);
        }
    }

    if (!bSettingsOpen && DrawFoundationDiagnosticSurface(Character, Scale, true)) return;
    if (!bSettingsOpen && FoundationSurface == TEXT("world") && DrawCanonicalTurnSurface(Character, Scale)) return;

    if (!bSettingsOpen &&
        FoundationSurface == FName(TEXT("opening")))
    {
        DrawFoundationOpening(
            Scale,
            MainText,
            SubText,
            Panel,
            Button);
        return;
    }

    if (!bSettingsOpen &&
        FoundationSurface == FName(TEXT("ruler")))
    {
        DrawFoundationRuler(
            Scale,
            MainText,
            SubText,
            Panel,
            Button);
        DrawFoundationDiagnosticSurface(Character, Scale, false);
        return;
    }

    const bool bWorldSurface =
        FoundationSurface == FName(TEXT("world"));

    const float MenuWidth = FMath::Max(190.0f * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.18f);
    const float MenuHeight = FMath::Max(44.0f * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f + 20.0f);
    const float MenuX = Canvas->ClipX - MenuWidth - 24.0f * Scale;
    const float MenuY = 22.0f * Scale;
    DrawGamePanel(Button, MenuX, MenuY, MenuWidth, MenuHeight);
    DrawFoundationText(
        bSettingsOpen
            ? TEXT("Close")
            : TEXT("Settings"),
        MainText,
        MenuX + 12.0f * Scale,
        MenuY + 9.0f * Scale,
        nullptr,
        0.72f * Scale,
        false);
    AddFoundationHitBox(
        FVector2D(MenuX, MenuY),
        FVector2D(MenuWidth, MenuHeight),
        FName(TEXT("OG.Settings.Toggle")),
        true,
        100);

    const float PauseWidth = 140.0f * Scale;
    const float PauseX = MenuX - PauseWidth - 10.0f * Scale;
    if (bWorldSurface)
    {
        DrawGamePanel(Button, PauseX, MenuY, PauseWidth, MenuHeight);
        DrawFoundationText(
            bPauseOpen ? TEXT("Resume") : TEXT("Pause"),
            MainText,
            PauseX + 14.0f * Scale,
            MenuY + 9.0f * Scale,
            nullptr,
            0.68f * Scale,
            false);
        AddFoundationHitBox(
            FVector2D(PauseX, MenuY),
            FVector2D(PauseWidth, MenuHeight),
            FName(TEXT("OG.Pause.Toggle")),
            true,
            100);

        const float RulerWidth = 110.0f * Scale;
        const float RulerX = PauseX - RulerWidth - 10.0f * Scale;
        DrawGamePanel(Button, RulerX, MenuY, RulerWidth, MenuHeight);
        DrawFoundationText(
            TEXT("Ruler"),
            MainText,
            RulerX + 20.0f * Scale,
            MenuY + 9.0f * Scale,
            nullptr,
            0.68f * Scale,
            false);
        AddFoundationHitBox(
            FVector2D(RulerX, MenuY),
            FVector2D(RulerWidth, MenuHeight),
            FName(TEXT("OG.World.Ruler")),
            true,
            100);
    }

    if (bWorldSurface &&
        bPauseOpen && !bSettingsOpen && Character)
    {
        DrawRect(
            FLinearColor(0.0f, 0.0f, 0.0f, 0.66f),
            0.0f,
            0.0f,
            Canvas->ClipX,
            Canvas->ClipY);
        const float PausePanelW =
            FMath::Min(520.0f * Scale, Canvas->ClipX * 0.82f);
        const float PausePanelH = 260.0f * Scale;
        const float PausePanelX =
            (Canvas->ClipX - PausePanelW) * 0.5f;
        const float PausePanelY =
            (Canvas->ClipY - PausePanelH) * 0.5f;
        DrawRect(
            Panel,
            PausePanelX,
            PausePanelY,
            PausePanelW,
            PausePanelH);
        DrawFoundationText(
            TEXT("PAUSED"),
            MainText,
            PausePanelX + 28.0f * Scale,
            PausePanelY + 24.0f * Scale,
            nullptr,
            0.92f * Scale,
            false);

        const float PauseButtonW = PausePanelW - 56.0f * Scale;
        const float PauseButtonH = 44.0f * Scale;
        const float PauseButtonX = PausePanelX + 28.0f * Scale;
        float PauseButtonY = PausePanelY + 78.0f * Scale;
        auto PauseAction =
            [&](const FString& Label, FName Name)
            {
                DrawRect(
                    Button,
                    PauseButtonX,
                    PauseButtonY,
                    PauseButtonW,
                    PauseButtonH);
                DrawFoundationText(
                    Label,
                    MainText,
                    PauseButtonX + 14.0f * Scale,
                    PauseButtonY + 10.0f * Scale,
                    nullptr,
                    0.62f * Scale,
                    false);
                AddFoundationHitBox(
                    FVector2D(PauseButtonX, PauseButtonY),
                    FVector2D(PauseButtonW, PauseButtonH),
                    Name,
                    true,
                    115);
                PauseButtonY += 54.0f * Scale;
            };

        PauseAction(TEXT("Resume"), FName(TEXT("OG.Pause.Resume")));
        PauseAction(
            TEXT("Profile / Settings"),
            FName(TEXT("OG.Pause.Settings")));
        PauseAction(
            FString::Printf(
                TEXT("Privacy / SFW: %s"),
                Character->GetSfwPresentation()
                    ? TEXT("On")
                    : TEXT("Off")),
            FName(TEXT("OG.Settings.SFW")));
        return;
    }

    if (bSettingsOpen && Character)
    {
        DrawRect(
            FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
            0.0f,
            0.0f,
            Canvas->ClipX,
            Canvas->ClipY);

        const float PanelWidth =
            FMath::Min(820.0f * Scale, Canvas->ClipX * 0.90f);
        const float PanelHeight =
            Canvas->ClipY * 0.84f;
        const float PanelX =
            (Canvas->ClipX - PanelWidth) * 0.5f;
        const float PanelY =
            (Canvas->ClipY - PanelHeight) * 0.5f;

        DrawGamePanel(Panel, PanelX, PanelY, PanelWidth, PanelHeight);
        DrawFoundationText(
            TEXT("SETTINGS"),
            MainText,
            PanelX + 28.0f * Scale,
            PanelY + 24.0f * Scale,
            nullptr,
            0.92f * Scale,
            false);
        DrawFoundationText(
            TEXT("Changes save automatically"),
            SubText,
            PanelX + 28.0f * Scale,
            PanelY + 74.0f * Scale,
            nullptr,
            0.68f * Scale,
            false);

        const float ReadableHeight = FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f;
        const float ButtonH = FMath::Max(32.0f * Scale, ReadableHeight + 16.0f);
        const float RowStep = ButtonH + 10.0f * Scale;
        const float RowsTop = PanelY + 122.0f * Scale + ButtonH + 12.0f * Scale;
        const float RowsBottom = PanelY + PanelHeight - ButtonH - 20.0f * Scale;
        const int32 VisibleRows = FMath::Max(1, FMath::FloorToInt((RowsBottom - RowsTop) / RowStep));
        auto ProjectSettingsY = [&](float RawY) { return RawY - SettingsRowPage * VisibleRows * RowStep; };
        auto IsSettingsRowVisible = [&](float RawY)
        {
            const float ScreenY = ProjectSettingsY(RawY);
            return ScreenY >= RowsTop - 1.0f && ScreenY + ButtonH <= RowsBottom;
        };

        auto DrawHitButton =
            [&](const FString& Label,
                FName BoxName,
                float X,
                float Y,
                float W,
                float H,
                float TextScale = 0.66f)
            {
                if (Y >= RowsTop - 1.0f)
                {
                    if (!IsSettingsRowVisible(Y)) return;
                    Y = ProjectSettingsY(Y);
                }
                DrawGamePanel(Button, X, Y, W, H);
                DrawFoundationText(
                    Label,
                    MainText,
                    X + 10.0f * Scale,
                    Y + 8.0f * Scale,
                    nullptr,
                    TextScale * Scale,
                    false);
                AddFoundationHitBox(
                    FVector2D(X, Y),
                    FVector2D(W, H),
                    BoxName,
                    true,
                    110);
            };

        const float Left = PanelX + 28.0f * Scale;
        const float RowWidth = PanelWidth - 56.0f * Scale;

        const float SmallW = 48.0f * Scale;
        const float ValueW = 150.0f * Scale;

        float Y = PanelY + 122.0f * Scale;

        const float TabGap = 6.0f * Scale;
        const float TabW = (RowWidth - 3.0f * TabGap) / 4.0f;
        DrawHitButton(
            TEXT("Controls"),
            FName(TEXT("OG.Settings.Page.Controls")),
            Left,
            Y,
            TabW,
            ButtonH,
            0.56f);
        DrawHitButton(
            TEXT("Access"),
            FName(TEXT("OG.Settings.Page.Accessibility")),
            Left + TabW + TabGap,
            Y,
            TabW,
            ButtonH,
            0.50f);
        DrawHitButton(
            TEXT("Audio"),
            FName(TEXT("OG.Settings.Page.Audio")),
            Left + 2.0f * (TabW + TabGap),
            Y,
            TabW,
            ButtonH,
            0.56f);
        DrawHitButton(
            TEXT("Content"),
            FName(TEXT("OG.Settings.Page.Content")),
            Left + 3.0f * (TabW + TabGap),
            Y,
            TabW,
            ButtonH,
            0.52f);
        Y = RowsTop;

        auto DrawAdjustableRow =
            [&](const TCHAR* Label,
                float Value,
                FName MinusName,
                FName PlusName)
            {
                if (!IsSettingsRowVisible(Y)) { Y += RowStep; return; }
                const float ScreenY = ProjectSettingsY(Y);
                DrawFoundationText(
                    Label,
                    MainText,
                    Left,
                    ScreenY + 7.0f * Scale,
                    nullptr,
                    0.54f * Scale,
                    false);
                const float PlusX = Left + RowWidth - SmallW;
                const float MinusX = PlusX - SmallW - 6.0f * Scale;
                DrawHitButton(
                    TEXT("-"),
                    MinusName,
                    MinusX,
                    Y,
                    SmallW,
                    ButtonH,
                    0.72f);
                DrawHitButton(
                    TEXT("+"),
                    PlusName,
                    PlusX,
                    Y,
                    SmallW,
                    ButtonH,
                    0.72f);
                DrawFoundationText(
                    FString::Printf(TEXT("%.2f"), Value),
                    SubText,
                    MinusX - FMath::Max(90.0f * Scale, ReadableHeight * 2.5f),
                    ScreenY + 8.0f * Scale,
                    nullptr,
                    0.54f * Scale,
                    false);
                Y += RowStep;
            };

        const float HalfW = (RowWidth - 8.0f * Scale) * 0.5f;
        const int32 Page = Character->GetFoundationSettingsPage();

        if (Page == 0)
        {
            if (IsSettingsRowVisible(Y))
                DrawFoundationText(TEXT("Orientation"), MainText, Left,
                    ProjectSettingsY(Y) + 7.0f * Scale, nullptr, 0.54f * Scale, false);
            DrawHitButton(
                Character->GetOrientationOverride().ToString(),
                FName(TEXT("OG.Settings.Orientation")),
                Left + RowWidth - ValueW,
                Y,
                ValueW,
                ButtonH,
                0.50f);
            Y += RowStep;

            DrawAdjustableRow(
                TEXT("Camera horizontal"),
                Character->GetCameraHorizontalSensitivity(),
                FName(TEXT("OG.Settings.CameraH.Minus")),
                FName(TEXT("OG.Settings.CameraH.Plus")));
            DrawAdjustableRow(
                TEXT("Camera vertical"),
                Character->GetCameraVerticalSensitivity(),
                FName(TEXT("OG.Settings.CameraV.Minus")),
                FName(TEXT("OG.Settings.CameraV.Plus")));
            DrawAdjustableRow(
                TEXT("Camera response"),
                Character->GetCameraResponseExponent(),
                FName(TEXT("OG.Settings.Response.Minus")),
                FName(TEXT("OG.Settings.Response.Plus")));
            DrawHitButton(
                FString::Printf(
                    TEXT("Invert horizontal: %s"),
                    Character->GetInvertCameraX()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.InvertX")),
                Left,
                Y,
                HalfW,
                ButtonH,
                0.48f);
            DrawHitButton(
                FString::Printf(
                    TEXT("Invert vertical: %s"),
                    Character->GetInvertCameraY()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.InvertY")),
                Left + HalfW + 8.0f * Scale,
                Y,
                HalfW,
                ButtonH,
                0.48f);
            Y += RowStep;
            DrawAdjustableRow(
                TEXT("Control scale"),
                Character->GetTouchControlScale(),
                FName(TEXT("OG.Settings.TouchScale.Minus")),
                FName(TEXT("OG.Settings.TouchScale.Plus")));
            DrawAdjustableRow(
                TEXT("Control opacity"),
                Character->GetTouchControlOpacity(),
                FName(TEXT("OG.Settings.TouchOpacity.Minus")),
                FName(TEXT("OG.Settings.TouchOpacity.Plus")));
            DrawAdjustableRow(
                TEXT("Movement deadzone"),
                Character->GetMovementDeadzone(),
                FName(TEXT("OG.Settings.MoveDeadzone.Minus")),
                FName(TEXT("OG.Settings.MoveDeadzone.Plus")));
            DrawAdjustableRow(
                TEXT("Look deadzone"),
                Character->GetLookDeadzone(),
                FName(TEXT("OG.Settings.LookDeadzone.Minus")),
                FName(TEXT("OG.Settings.LookDeadzone.Plus")));
            DrawAdjustableRow(
                TEXT("Stick horizontal"),
                Character->GetMovementStickInset(),
                FName(TEXT("OG.Settings.MoveInset.Minus")),
                FName(TEXT("OG.Settings.MoveInset.Plus")));
            DrawAdjustableRow(
                TEXT("Stick vertical"),
                Character->GetMovementStickBottom(),
                FName(TEXT("OG.Settings.MoveBottom.Minus")),
                FName(TEXT("OG.Settings.MoveBottom.Plus")));
            DrawAdjustableRow(
                TEXT("Actions horizontal"),
                Character->GetActionClusterHorizontalOffset(),
                FName(TEXT("OG.Settings.ActionX.Minus")),
                FName(TEXT("OG.Settings.ActionX.Plus")));
            DrawAdjustableRow(
                TEXT("Actions vertical"),
                Character->GetActionClusterVerticalOffset(),
                FName(TEXT("OG.Settings.ActionY.Minus")),
                FName(TEXT("OG.Settings.ActionY.Plus")));

            DrawHitButton(
                FString::Printf(
                    TEXT("Handedness: %s"),
                    Character->GetLeftHandedControls()
                        ? TEXT("Left")
                        : TEXT("Right")),
                FName(TEXT("OG.Settings.Handedness")),
                Left,
                Y,
                HalfW,
                ButtonH,
                0.48f);
            DrawHitButton(
                FString::Printf(
                    TEXT("Sprint: %s"),
                    Character->GetSprintTogglePreference()
                        ? TEXT("Toggle")
                        : TEXT("Hold")),
                FName(TEXT("OG.Settings.Sprint")),
                Left + HalfW + 8.0f * Scale,
                Y,
                HalfW,
                ButtonH,
                0.48f);
        }
        else if (Page == 1)
        {
            DrawHitButton(
                FString::Printf(
                    TEXT("Privacy / SFW: %s"),
                    Character->GetSfwPresentation()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.SFW")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Reduced motion: %s"),
                    Character->GetReducedMotion()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.ReducedMotion")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Reduced camera shake: %s"),
                    Character->GetReducedCameraShake()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.ReducedShake")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Subtitles / captions: %s"),
                    Character->GetSubtitlesEnabled()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.Subtitles")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Subtitle presentation: %s"),
                    *Character->GetSubtitlePresentation().ToString()),
                FName(TEXT("OG.Settings.SubtitlePresentation")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("UI readability: %s"),
                    *Character->GetUiReadabilityProfile().ToString()),
                FName(TEXT("OG.Settings.Readability")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Color-vision palette: %s"),
                    *Character->GetColorVisionProfile().ToString()),
                FName(TEXT("OG.Settings.ColorVision")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawHitButton(
                FString::Printf(
                    TEXT("Haptics: %s"),
                    Character->GetHapticsEnabled()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.Haptics")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
            Y += RowStep;
            DrawAdjustableRow(
                TEXT("Haptics intensity"),
                Character->GetHapticsIntensity(),
                FName(TEXT("OG.Settings.HapticsIntensity.Minus")),
                FName(TEXT("OG.Settings.HapticsIntensity.Plus")));
            DrawHitButton(
                FString::Printf(
                    TEXT("Damage numbers: %s"),
                    *Character->GetDamageNumberPresentation().ToString()),
                FName(TEXT("OG.Settings.DamageNumbers")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
        }
        else if (Page == 2)
        {
            DrawAdjustableRow(
                TEXT("Master volume"),
                Character->GetMasterVolume(),
                FName(TEXT("OG.Settings.MasterVolume.Minus")),
                FName(TEXT("OG.Settings.MasterVolume.Plus")));
            DrawAdjustableRow(
                TEXT("Music volume"),
                Character->GetMusicVolume(),
                FName(TEXT("OG.Settings.MusicVolume.Minus")),
                FName(TEXT("OG.Settings.MusicVolume.Plus")));
            DrawAdjustableRow(
                TEXT("Voice volume"),
                Character->GetVoiceVolume(),
                FName(TEXT("OG.Settings.VoiceVolume.Minus")),
                FName(TEXT("OG.Settings.VoiceVolume.Plus")));
            DrawAdjustableRow(
                TEXT("SFX volume"),
                Character->GetSfxVolume(),
                FName(TEXT("OG.Settings.SfxVolume.Minus")),
                FName(TEXT("OG.Settings.SfxVolume.Plus")));
            DrawAdjustableRow(
                TEXT("Ambience volume"),
                Character->GetAmbienceVolume(),
                FName(TEXT("OG.Settings.AmbienceVolume.Minus")),
                FName(TEXT("OG.Settings.AmbienceVolume.Plus")));
            DrawHitButton(
                FString::Printf(
                    TEXT("Dynamic range: %s"),
                    *Character->GetDynamicRangeProfile().ToString()),
                FName(TEXT("OG.Settings.DynamicRange")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.52f);
        }
        else
        {
            DrawHitButton(
                FString::Printf(
                    TEXT("Roster density: %s"),
                    *Character->GetRosterDensity().ToString()),
                FName(TEXT("OG.Settings.RosterDensity")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.50f);
            Y += RowStep;

            DrawHitButton(
                FString::Printf(
                    TEXT("Ultimate cinematics: %s"),
                    *Character->GetCinematicRepeatPolicy().ToString()),
                FName(TEXT("OG.Settings.CinematicPolicy")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.50f);
            Y += RowStep;

            DrawHitButton(
                FString::Printf(
                    TEXT("Automatic content downloads: %s"),
                    Character->GetAutoDownloadEnabled()
                        ? TEXT("On")
                        : TEXT("Off")),
                FName(TEXT("OG.Settings.AutoDownload")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.50f);
            Y += RowStep;

            DrawHitButton(
                FString::Printf(
                    TEXT("Large downloads: %s"),
                    Character->GetLargeDownloadsUnmeteredOnly()
                        ? TEXT("Wi-Fi / unmetered only")
                        : TEXT("Any network")),
                FName(TEXT("OG.Settings.DownloadNetwork")),
                Left,
                Y,
                RowWidth,
                ButtonH,
                0.48f);
        }
        const int32 TotalRows = FMath::FloorToInt((Y - RowsTop) / RowStep) + 1;
        SettingsLastRowPage = FMath::Max(0, (TotalRows - 1) / VisibleRows);
        SettingsRowPage = FMath::Clamp(SettingsRowPage, 0, SettingsLastRowPage);
        if (SettingsLastRowPage > 0)
        {
            const float FooterY = PanelY + PanelHeight - ButtonH - 8.0f * Scale;
            const float FooterW = (RowWidth - 12.0f * Scale) * 0.5f;
            // Footer targets are outside the scrolling row projection.
            for (int32 Direction : {-1, 1})
            {
                const float X = Left + (Direction > 0 ? FooterW + 12.0f * Scale : 0.0f);
                const FName Name = Direction < 0 ? TEXT("OG.Settings.Rows.Previous") : TEXT("OG.Settings.Rows.Next");
                DrawGamePanel(Button, X, FooterY, FooterW, ButtonH);
                DrawFoundationText(Direction < 0 ? TEXT("Previous controls") : TEXT("More controls"),
                    MainText, X + 10.0f * Scale, FooterY + 8.0f * Scale, nullptr, 0.54f * Scale);
                AddFoundationHitBox(FVector2D(X, FooterY), FVector2D(FooterW, ButtonH), Name, true, 112);
            }
        }
        return;
    }

    if (Character)
    {
        if (auto* Combat = Character->FindComponentByClass<UOGDiagnosticCombatComponent>())
        {
            FString Status = Combat->GetStatus();
            if (Status.StartsWith(TEXT("Training:"))) Status.Reset();
            const bool bNarrow = Canvas->ClipX < Canvas->ClipY;
            DrawFoundationText(Status.Left(bNarrow ? 28 : 80), MainText, Canvas->ClipX * .25f,
                bNarrow ? Canvas->ClipY * .20f : MenuY + MenuHeight + 12.0f * Scale,
                nullptr, .45f * Scale);
        }
        // Knowledge-safe navigation: a compass is always legitimate because
        // it derives only from the player's local facing. No omniscient map
        // completion or hidden-location data is projected here.
        const float Yaw =
            FRotator::NormalizeAxis(
                Character->GetControlRotation().Yaw);
        const float CompassYaw =
            Yaw < 0.0f ? Yaw + 360.0f : Yaw;
        const TCHAR* Cardinal =
            (CompassYaw >= 315.0f || CompassYaw < 45.0f)
                ? TEXT("N")
                : (CompassYaw < 135.0f)
                    ? TEXT("E")
                    : (CompassYaw < 225.0f)
                        ? TEXT("S")
                        : TEXT("W");
        DrawRect(
            FLinearColor(0.04f, 0.05f, 0.06f, 0.76f),
            28.0f * Scale,
            24.0f * Scale,
            112.0f * Scale,
            38.0f * Scale);
        DrawFoundationText(
            FString::Printf(
                TEXT("%s  %03.0f"),
                Cardinal,
                CompassYaw),
            MainText,
            42.0f * Scale,
            34.0f * Scale,
            nullptr,
            0.58f * Scale,
            false);

        // Knowledge-limited minimap: only the player's actually visited trail
        // is drawn. Hidden locations, enemies and undiscovered structures are
        // never projected into this map.
        const float MiniX = 28.0f * Scale;
        const float MiniY = 70.0f * Scale;
        const float MiniSize = 146.0f * Scale;
        const float MiniCenterX = MiniX + MiniSize * 0.5f;
        const float MiniCenterY = MiniY + MiniSize * 0.5f;
        DrawRect(
            FLinearColor(0.025f, 0.035f, 0.045f, 0.80f),
            MiniX,
            MiniY,
            MiniSize,
            MiniSize);
        DrawFoundationText(
            TEXT("Map"),
            SubText,
            MiniX + 8.0f * Scale,
            MiniY + 6.0f * Scale,
            nullptr,
            0.40f * Scale,
            false);

        const FVector CharacterLocation =
            Character->GetActorLocation();
        const FVector2D CurrentMapPoint(
            CharacterLocation.X,
            CharacterLocation.Y);
        constexpr float MiniWorldRadius = 6000.0f;
        for (const FVector2D& Visited :
             Character->GetVisitedMapTrailForHud())
        {
            const FVector2D Delta =
                Visited - CurrentMapPoint;
            if (FMath::Abs(Delta.X) > MiniWorldRadius ||
                FMath::Abs(Delta.Y) > MiniWorldRadius)
            {
                continue;
            }

            const float X =
                MiniCenterX +
                (Delta.X / MiniWorldRadius) *
                    (MiniSize * 0.44f);
            const float Y =
                MiniCenterY -
                (Delta.Y / MiniWorldRadius) *
                    (MiniSize * 0.44f);
            DrawRect(
                FLinearColor(0.64f, 0.72f, 0.76f, 0.82f),
                X - 1.5f * Scale,
                Y - 1.5f * Scale,
                3.0f * Scale,
                3.0f * Scale);
        }

        DrawRect(
            MainText,
            MiniCenterX - 3.0f * Scale,
            MiniCenterY - 3.0f * Scale,
            6.0f * Scale,
            6.0f * Scale);

        const float HeadingRadians =
            FMath::DegreesToRadians(CompassYaw);
        const FVector2D Heading(
            FMath::Cos(HeadingRadians),
            FMath::Sin(HeadingRadians));
        DrawRect(
            MainText,
            MiniCenterX + Heading.X * 10.0f * Scale - 1.0f * Scale,
            MiniCenterY - Heading.Y * 10.0f * Scale - 1.0f * Scale,
            3.0f * Scale,
            3.0f * Scale);

        // Controlled-character HP: compact and permanently readable without
        // turning World Mode into a dashboard.
        const float HpPanelX = Canvas->ClipX * 0.5f - 125.0f * Scale;
        const float HealthShortSide = FMath::Min(Canvas->ClipX, Canvas->ClipY);
        const float HealthActionSize = HealthShortSide * 0.148f *
            FMath::Clamp(Character->GetTouchControlScale(), 0.9f, 1.1f);
        const float HealthCell = HealthActionSize + HealthShortSide * 0.016f;
        const float HealthMinimumInset = HealthActionSize * 0.5f + HealthShortSide * 0.016f;
        const float HealthBaseY = FMath::Clamp(HealthShortSide * 0.13f +
            Canvas->ClipY * Character->GetActionClusterVerticalOffset(),
            HealthMinimumInset, FMath::Max(HealthMinimumInset,
                Canvas->ClipY * 0.65f - 2.0f * HealthCell - HealthActionSize * 0.5f));
        const float HealthActionsTop = Canvas->ClipY - HealthBaseY -
            2.0f * HealthCell - HealthActionSize * 0.5f;
        const float HpPanelY = Canvas->ClipY > Canvas->ClipX
            ? HealthActionsTop - 72.0f * Scale - HealthShortSide / 27.0f
            : Canvas->ClipY - 112.0f * Scale;
        const float HpPanelW = 250.0f * Scale;
        const float HpPanelH = 24.0f * Scale;
        const float HpRatio =
            Character->GetFoundationMaxHealth() > KINDA_SMALL_NUMBER
                ? FMath::Clamp(
                    Character->GetFoundationHealth() /
                        Character->GetFoundationMaxHealth(),
                    0.0f,
                    1.0f)
                : 0.0f;
        DrawRect(
            FLinearColor(0.03f, 0.04f, 0.05f, 0.82f),
            HpPanelX,
            HpPanelY,
            HpPanelW,
            HpPanelH);
        DrawRect(
            FLinearColor(0.72f, 0.78f, 0.80f, 0.90f),
            HpPanelX + 2.0f * Scale,
            HpPanelY + 2.0f * Scale,
            (HpPanelW - 4.0f * Scale) * HpRatio,
            HpPanelH - 4.0f * Scale);
        if (Character->GetFoundationHealth() <= 0.0f)
        {
            const float NoticeW = FMath::Min(Canvas->ClipX * 0.55f, 660.0f * Scale);
            const float NoticeX = (Canvas->ClipX - NoticeW) * 0.5f;
            const float NoticeY = Canvas->ClipY * 0.12f;
            DrawGamePanel(Panel, NoticeX, NoticeY, NoticeW, 96.0f * Scale);
            DrawFoundationText(TEXT("Party defeated"), MainText, NoticeX + 16.0f * Scale,
                NoticeY + 14.0f * Scale, nullptr, 0.78f * Scale);
            DrawFoundationText(TEXT("Ruler > Characters > Assign"), SubText,
                NoticeX + 16.0f * Scale, NoticeY + 54.0f * Scale, nullptr, 0.56f * Scale);
        }
        DrawFoundationText(
            Character->GetCanonicalHealthText(),
            MainText,
            HpPanelX,
            HpPanelY - FMath::Max(42.0f * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f + 8.0f),
            nullptr,
            0.58f * Scale,
            false);

        if (Character->IsControlledResourceVisibleForHud())
        {
            const float ResourceMax =
                Character->GetControlledResourceMaxForHud();
            const float ResourceRatio =
                ResourceMax > KINDA_SMALL_NUMBER
                    ? FMath::Clamp(
                        Character->GetControlledResourceCurrentForHud() /
                            ResourceMax,
                        0.0f,
                        1.0f)
                    : 0.0f;
            const float ResourceY =
                HpPanelY + 32.0f * Scale;
            DrawRect(
                FLinearColor(0.03f, 0.04f, 0.05f, 0.78f),
                HpPanelX,
                ResourceY,
                HpPanelW,
                16.0f * Scale);
            DrawRect(
                FLinearColor(0.55f, 0.62f, 0.70f, 0.90f),
                HpPanelX + 2.0f * Scale,
                ResourceY + 2.0f * Scale,
                (HpPanelW - 4.0f * Scale) * ResourceRatio,
                12.0f * Scale);
            DrawFoundationText(
                FString::Printf(
                    TEXT("%s %.0f / %.0f"),
                    *Character->GetControlledResourceLabelForHud().ToString(),
                    Character->GetControlledResourceCurrentForHud(),
                    ResourceMax),
                SubText,
                HpPanelX,
                ResourceY + 20.0f * Scale,
                nullptr,
                0.46f * Scale,
                false);
        }

        // Show the two off-field party members on the upper-right. The list
        // automatically changes when control transfers, so the protagonist
        // remains switchable while a companion is controlled.
        int32 PartyDisplayIndex = 0;
        for (int32 SlotIndex = 0;
             SlotIndex < Character->GetPartySlotCountForHud();
             ++SlotIndex)
        {
            if (SlotIndex == Character->GetControlledPartySlotForHud())
            {
                continue;
            }

            const float CardW = FMath::Max(340.0f * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.34f);
            const float CardH = FMath::Max(92.0f * Scale, 2.0f * FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f + 20.0f);
            const float CardX =
                Canvas->ClipX - CardW - 24.0f * Scale;
            const float CardY =
                MenuY + MenuHeight + 20.0f * Scale +
                PartyDisplayIndex * (CardH + 12.0f * Scale);
            const bool bAvailable =
                Character->IsPartySlotAvailableForHud(SlotIndex);
            const bool bQteReady =
                Character->IsPartySlotQteReadyForHud(SlotIndex);
            const FString SlotLabel =
                SlotIndex == 0
                    ? FString(TEXT("Protagonist"))
                    : FString::Printf(
                        TEXT("Companion %d"),
                        SlotIndex);

            DrawGamePanel(FLinearColor(.025f,.045f,.075f,.90f),CardX,CardY,CardW,CardH);
            DrawFoundationCharacterStandIn(CardX,CardY,CardH*.74f,CardH,
                bAvailable?FLinearColor(.38f,.72f,.75f,1):FLinearColor(.35f,.38f,.4f,1));
            DrawGameText(SlotLabel,CardX+CardH*.85f,CardY+CardH*.12f,CardH*.26f,
                MainText,CardW-CardH*.92f);
            DrawGameText(bAvailable?(bQteReady?TEXT("QTE ready"):TEXT("Switch")):TEXT("Unavailable"),
                CardX+CardH*.85f,CardY+CardH*.56f,CardH*.22f,
                bQteReady?FLinearColor(.83f,.72f,.47f,1):SubText,CardW-CardH*.92f);

            if (bAvailable)
            {
                AddFoundationHitBox(
                    FVector2D(CardX, CardY),
                    FVector2D(CardW, CardH),
                    FName(*FString::Printf(
                        TEXT("OG.Party.Switch.%d"),
                        SlotIndex)),
                    true,
                    106);

                if (bQteReady)
                {
                    const float QteW = 72.0f * Scale;
                    DrawRect(
                        FLinearColor(0.20f, 0.23f, 0.28f, 0.96f),
                        CardX - QteW - 8.0f * Scale,
                        CardY,
                        QteW,
                        CardH);
                    DrawFoundationText(
                        TEXT("QTE"),
                        MainText,
                        CardX - QteW + 17.0f * Scale,
                        CardY + 17.0f * Scale,
                        nullptr,
                        0.58f * Scale,
                        false);
                    AddFoundationHitBox(
                        FVector2D(
                            CardX - QteW - 8.0f * Scale,
                            CardY),
                        FVector2D(QteW, CardH),
                        FName(*FString::Printf(
                            TEXT("OG.Party.QTE.%d"),
                            SlotIndex)),
                        true,
                        107);
                }
            }

            ++PartyDisplayIndex;
            if (PartyDisplayIndex >= 2)
            {
                break;
            }
        }

        // Active-kit controls are projected only when the controlled unit's
        // resolved authored kit actually contains them. This keeps the
        // unarmed opening clean while making real skills usable on touch.
        auto DrawWorldActionButton =
            [&](const FString& Label,
                FName HitBoxName,
                float HorizontalInset,
                float BottomInset)
            {
                const bool bLeftHanded =
                    Character->GetLeftHandedControls();
                const float ShortSide = FMath::Min(Canvas->ClipX, Canvas->ClipY);
                const float ButtonSize = ShortSide * 0.148f *
                    FMath::Clamp(Character->GetTouchControlScale(), 0.9f, 1.1f);
                const float Cell = ButtonSize + ShortSide * 0.016f;
                const float Column = FMath::RoundToFloat((HorizontalInset - 0.08f) / 0.09f);
                const float Row = FMath::RoundToFloat((BottomInset - 0.13f) / 0.17f);
                const float Margin = ShortSide * 0.016f;
                const float MinimumInset = ButtonSize * 0.5f + Margin;
                // Clamp the whole grid, preserving spacing at profile extremes.
                const float BaseX = FMath::Clamp(ShortSide * 0.08f +
                    Canvas->ClipX * Character->GetActionClusterHorizontalOffset(),
                    MinimumInset, FMath::Max(MinimumInset,
                        Canvas->ClipX * 0.62f - 2.0f * Cell - ButtonSize * 0.5f));
                const float BaseY = FMath::Clamp(ShortSide * 0.13f +
                    Canvas->ClipY * Character->GetActionClusterVerticalOffset(),
                    MinimumInset, FMath::Max(MinimumInset,
                        Canvas->ClipY * 0.65f - 2.0f * Cell - ButtonSize * 0.5f));
                const float SideInset = BaseX + Column * Cell;
                const float CenterX = bLeftHanded ? SideInset : Canvas->ClipX - SideInset;
                const float CenterY = Canvas->ClipY - BaseY - Row * Cell;
                const float X = CenterX - ButtonSize * 0.5f;
                const float Y = CenterY - ButtonSize * 0.5f;

                bool bReady = Character->GetFoundationHealth() > 0.0f;
                FString DisabledReason;
                if (auto* Combat = Character->FindComponentByClass<UOGDiagnosticCombatComponent>())
                {
                    if (HitBoxName == TEXT("OG.World.Attack")) bReady = Combat->IsWorldActionReady(EOGDiagnosticCommand::Basic, DisabledReason);
                    else if (HitBoxName == TEXT("OG.World.Skill.0")) bReady = Combat->IsWorldActionReady(EOGDiagnosticCommand::Skill1, DisabledReason);
                    else if (HitBoxName == TEXT("OG.World.Skill.1")) bReady = Combat->IsWorldActionReady(EOGDiagnosticCommand::Skill2, DisabledReason);
                    else if (HitBoxName == TEXT("OG.World.Ultimate")) bReady = Combat->IsWorldActionReady(EOGDiagnosticCommand::Ultimate, DisabledReason);
                }
                const bool bPressed = PressedWorldControls.Contains(HitBoxName);
                const FLinearColor Accent = bReady ? FLinearColor(.83f,.72f,.47f,1) : FLinearColor(.4f,.46f,.52f,1);
                Canvas->K2_DrawPolygon(nullptr, FVector2D(CenterX,CenterY),
                    FVector2D(ButtonSize*.48f,ButtonSize*.48f), 40,
                    FLinearColor(bPressed?.14f:.025f,bPressed?.24f:.045f,bPressed?.30f:.075f,Character->GetTouchControlOpacity()));
                for(int32 I=0; I<32; ++I)
                {
                    const float A=I*2*PI/32, B=(I+1)*2*PI/32, R=ButtonSize*.46f;
                    DrawLine(CenterX+FMath::Cos(A)*R,CenterY+FMath::Sin(A)*R,
                        CenterX+FMath::Cos(B)*R,CenterY+FMath::Sin(B)*R,Accent,bPressed?3.f:1.5f);
                }
                DrawGameIcon(FName(*Label),X+ButtonSize*.26f,Y+ButtonSize*.10f,ButtonSize*.48f,Accent);
                const FString Caption=Label;
                DrawGameText(Caption,X+ButtonSize*.15f,Y+ButtonSize*.65f,
                    ButtonSize*.18f,bReady?MainText:SubText,ButtonSize*.70f);
                AddFoundationHitBox(
                    FVector2D(X, Y),
                    FVector2D(ButtonSize, ButtonSize),
                    HitBoxName,
                    true,
                    109);
            };

        DrawWorldActionButton(TEXT("Attack"), TEXT("OG.World.Attack"), .08f, .13f);
        DrawWorldActionButton(TEXT("Dodge"), TEXT("OG.World.Dodge"), .17f, .13f);
        const FString Traversal = Character->GetTraversalModeLabelForHud();
        const bool bWater = Traversal == TEXT("Swimming") || Traversal == TEXT("Diving");
        DrawWorldActionButton(bWater ? TEXT("Rise") :
            Traversal == TEXT("Flying") ? TEXT("Ascend") : TEXT("Jump"),
            TEXT("OG.World.Jump"), .26f, .13f);
        DrawWorldActionButton(TEXT("Lock"), TEXT("OG.World.Lock"), .08f, .47f);
        DrawWorldActionButton(TEXT("Sprint"), TEXT("OG.World.Sprint"), .17f, .47f);

        if (Character->HasSkillSlotForHud(0))
        {
            DrawWorldActionButton(
                TEXT("Skill 1"),
                FName(TEXT("OG.World.Skill.0")),
                0.08f,
                0.30f);
        }
        if (Character->HasSkillSlotForHud(1))
        {
            DrawWorldActionButton(
                TEXT("Skill 2"),
                FName(TEXT("OG.World.Skill.1")),
                0.17f,
                0.30f);
        }
        if (Character->HasUltimateForHud())
        {
            DrawWorldActionButton(
                TEXT("Ultimate"),
                FName(TEXT("OG.World.Ultimate")),
                0.26f,
                0.30f);
        }

        if (Character->ShouldShowTraversalDownForHud())
            DrawWorldActionButton(bWater ? TEXT("Dive") :
                Traversal == TEXT("Climbing") ? TEXT("Let go") : TEXT("Descend"),
                TEXT("OG.World.Descend"), .26f, .47f);

        int32 SecondaryIndex = 0;
        for (const FOGWorldTargetViewModel& Secondary :
             Character->GetSecondaryTargetHudProjectionsForHud())
        {
            if (SecondaryIndex >= 4)
            {
                break;
            }

            const float IndicatorW = 152.0f * Scale;
            const float IndicatorH = 34.0f * Scale;
            const float IndicatorX =
                Canvas->ClipX - IndicatorW - 24.0f * Scale;
            const float IndicatorY =
                238.0f * Scale +
                SecondaryIndex * 40.0f * Scale;
            DrawRect(
                FLinearColor(0.055f, 0.07f, 0.085f, 0.84f),
                IndicatorX,
                IndicatorY,
                IndicatorW,
                IndicatorH);
            DrawFoundationText(
                FString::Printf(
                    TEXT("Enemy %d  %s"),
                    SecondaryIndex + 1,
                    *Secondary.HpDisplay),
                SubText,
                IndicatorX + 8.0f * Scale,
                IndicatorY + 8.0f * Scale,
                nullptr,
                0.42f * Scale,
                false);
            ++SecondaryIndex;
        }

        if (AActor* LockedTarget = Character->GetHardLockedTarget())
        {
            const FVector TargetPoint =
                LockedTarget->GetClass()->ImplementsInterface(
                    UOGWorldTargetable::StaticClass())
                    ? IOGWorldTargetable::Execute_GetTargetPoint(
                        LockedTarget,
                        Character)
                    : LockedTarget->GetActorLocation();
            FVector2D TargetScreen;
            if (PlayerOwner &&
                PlayerOwner->ProjectWorldLocationToScreen(
                    TargetPoint,
                    TargetScreen,
                    true))
            {
                const float Marker = 18.0f * Scale;
                DrawRect(
                    MainText,
                    TargetScreen.X - Marker,
                    TargetScreen.Y - 1.0f * Scale,
                    Marker * 0.7f,
                    2.0f * Scale);
                DrawRect(
                    MainText,
                    TargetScreen.X + Marker * 0.3f,
                    TargetScreen.Y - 1.0f * Scale,
                    Marker * 0.7f,
                    2.0f * Scale);
                DrawRect(
                    MainText,
                    TargetScreen.X - 1.0f * Scale,
                    TargetScreen.Y - Marker,
                    2.0f * Scale,
                    Marker * 0.7f);
                DrawRect(
                    MainText,
                    TargetScreen.X - 1.0f * Scale,
                    TargetScreen.Y + Marker * 0.3f,
                    2.0f * Scale,
                    Marker * 0.7f);
            }

            const float TargetW = 330.0f * Scale;
            const float TargetH = 48.0f * Scale;
            const float TargetX =
                (Canvas->ClipX - TargetW) * 0.5f;
            const float TargetY = 24.0f * Scale;
            DrawRect(
                FLinearColor(0.04f, 0.05f, 0.06f, 0.90f),
                TargetX,
                TargetY,
                TargetW,
                TargetH);

            FString TargetState(TEXT("Target locked"));
            const FOGWorldTargetViewModel* TargetProjection =
                Character->HasTargetHudProjectionForHud()
                    ? &Character->GetTargetHudProjectionForHud()
                    : nullptr;

            if (TargetProjection)
            {
                TargetState = FString::Printf(
                    TEXT("Target  %s"),
                    *TargetProjection->HpDisplay);
            }
            else if (AreFoundationDebugFixturesEnabled())
            {
                if (const AOGFoundationCombatTarget* FoundationTarget =
                        Cast<AOGFoundationCombatTarget>(LockedTarget))
                {
                    TargetState = FString::Printf(
                        TEXT("Target  %.0f / %.0f HP"),
                        FoundationTarget->GetCurrentHealth(),
                        FoundationTarget->GetMaxHealth());

                    const float TargetHpRatio =
                        FoundationTarget->GetMaxHealth() > KINDA_SMALL_NUMBER
                            ? FMath::Clamp(
                                FoundationTarget->GetCurrentHealth() /
                                    FoundationTarget->GetMaxHealth(),
                                0.0f,
                                1.0f)
                            : 0.0f;
                    DrawRect(
                        FLinearColor(0.68f, 0.68f, 0.70f, 0.85f),
                        TargetX + 2.0f * Scale,
                        TargetY + TargetH - 6.0f * Scale,
                        (TargetW - 4.0f * Scale) * TargetHpRatio,
                        4.0f * Scale);
                }
            }

            DrawFoundationText(
                TargetState,
                MainText,
                TargetX + 12.0f * Scale,
                TargetY + 10.0f * Scale,
                nullptr,
                0.62f * Scale,
                false);

            if (TargetProjection)
            {
                float StatusX = TargetX;
                const float StatusY =
                    TargetY + TargetH + 6.0f * Scale;
                for (int32 StatusIndex = 0;
                     StatusIndex < TargetProjection->StatusEffects.Num() &&
                     StatusIndex < 6;
                     ++StatusIndex)
                {
                    const FOGHudStatusEffectViewModel& Status =
                        TargetProjection->StatusEffects[StatusIndex];
                    const float StatusW = 82.0f * Scale;
                    const float StatusH = 34.0f * Scale;
                    DrawRect(
                        ExpandedTargetStatusIndex == StatusIndex
                            ? FLinearColor(0.18f, 0.22f, 0.27f, 0.96f)
                            : FLinearColor(0.08f, 0.10f, 0.12f, 0.90f),
                        StatusX,
                        StatusY,
                        StatusW,
                        StatusH);
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("%s x%d"),
                            *Status.EffectId.ToString(),
                            Status.Stacks),
                        MainText,
                        StatusX + 6.0f * Scale,
                        StatusY + 8.0f * Scale,
                        nullptr,
                        0.38f * Scale,
                        false);
                    AddFoundationHitBox(
                        FVector2D(StatusX, StatusY),
                        FVector2D(StatusW, StatusH),
                        FName(*FString::Printf(
                            TEXT("OG.Target.Status.%d"),
                            StatusIndex)),
                        true,
                        110);
                    StatusX += StatusW + 6.0f * Scale;
                }

                if (TargetProjection->StatusEffects.IsValidIndex(
                        ExpandedTargetStatusIndex))
                {
                    const FOGHudStatusEffectViewModel& Expanded =
                        TargetProjection->StatusEffects[
                            ExpandedTargetStatusIndex];
                    const FString Detail =
                        Expanded.bPreciseDetailsVisible &&
                        !Expanded.PreciseEffectTextKey.IsEmpty()
                            ? Expanded.PreciseEffectTextKey
                            : FString::Printf(
                                TEXT("%s — exact details unknown"),
                                *Expanded.EffectId.ToString());
                    DrawFoundationText(
                        Detail,
                        SubText,
                        TargetX,
                        StatusY + 40.0f * Scale,
                        nullptr,
                        0.44f * Scale,
                        false);
                }

                if (TargetProjection->Phase.bExactValueVisible)
                {
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("Phase: %s"),
                            *TargetProjection->Phase.ValueJson),
                        SubText,
                        TargetX,
                        StatusY + 64.0f * Scale,
                        nullptr,
                        0.42f * Scale,
                        false);
                }
                if (TargetProjection->ResourceState.bExactValueVisible)
                {
                    DrawFoundationText(
                        FString::Printf(
                            TEXT("Resource: %s"),
                            *TargetProjection->ResourceState.ValueJson),
                        SubText,
                        TargetX,
                        StatusY + 84.0f * Scale,
                        nullptr,
                        0.42f * Scale,
                        false);
                }
            }

            const float AimW = 74.0f * Scale;
            const float AimX =
                TargetX + TargetW + 10.0f * Scale;
            DrawRect(
                Character->IsManualAimEnabled()
                    ? FLinearColor(0.20f, 0.24f, 0.30f, 0.96f)
                    : FLinearColor(0.10f, 0.12f, 0.14f, 0.90f),
                AimX,
                TargetY,
                AimW,
                TargetH);
            DrawFoundationText(
                Character->IsManualAimEnabled()
                    ? TEXT("Aim on")
                    : TEXT("Aim"),
                MainText,
                AimX + 10.0f * Scale,
                TargetY + 13.0f * Scale,
                nullptr,
                0.53f * Scale,
                false);
            AddFoundationHitBox(
                FVector2D(AimX, TargetY),
                FVector2D(AimW, TargetH),
                FName(TEXT("OG.World.Aim")),
                true,
                108);
        }

        if (Character->IsManualAimEnabled())
        {
            const float CenterX = Canvas->ClipX * 0.5f;
            const float CenterY = Canvas->ClipY * 0.5f;
            DrawRect(
                MainText,
                CenterX - 12.0f * Scale,
                CenterY - 1.0f * Scale,
                24.0f * Scale,
                2.0f * Scale);
            DrawRect(
                MainText,
                CenterX - 1.0f * Scale,
                CenterY - 12.0f * Scale,
                2.0f * Scale,
                24.0f * Scale);
        }

        if (Character->HasRecentFoundationDamage() &&
            Character->GetDamageNumberPresentation() !=
                FName(TEXT("off")))
        {
            DrawFoundationText(
                FString::Printf(
                    TEXT("-%.0f"),
                    Character->GetRecentFoundationDamage()),
                MainText,
                Canvas->ClipX * 0.5f + 28.0f * Scale,
                Canvas->ClipY * 0.40f,
                nullptr,
                0.95f * Scale,
                false);
        }

        const FString TraversalLabel =
            Character->GetTraversalModeLabelForHud();
        if (TraversalLabel != TEXT("Ground"))
        {
            DrawFoundationText(
                TraversalLabel,
                MainText,
                28.0f * Scale,
                28.0f * Scale,
                nullptr,
                0.66f * Scale,
                false);
        }

        if (Traversal == TEXT("Flying"))
        {
            const float ExitW = 180.0f * Scale, ExitH = 56.0f * Scale;
            const FVector2D ExitAt((Canvas->ClipX - ExitW) * 0.5f, Canvas->ClipY * 0.65f);
            DrawGamePanel(Button, ExitAt.X, ExitAt.Y, ExitW, ExitH);
            DrawFoundationText(TEXT("Stop flight"), MainText, ExitAt.X + 12.0f * Scale,
                ExitAt.Y + 14.0f * Scale, nullptr, 0.62f * Scale);
            AddFoundationHitBox(ExitAt, FVector2D(ExitW, ExitH),
                TEXT("OG.World.EndFlight"), true, 110);
        }

        if (AActor* Interactable =
                Character->GetContextInteractableForHud())
        {
            const float PromptX =
                Canvas->ClipX * 0.5f - 125.0f * Scale;
            const float PromptY =
                Canvas->ClipY * 0.72f;
            const float PromptW = 250.0f * Scale;
            const float PromptH = 56.0f * Scale;
            FText Prompt;
            if (Interactable->GetClass()->ImplementsInterface(UOGWorldInteractable::StaticClass()))
                Prompt = IOGWorldInteractable::Execute_GetInteractionPrompt(Interactable, Character);
            const FString PromptText =
                Traversal == TEXT("Mounted") ? FString(TEXT("Dismount")) :
                Traversal == TEXT("Vehicle") ? FString(TEXT("Exit vehicle")) :
                Prompt.IsEmpty() ? FString(TEXT("Interact")) : Prompt.ToString();

            DrawRect(
                FLinearColor(0.04f, 0.05f, 0.06f, 0.84f),
                PromptX,
                PromptY,
                PromptW,
                PromptH);
            DrawFoundationText(
                PromptText,
                MainText,
                PromptX + 16.0f * Scale,
                PromptY + 10.0f * Scale,
                nullptr,
                0.62f * Scale,
                false);
            AddFoundationHitBox(
                FVector2D(PromptX, PromptY),
                FVector2D(PromptW, PromptH),
                FName(TEXT("OG.World.Interact")),
                true,
                105);
        }
    }

    DrawFoundationDiagnosticSurface(Character, Scale, false);
    if (Character)
    {
        const auto Caption = Character->FindComponentByClass<UOGSemanticAudioRuntime>()->GetCurrentCaption();
        if (Caption.bVisible)
        {
            const float CaptionScale = Caption.Presentation == TEXT("large") ? .82f : .6f;
            DrawRect(FLinearColor(.01f,.015f,.02f,Caption.Readability == TEXT("high_contrast") ? .98f : .80f),
                Canvas->ClipX * .20f, Canvas->ClipY * .83f, Canvas->ClipX * .60f, 58 * Scale);
            DrawFoundationText(Caption.Speaker.ToString() + TEXT(" ") + Caption.Text.ToString(), MainText,
                Canvas->ClipX * .22f, Canvas->ClipY * .85f, nullptr, CaptionScale * Scale);
        }
    }
}

void AOGWorldPresentationHud::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    // Modal controls never leak a second summon or a background navigation click.
    if (FoundationSurface == TEXT("ruler") && RulerDestination == TEXT("gacha") && !GachaOverlay.IsNone())
    {
        if (BoxName == TEXT("OG.Presentation.Close")) GachaOverlay = NAME_None;
        else if (BoxName == TEXT("OG.Presentation.RatesNext")) ++GachaDetailsPage;
        return;
    }
    if (BoxName == TEXT("OG.Presentation.Rates") || BoxName == TEXT("OG.Presentation.History"))
    {
        GachaOverlay = BoxName == TEXT("OG.Presentation.Rates") ? TEXT("rates") : TEXT("history");
        GachaDetailsPage = 0;
        return;
    }
    if (BoxName == TEXT("OG.Presentation.RosterPrevious") || BoxName == TEXT("OG.Presentation.RosterNext"))
    {
        RosterPage = FMath::Max(0, RosterPage + (BoxName == TEXT("OG.Presentation.RosterNext") ? 1 : -1));
        return;
    }
    if (BoxName == TEXT("OG.Presentation.TerritoryNext")) { ++TerritoryPage; return; }
    if (BoxName == TEXT("OG.Presentation.TurnNext")) { ++TurnPage; return; }
    if (BoxName == TEXT("OG.Presentation.MenuNext"))
    { DiagnosticMenuPage = (DiagnosticMenuPage + 1) % FMath::Max(1, DiagnosticMenuPageCount); return; }


    AOGWorldPrototypeCharacter* Character =
        PlayerOwner
            ? Cast<AOGWorldPrototypeCharacter>(PlayerOwner->GetPawn())
            : nullptr;
    if (!Character)
    {
        return;
    }

    if (FoundationSurface == FName(TEXT("opening")) &&
        OpeningPage == FName(TEXT("confirm_clear")) &&
        BoxName != FName(TEXT("OG.Opening.ClearCancel")) &&
        BoxName != FName(TEXT("OG.Opening.ClearConfirm")))
    {
        return;
    }

    if (HandleFoundationDiagnosticClick(BoxName, Character))
    { Character->RefreshFoundationInputState(); return; }
    Character->FindComponentByClass<UOGSemanticAudioRuntime>()->PlaySemanticEvent(TEXT("ui.activate"), Character->GetActorLocation());
    if (BoxName.ToString().StartsWith(TEXT("OG.Turn.")) &&
        Character->FindComponentByClass<UOGDiagnosticCombatComponent>()->IsInTurn())
    {
        if (auto* Combat = Character->FindComponentByClass<UOGDiagnosticCombatComponent>())
        {
            FString Error; bool bOk = false;
            if (BoxName == TEXT("OG.Turn.Return")) bOk = Combat->ReturnFromTurn(Error);
            else if (BoxName == TEXT("OG.Turn.Basic")) bOk = Combat->SubmitTurn(EOGDiagnosticCommand::Basic, Error);
            else if (BoxName == TEXT("OG.Turn.Skill1")) bOk = Combat->SubmitTurn(EOGDiagnosticCommand::Skill1, Error);
            else if (BoxName == TEXT("OG.Turn.Skill2")) bOk = Combat->SubmitTurn(EOGDiagnosticCommand::Skill2, Error);
            else if (BoxName == TEXT("OG.Turn.Ultimate")) bOk = Combat->SubmitTurn(EOGDiagnosticCommand::Ultimate, Error);
            if (!bOk) Combat->SetStatus(Error);
        }
        return;
    }
    if (FoundationSurface == TEXT("world") && Character->HandleWorldHudControl(BoxName, true))
    { PressedWorldControls.Add(BoxName); return; }


    if (BoxName == FName(TEXT("OG.Opening.Clear")))
    {
        OpeningPage = FName(TEXT("confirm_clear"));
        FoundationStatusMessage.Reset();
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.ClearCancel")))
    {
        OpeningPage = FName(TEXT("main"));
        FoundationStatusMessage.Reset();
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.ClearConfirm")))
    {
        if (FoundationSurface != FName(TEXT("opening")) ||
            OpeningPage != FName(TEXT("confirm_clear")))
        {
            return;
        }
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UOGGameCoreSubsystem* Core =
                    GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
            {
                FString Error;
                if (Core->ClearCanonicalWorld(false, Error))
                {
                    FoundationStatusMessage =
                        TEXT("World cleared safely; recovery copy preserved.");
                    UGameplayStatics::OpenLevel(
                        this,
                        FName(TEXT("StartingWorld")));
                }
                else
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Clear failed: %s"),
                            *Error);
                }
            }
        }
        return;
    }

    if (BoxName == FName(TEXT("OG.Recovery.CreateBackup")))
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UOGGameCoreSubsystem* Core =
                    GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
            {
                FString Path;
                FString Error;
                if (Core->CreateManualRecoverySnapshot(Path, Error))
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Backup created: %s"),
                            *FPaths::GetCleanFilename(Path));
                }
                else
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Backup failed: %s"),
                            *Error);
                }
            }
        }
        return;
    }

    if (BoxName == FName(TEXT("OG.Recovery.ExportCurrent")))
    {
        const FString Base =
            FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("OfflineGame"));
        const FString ExportDirectory =
            FPaths::Combine(Base, TEXT("ExportStaging"));
        IFileManager::Get().MakeDirectory(
            *ExportDirectory,
            true);
        const FString SuggestedName =
            FString::Printf(
                TEXT("OfflineGame_World_%s.db"),
                *FDateTime::UtcNow().ToString(
                    TEXT("%Y%m%dT%H%M%SZ")));
        const FString StagingPath =
            FPaths::Combine(
                ExportDirectory,
                SuggestedName);

        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UOGGameCoreSubsystem* Core =
                    GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
            {
                FString BackupId;
                FString Error;
                if (!Core->ExportCanonicalWorldBackup(
                        StagingPath,
                        BackupId,
                        Error))
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Export preparation failed: %s"),
                            *Error);
                    return;
                }

                if (!FOGAndroidBackupDocumentBridge::LaunchExport(
                        StagingPath,
                        SuggestedName,
                        Error))
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Document export failed: %s"),
                            *Error);
                    return;
                }

                FoundationStatusMessage =
                    TEXT("Choose an external backup location in Android Files.");
            }
        }
        return;
    }

    if (BoxName == FName(TEXT("OG.Recovery.PickImport")))
    {
        bPendingImportedBackupRestore = false;
        const FString Destination =
            FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("OfflineGame"),
                TEXT("Import"),
                TEXT("selected_import.db"));
        IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(Destination),
            true);
        IFileManager::Get().Delete(
            *Destination,
            false,
            true,
            true);

        FString Error;
        if (!FOGAndroidBackupDocumentBridge::LaunchImport(
                Destination,
                Error))
        {
            FoundationStatusMessage =
                FString::Printf(
                    TEXT("Document import failed: %s"),
                    *Error);
        }
        else
        {
            FoundationStatusMessage =
                TEXT("Choose a world backup in Android Files.");
        }
        return;
    }

    if (BoxName == FName(TEXT("OG.Recovery.RestorePickedImport")))
    {
        const FString Candidate =
            FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("OfflineGame"),
                TEXT("Import"),
                TEXT("selected_import.db"));

        if (!bPendingImportedBackupRestore)
        {
            bPendingImportedBackupRestore = true;
            FoundationStatusMessage =
                TEXT("Restore will replace the current world. Tap Confirm restore selected import.");
            return;
        }

        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UOGGameCoreSubsystem* Core =
                    GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
            {
                FString PreservedPath;
                FString Error;
                if (Core->ImportCanonicalWorldBackup(
                        Candidate,
                        PreservedPath,
                        Error))
                {
                    bPendingImportedBackupRestore = false;
                    FoundationStatusMessage =
                        TEXT("Imported backup validated and restored.");
                    UGameplayStatics::OpenLevel(
                        this,
                        FName(TEXT("StartingWorld")));
                }
                else
                {
                    bPendingImportedBackupRestore = false;
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Import restore failed: %s"),
                            *Error);
                }
            }
        }
        return;
    }

    {
        const FString Click = BoxName.ToString();
        const FString ExportPrefix(TEXT("OG.Recovery.Export."));
        const FString RestorePrefix(TEXT("OG.Recovery.Restore."));
        const bool bExport = Click.StartsWith(ExportPrefix);
        const bool bRestore = Click.StartsWith(RestorePrefix);
        if (bExport || bRestore)
        {
            const FString IndexText =
                Click.Mid(
                    bExport
                        ? ExportPrefix.Len()
                        : RestorePrefix.Len());
            const int32 Index = FCString::Atoi(*IndexText);
            const FString CatalogPath =
                FPaths::Combine(
                    FPaths::ProjectSavedDir(),
                    TEXT("OfflineGame"),
                    TEXT("RecoveryCatalog.json"));
            TArray<FOGBackupCatalogEntry> Entries;
            FString Error;
            if (!FOGRecoveryCatalogService::LoadEntries(
                    CatalogPath,
                    Entries,
                    Error) ||
                !Entries.IsValidIndex(Index))
            {
                FoundationStatusMessage =
                    Error.IsEmpty()
                        ? TEXT("Selected backup is no longer available.")
                        : Error;
                return;
            }

            const FOGBackupCatalogEntry& Entry = Entries[Index];
            if (bExport)
            {
                const FString SuggestedName =
                    FString::Printf(
                        TEXT("OfflineGame_Backup_%d.db"),
                        Index + 1);
                if (!FOGAndroidBackupDocumentBridge::LaunchExport(
                        Entry.BackupPathOrUri,
                        SuggestedName,
                        Error))
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Backup export failed: %s"),
                            *Error);
                }
                else
                {
                    FoundationStatusMessage =
                        TEXT("Choose an external backup location in Android Files.");
                }
                return;
            }

            if (PendingBackupRestoreIndex != Index)
            {
                PendingBackupRestoreIndex = Index;
                FoundationStatusMessage =
                    TEXT("Restore will replace the current world. Tap Confirm again.");
                return;
            }

            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UOGGameCoreSubsystem* Core =
                        GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
                {
                    FString PreservedPath;
                    if (Core->ImportCanonicalWorldBackup(
                            Entry.BackupPathOrUri,
                            PreservedPath,
                            Error))
                    {
                        PendingBackupRestoreIndex = INDEX_NONE;
                        FoundationStatusMessage =
                            TEXT("Backup validated and restored.");
                        UGameplayStatics::OpenLevel(
                            this,
                            FName(TEXT("StartingWorld")));
                    }
                    else
                    {
                        FoundationStatusMessage =
                            FString::Printf(
                                TEXT("Restore failed: %s"),
                                *Error);
                    }
                }
            }
            return;
        }
    }

    if (BoxName == FName(TEXT("OG.Gacha.Pull")) || BoxName == FName(TEXT("OG.Gacha.Pull10")))
    {
        if (!bHasActiveGachaPresentation)
        {
            FoundationStatusMessage =
                TEXT("No active gacha banner/command context.");
            return;
        }

        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UOGGameCoreSubsystem* Core =
                    GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
            {
                if (IOGWorldStore* Store = Core->GetWorldStore())
                {
                    FOGGachaService Gacha(*Store);
                    TArray<FOGGachaPullResult> Results;
                    const int32 Count = BoxName == TEXT("OG.Gacha.Pull10") ? 10 : 1;
                    FString Error;
                    if (Gacha.PullBatch(
                            ActiveGachaBanner,
                            GetFoundationRulerPresentationOwner(),
                            GachaCommandWorldTick,
                            GachaCommandSeed,
                            Count,
                            Results,
                            Error))
                    {
                        LastGachaResults = Results;
                        GachaOverlay = TEXT("results");
                        GachaDetailsPage = 0;
                        for (const auto& Result : Results)
                            GachaCommandSeed = static_cast<int64>(static_cast<uint64>(GachaCommandSeed) +
                                static_cast<uint64>(FMath::Max<int64>(1, Result.RngDrawCount + 1)));
                        FoundationStatusMessage = FString::Printf(
                            TEXT("Summoned %d character%s. Results below."),
                            Count, Count == 1 ? TEXT("") : TEXT("s"));
                    }
                    else
                    {
                        FoundationStatusMessage =
                            FString::Printf(
                                TEXT("Pull failed: %s"),
                                *Error);
                    }
                }
            }
        }
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.Continue")))
    {
        OpeningPage = FName(TEXT("main"));
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("world")));
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.Ruler")))
    {
        RulerDestination = FName(TEXT("home"));
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("ruler")));
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.Recover")))
    {
        OpeningPage = FName(TEXT("recover"));
        RulerDestination = FName(TEXT("records"));
        RecordsDestination = FName(TEXT("recovery"));
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("ruler")));
        return;
    }

    if (BoxName == FName(TEXT("OG.Opening.Import")))
    {
        OpeningPage = FName(TEXT("import"));
        RulerDestination = FName(TEXT("records"));
        RecordsDestination = FName(TEXT("recovery"));
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("ruler")));
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Ruler")))
    {
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("ruler")));
        return;
    }

    if (BoxName == FName(TEXT("OG.Ruler.World")))
    {
        Character->SetFoundationSettingsOpen(false);
        Character->SetFoundationPauseMenuOpen(false);
        SetFoundationSurface(FName(TEXT("world")));
        return;
    }

    const TCHAR* RulerDestinations[] =
    {
        TEXT("home"),
        TEXT("characters"),
        TEXT("gacha"),
        TEXT("territory"),
        TEXT("records")
    };
    for (const TCHAR* Destination : RulerDestinations)
    {
        if (BoxName == FName(*FString::Printf(
                TEXT("OG.Ruler.%s"),
                Destination)))
        {
            RulerDestination = FName(Destination);
            if (RulerDestination == FName(TEXT("records")))
            {
                RecordsDestination = FName(TEXT("hub"));
            }
            return;
        }
    }

    if (BoxName == TEXT("OG.Characters.Assign.1") || BoxName == TEXT("OG.Characters.Assign.2"))
    {
        auto* Core = GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
        auto* Combat = Character->FindComponentByClass<UOGDiagnosticCombatComponent>();
        TArray<FOGRosterIdentityViewModel> Roster; FString Error;
        if (Core && Core->GetWorldStore() && Combat &&
            FOGUiViewModelService(*Core->GetWorldStore()).BuildRoster(GetFoundationRulerPresentationOwner(), Roster, Error) &&
            Roster.IsValidIndex(SelectedRosterIndex) && Roster[SelectedRosterIndex].Manifestations.IsValidIndex(SelectedManifestationIndex))
        {
            const auto& Selected = Roster[SelectedRosterIndex].Manifestations[SelectedManifestationIndex];
            if (Combat->DeployManifestation(Selected.ManifestationId, BoxName == TEXT("OG.Characters.Assign.1") ? 1 : 2, Error))
                FoundationStatusMessage = TEXT("Companion assignment saved.");
            else FoundationStatusMessage = Error;
        }
        else FoundationStatusMessage = Error.IsEmpty() ? TEXT("Select an owned Manifestation first.") : Error;
        return;
    }

    if (BoxName == TEXT("OG.Characters.Previous") || BoxName == TEXT("OG.Characters.Next"))
    {
        ManifestationPage = FMath::Max(0, ManifestationPage + (BoxName == TEXT("OG.Characters.Next") ? 1 : -1));
        return;
    }
    if (BoxName == FName(TEXT("OG.Characters.Back")))
    {
        SelectedRosterIndex = INDEX_NONE;
        SelectedManifestationIndex = INDEX_NONE;
        return;
    }

    {
        const FString Click = BoxName.ToString();
        const FString IdentityPrefix(TEXT("OG.Characters.Identity."));
        const FString ManifestationPrefix(TEXT("OG.Characters.Manifestation."));
        const FString TerritoryPrefix(TEXT("OG.Territory."));
        const FString ReportPrefix(TEXT("OG.Reports.Acknowledge."));

        if (Click.StartsWith(IdentityPrefix))
        {
            SelectedRosterIndex =
                FCString::Atoi(
                    *Click.Mid(IdentityPrefix.Len()));
            SelectedManifestationIndex = INDEX_NONE;
            ManifestationPage = 0;
            return;
        }
        if (Click.StartsWith(ManifestationPrefix))
        {
            SelectedManifestationIndex =
                FCString::Atoi(
                    *Click.Mid(ManifestationPrefix.Len()));
            return;
        }
        if (Click.StartsWith(TerritoryPrefix))
        {
            SelectedTerritoryIndex =
                FCString::Atoi(
                    *Click.Mid(TerritoryPrefix.Len()));
            return;
        }
        if (Click.StartsWith(ReportPrefix))
        {
            const int32 ReportIndex =
                FCString::Atoi(
                    *Click.Mid(ReportPrefix.Len()));
            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UOGGameCoreSubsystem* Core =
                        GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
                {
                    if (IOGWorldStore* Store = Core->GetWorldStore())
                    {
                        TArray<FOGReportRecord> CurrentReports;
                        FString Error;
                        if (Store->ListReportsByOwner(
                                GetFoundationRulerPresentationOwner(),
                                CurrentReports,
                                Error) &&
                            CurrentReports.IsValidIndex(ReportIndex))
                        {
                            FOGReportService ReportsService(*Store);
                            const FOGReportRecord& Report =
                                CurrentReports[ReportIndex];
                            if (ReportsService.AcknowledgeReport(
                                    Report.ReportId,
                                    Report.CreatedWorldTick,
                                    Error))
                            {
                                FoundationStatusMessage =
                                    TEXT("Report acknowledged.");
                            }
                            else
                            {
                                FoundationStatusMessage =
                                    FString::Printf(
                                        TEXT("Report acknowledgement failed: %s"),
                                        *Error);
                            }
                        }
                    }
                }
            }
            return;
        }
    }

    const TCHAR* RecordDestinations[] =
    {
        TEXT("hub"),
        TEXT("reports"),
        TEXT("chronicle"),
        TEXT("codex"),
        TEXT("intelligence"),
        TEXT("recovery"),
        TEXT("packages")
    };
    for (const TCHAR* Destination : RecordDestinations)
    {
        if (BoxName == FName(*FString::Printf(
                TEXT("OG.Records.%s"),
                Destination)))
        {
            RecordsDestination = FName(Destination);
            return;
        }
    }

    if (BoxName == TEXT("OG.Settings.Rows.Previous") || BoxName == TEXT("OG.Settings.Rows.Next"))
    {
        SettingsRowPage = FMath::Clamp(SettingsRowPage +
            (BoxName == TEXT("OG.Settings.Rows.Next") ? 1 : -1), 0, SettingsLastRowPage);
        return;
    }
    if (BoxName.ToString().StartsWith(TEXT("OG.Settings.Page."))) SettingsRowPage = 0;

    if (BoxName == FName(TEXT("OG.Settings.Toggle")) ||
        BoxName == FName(TEXT("OG.Pause.Settings")))
    {
        if (Character->IsFoundationPauseMenuOpen())
            Character->SetFoundationPauseMenuOpen(false);
        Character->SetFoundationSettingsOpen(
            !Character->IsFoundationSettingsOpen());
        SetFoundationSurface(FoundationSurface);
        return;
    }

    if (BoxName == FName(TEXT("OG.Pause.Toggle")) ||
        BoxName == FName(TEXT("OG.Pause.Resume")))
    {
        Character->SetFoundationPauseMenuOpen(
            !Character->IsFoundationPauseMenuOpen());
        return;
    }

    // Privacy/SFW is intentionally available from the pause surface as well as
    // Settings, so it must not be gated on the settings panel being open.
    if (BoxName == FName(TEXT("OG.Settings.SFW")))
    {
        Character->ToggleSfwPresentation();
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Interact")))
    {
        Character->TriggerContextInteract();
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Aim")))
    {
        Character->ToggleManualAimFromHud();
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Descend")))
    {
        Character->TriggerTraversalDownPulse();
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Skill.0")))
    {
        Character->TriggerSkillFromHud(0);
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Skill.1")))
    {
        Character->TriggerSkillFromHud(1);
        return;
    }

    if (BoxName == FName(TEXT("OG.World.Ultimate")))
    {
        Character->TriggerUltimateFromHud();
        return;
    }

    {
        const FString Click = BoxName.ToString();
        const FString StatusPrefix(TEXT("OG.Target.Status."));
        if (Click.StartsWith(StatusPrefix))
        {
            const int32 Index =
                FCString::Atoi(
                    *Click.Mid(StatusPrefix.Len()));
            ExpandedTargetStatusIndex =
                ExpandedTargetStatusIndex == Index
                    ? INDEX_NONE
                    : Index;
            return;
        }
    }

    for (int32 SlotIndex = 0; SlotIndex < 3; ++SlotIndex)
    {
        if (BoxName == FName(*FString::Printf(
                TEXT("OG.Party.Switch.%d"),
                SlotIndex)))
        {
            Character->TriggerPartySwitchFromHud(SlotIndex);
            return;
        }

        if (BoxName == FName(*FString::Printf(
                TEXT("OG.Party.QTE.%d"),
                SlotIndex)))
        {
            Character->TriggerQteFromHud(SlotIndex);
            return;
        }
    }

    {
        const FString Click = BoxName.ToString();
        const FString MovePrefix(TEXT("OG.Package.Move."));
        const FString ArchivePrefix(TEXT("OG.Package.Archive."));
        const bool bMove = Click.StartsWith(MovePrefix);
        const bool bArchive = Click.StartsWith(ArchivePrefix);
        if (bMove || bArchive)
        {
            const FString& Prefix =
                bMove ? MovePrefix : ArchivePrefix;
            const int32 Index =
                FCString::Atoi(*Click.Mid(Prefix.Len()));
            if (!CachedPackageEntries.IsValidIndex(Index))
            {
                FoundationStatusMessage =
                    TEXT("Package action target is no longer available.");
                return;
            }

            const FOGPackageStorageEntryViewModel Entry =
                CachedPackageEntries[Index];

            if (bArchive)
            {
                FString Error;
                FString SuggestedName =
                    FPaths::GetCleanFilename(Entry.InstallUri);
                if (SuggestedName.IsEmpty())
                {
                    SuggestedName =
                        FPaths::MakeValidFileName(
                            Entry.PackageId.ToString()) +
                        TEXT(".ogpkg");
                }

                if (FOGAndroidBackupDocumentBridge::LaunchExport(
                        Entry.InstallUri,
                        SuggestedName,
                        Error))
                {
                    FoundationStatusMessage =
                        TEXT("Choose an archive destination in Android Files.");
                }
                else
                {
                    FoundationStatusMessage =
                        FString::Printf(
                            TEXT("Package archive failed: %s"),
                            *Error);
                }
                return;
            }

            if (UGameInstance* GameInstance = GetGameInstance())
            {
                if (UOGGameCoreSubsystem* Core =
                        GameInstance->GetSubsystem<UOGGameCoreSubsystem>())
                {
                    if (IOGWorldStore* Store = Core->GetWorldStore())
                    {
                        const bool bMoveToExternal =
                            Entry.StorageClass !=
                            FName(TEXT("managed_external"));
                        const FString DestinationRoot =
                            bMoveToExternal
                                ? FPaths::Combine(
                                    FPaths::ProjectPersistentDownloadDir(),
                                    TEXT("OfflineGame"),
                                    TEXT("Packages"))
                                : FPaths::Combine(
                                    FPaths::ProjectSavedDir(),
                                    TEXT("OfflineGame"),
                                    TEXT("Packages"));
                        const FName DestinationClass =
                            bMoveToExternal
                                ? FName(TEXT("managed_external"))
                                : FName(TEXT("local_hot"));

                        FOGPackageManagerService PackageManager(*Store);
                        FString NewInstallUri;
                        FString Error;
                        if (PackageManager.MovePackageStorage(
                                Entry.PackageId,
                                DestinationRoot,
                                DestinationClass,
                                NewInstallUri,
                                Error))
                        {
                            FoundationStatusMessage =
                                FString::Printf(
                                    TEXT("Package moved to %s."),
                                    *DestinationClass.ToString());
                            CachedPackageEntries.Reset();
                        }
                        else
                        {
                            // A verified destination may already be canonical even
                            // when old-file cleanup fails. Refresh that actual state.
                            if (!NewInstallUri.IsEmpty()) CachedPackageEntries.Reset();
                            FoundationStatusMessage =
                                FString::Printf(
                                    TEXT("Package move failed: %s"),
                                    *Error);
                        }
                    }
                }
            }
            return;
        }
    }

    if (!Character->IsFoundationSettingsOpen())
    {
        return;
    }

    if (BoxName == FName(TEXT("OG.Settings.Page.Controls")))
    {
        Character->SetFoundationSettingsPage(0);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Page.Accessibility")))
    {
        Character->SetFoundationSettingsPage(1);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Page.Audio")))
    {
        Character->SetFoundationSettingsPage(2);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Page.Content")))
    {
        Character->SetFoundationSettingsPage(3);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Orientation")))
    {
        Character->CycleOrientationOverride();
    }
    else if (BoxName == FName(TEXT("OG.Settings.CameraH.Minus")))
    {
        Character->AdjustCameraHorizontalSensitivity(-0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.CameraH.Plus")))
    {
        Character->AdjustCameraHorizontalSensitivity(0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.CameraV.Minus")))
    {
        Character->AdjustCameraVerticalSensitivity(-0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.CameraV.Plus")))
    {
        Character->AdjustCameraVerticalSensitivity(0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Response.Minus")))
    {
        Character->AdjustCameraResponseExponent(-0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.Response.Plus")))
    {
        Character->AdjustCameraResponseExponent(0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.InvertX")))
    {
        Character->ToggleInvertCameraX();
    }
    else if (BoxName == FName(TEXT("OG.Settings.InvertY")))
    {
        Character->ToggleInvertCameraY();
    }
    else if (BoxName == FName(TEXT("OG.Settings.Sprint")))
    {
        Character->ToggleSprintPreference();
    }
    else if (BoxName == FName(TEXT("OG.Settings.Handedness")))
    {
        Character->ToggleLeftHandedControls();
    }
    else if (BoxName == FName(TEXT("OG.Settings.TouchScale.Minus")))
    {
        Character->AdjustTouchControlScale(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.TouchScale.Plus")))
    {
        Character->AdjustTouchControlScale(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.TouchOpacity.Minus")))
    {
        Character->AdjustTouchControlOpacity(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.TouchOpacity.Plus")))
    {
        Character->AdjustTouchControlOpacity(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveDeadzone.Minus")))
    {
        Character->AdjustMovementDeadzone(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveDeadzone.Plus")))
    {
        Character->AdjustMovementDeadzone(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.LookDeadzone.Minus")))
    {
        Character->AdjustLookDeadzone(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.LookDeadzone.Plus")))
    {
        Character->AdjustLookDeadzone(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveInset.Minus")))
    {
        Character->AdjustMovementStickInset(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveInset.Plus")))
    {
        Character->AdjustMovementStickInset(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveBottom.Minus")))
    {
        Character->AdjustMovementStickBottom(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MoveBottom.Plus")))
    {
        Character->AdjustMovementStickBottom(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.ActionX.Minus")))
    {
        Character->AdjustActionClusterHorizontalOffset(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.ActionX.Plus")))
    {
        Character->AdjustActionClusterHorizontalOffset(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.ActionY.Minus")))
    {
        Character->AdjustActionClusterVerticalOffset(-0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.ActionY.Plus")))
    {
        Character->AdjustActionClusterVerticalOffset(0.01f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.ReducedMotion")))
    {
        Character->ToggleReducedMotion();
    }
    else if (BoxName == FName(TEXT("OG.Settings.ReducedShake")))
    {
        Character->ToggleReducedCameraShake();
    }
    else if (BoxName == FName(TEXT("OG.Settings.Subtitles")))
    {
        Character->ToggleSubtitles();
    }
    else if (BoxName == FName(TEXT("OG.Settings.SubtitlePresentation")))
    {
        Character->CycleSubtitlePresentation();
    }
    else if (BoxName == FName(TEXT("OG.Settings.Readability")))
    {
        Character->CycleUiReadabilityProfile();
    }
    else if (BoxName == FName(TEXT("OG.Settings.ColorVision")))
    {
        Character->CycleColorVisionProfile();
    }
    else if (BoxName == FName(TEXT("OG.Settings.Haptics")))
    {
        Character->ToggleHaptics();
    }
    else if (BoxName == FName(TEXT("OG.Settings.HapticsIntensity.Minus")))
    {
        Character->AdjustHapticsIntensity(-0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.HapticsIntensity.Plus")))
    {
        Character->AdjustHapticsIntensity(0.10f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.DamageNumbers")))
    {
        Character->CycleDamageNumberPresentation();
    }
    else if (BoxName == FName(TEXT("OG.Settings.MasterVolume.Minus")))
    {
        Character->AdjustMasterVolume(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MasterVolume.Plus")))
    {
        Character->AdjustMasterVolume(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MusicVolume.Minus")))
    {
        Character->AdjustMusicVolume(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.MusicVolume.Plus")))
    {
        Character->AdjustMusicVolume(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.VoiceVolume.Minus")))
    {
        Character->AdjustVoiceVolume(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.VoiceVolume.Plus")))
    {
        Character->AdjustVoiceVolume(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.SfxVolume.Minus")))
    {
        Character->AdjustSfxVolume(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.SfxVolume.Plus")))
    {
        Character->AdjustSfxVolume(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.AmbienceVolume.Minus")))
    {
        Character->AdjustAmbienceVolume(-0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.AmbienceVolume.Plus")))
    {
        Character->AdjustAmbienceVolume(0.05f);
    }
    else if (BoxName == FName(TEXT("OG.Settings.DynamicRange")))
    {
        Character->CycleDynamicRangeProfile();
    }
    else if (BoxName == FName(TEXT("OG.Settings.RosterDensity")))
    {
        Character->CycleRosterDensity();
    }
    else if (BoxName == FName(TEXT("OG.Settings.CinematicPolicy")))
    {
        Character->CycleCinematicRepeatPolicy();
    }
    else if (BoxName == FName(TEXT("OG.Settings.AutoDownload")))
    {
        Character->ToggleAutoDownload();
    }
    else if (BoxName == FName(TEXT("OG.Settings.DownloadNetwork")))
    {
        Character->ToggleLargeDownloadsUnmeteredOnly();
    }
}


// Add public native declaration void RestoreDiagnosticHardLock(AActor* Target);
void AOGWorldPrototypeCharacter::RestoreDiagnosticHardLock(AActor* Target)
{
    const bool bEligible = Target && Target->GetClass()->ImplementsInterface(UOGWorldTargetable::StaticClass()) &&
        IOGWorldTargetable::Execute_CanBeTargeted(Target, this);
    HardLockedTarget = bEligible ? Target : nullptr;
    OnHardLockChanged(HardLockedTarget.Get());
}


void AOGWorldPrototypeCharacter::InitializeFoundationIntegration()
{
    if (!GetWorld() || !GetGameInstance()) return;
    if (IntegrationWorld && DiagnosticCombat->IsActive()) return;
    auto* Core = GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    if (!Core || !Core->GetWorldStore() || Core->GetCanonicalWorldTick() < 0)
    {
        UE_LOG(LogOfflineGame, Error, TEXT("Foundation integration requires the canonical store and clock."));
        return;
    }
    GetWorld()->GetWorldSettings()->bEnableWorldBoundsChecks = false;
    if (DiagnosticAnimations) DiagnosticAnimations->BindMesh(GetMesh());
    bHasDiagnosticFacingSample = false;
    if (!IntegrationWorld) IntegrationWorld = GetWorld()->SpawnActor<AOGFoundationIntegrationWorld>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!IntegrationWorld) return;
    for (TActorIterator<AOGStartingRegionGenerator> It(GetWorld()); It; ++It)
        It->SetFoundationIntegrationBoundary(IntegrationWorld);
    TWeakObjectPtr<UOGGameCoreSubsystem> WeakCore(Core);
    if (!bIntegrationCourseBuilt)
    {
        IntegrationWorld->Build(this, [WeakCore]() -> int64
        { return WeakCore.IsValid() ? WeakCore->GetCanonicalWorldTick() : -1; });
        bIntegrationCourseBuilt = true;
    }
    GroundSnapAttempts = 12; // Cancel pending generated-terrain startup retries.
    ResetTraversalForRelocation();
    SetActorLocation(FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4));
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    FString Error;
    if (!DiagnosticCombat->StartScenario(*Core->GetWorldStore(),
        [WeakCore](int64& Tick, FString& OutError)
        {
            Tick = WeakCore.IsValid() ? WeakCore->GetCanonicalWorldTick() : -1;
            if (Tick < 0) { OutError = TEXT("Canonical clock unavailable."); return false; }
            return true;
        }, IntegrationWorld->GetActorLocation(), Error))
        UE_LOG(LogOfflineGame, Error, TEXT("Foundation combat integration: %s"), *Error);
    DiagnosticMenu->Initialize();
    InteractionHost->Initialize();
    if (auto* Presentation = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
        Presentation->RegisterWorldPresentationActor(IntegrationWorld);
}

void AOGWorldPrototypeCharacter::PresentAcceptedCombatAction(uint8 Command, int32 Chain)
{
    FOGWorldActionState State;
    State.ActionId = Command == static_cast<uint8>(EOGDiagnosticCommand::Basic)
        ? FName(TEXT("foundation.primary_attack")) : FName(TEXT("foundation.diagnostic_skill"));
    FOGActionCancelWindow Window;
    Window.OpensAtSeconds = .08f; Window.ClosesAtSeconds = .28f;
    Window.Destinations = {EOGActionCancelDestination::Dodge, EOGActionCancelDestination::Jump,
        EOGActionCancelDestination::Switch, EOGActionCancelDestination::Skill, EOGActionCancelDestination::Ultimate};
    State.CancelWindows.Add(Window);
    State.bAllowWhileAirborne = State.bAllowWhileSwimming = State.bAllowWhileDiving =
        State.bAllowWhileFlying = State.bAllowWhileMounted = true;
    State.bAllowWhileClimbing = false;
    if (ActionRuntime) ActionRuntime->BeginAuthoredAction(State);
    AttackVisualEndsAtSeconds = GetWorld()->GetTimeSeconds() + .32;
    const FName Pose = Command == static_cast<uint8>(EOGDiagnosticCommand::Basic)
        ? FName(*FString::Printf(TEXT("Attack%d"), Chain + 1))
        : Command == static_cast<uint8>(EOGDiagnosticCommand::Ultimate) ? FName(TEXT("Ultimate")) : FName(TEXT("Skill"));
    DiagnosticAnimations->PresentTimedAction(Pose, .32f);
    if (bHapticsEnabled)
        if (auto* PC = Cast<APlayerController>(Controller))
            PC->PlayDynamicForceFeedback(HapticsIntensity * .22f, .035f, true, true, true, true);
    SemanticAudio->PlaySemanticEvent(TEXT("gameplay.attack"), GetActorLocation());
}

void AOGWorldPrototypeCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    if (DiagnosticAnimations) DiagnosticAnimations->PresentTimedAction(TEXT("Land"), .22f);
}

bool AOGWorldPrototypeCharacter::HandleWorldHudControl(FName Name, bool bPressed)
{
    if (bPressed && !IsFoundationWorldInputAllowed() &&
        Name.ToString().StartsWith(TEXT("OG.World."))) return true;
    if (Name == TEXT("OG.World.EndFlight"))
    {
        if (bPressed)
        {
            if (!IntegrationWorld || !IntegrationWorld->CancelFlight(this)) EndFlight();
        }
        return true;
    }
    if (bPressed && DiagnosticCombat && DiagnosticCombat->IsWorldDefeated() &&
        Name.ToString().StartsWith(TEXT("OG.World."))) return true;
    if (Name == TEXT("OG.World.Jump")) { if (bPressed) JumpPressed(); else JumpReleased(); return true; }
    if (Name == TEXT("OG.World.Sprint")) { if (bPressed) SprintPressed(); else SprintReleased(); return true; }
    if (Name == TEXT("OG.World.Descend")) { if (bPressed) TraversalDownPressed(); else TraversalDownReleased(); return true; }
    if (Name == TEXT("OG.World.Attack")) { if (bPressed) PrimaryAttackPressed(); return true; }
    if (Name == TEXT("OG.World.Dodge")) { if (bPressed) DodgePressed(); return true; }
    if (Name == TEXT("OG.World.Lock")) { if (bPressed) ToggleLockOn(); return true; }
    return false;
}

void AOGWorldPresentationHud::NotifyHitBoxRelease(FName BoxName)
{
    Super::NotifyHitBoxRelease(BoxName);
    PressedWorldControls.Remove(BoxName);
    if (auto* Character = PlayerOwner ? Cast<AOGWorldPrototypeCharacter>(PlayerOwner->GetPawn()) : nullptr)
        Character->HandleWorldHudControl(BoxName, false);
}


float AOGWorldPrototypeCharacter::GetFoundationHealth() const
{
    if (!DiagnosticCombat || !DiagnosticCombat->IsActive()) return FoundationHealth;
    if (!PartyRuntime || !PartyRuntime->GetSlots().IsValidIndex(PartyRuntime->GetControlledSlot())) return 0.0f;
    const auto& Unit = PartyRuntime->GetSlots()[PartyRuntime->GetControlledSlot()].Unit;
    if (Unit.CurrentHp.GetSign() <= 0 || Unit.Stats.MaxHp.GetSign() <= 0) return 0;
    const int64 Difference = static_cast<int64>(Unit.CurrentHp.Exponent10) - Unit.Stats.MaxHp.Exponent10;
    if (Difference < -12) return 0;
    if (Difference > 12) return FoundationMaxHealth;
    const double Ratio = static_cast<double>(Unit.CurrentHp.Significand) / Unit.Stats.MaxHp.Significand * FMath::Pow(10.0, static_cast<double>(Difference));
    return static_cast<float>(FMath::Clamp(Ratio, 0.0, 1.0)) * FoundationMaxHealth;
}

FString AOGWorldPrototypeCharacter::GetCanonicalHealthText() const
{
    return DiagnosticCombat && DiagnosticCombat->IsActive()
        ? FString(TEXT("HP ")) + DiagnosticCombat->GetHpText()
        : FString::Printf(TEXT("HP %.0f / %.0f"), FoundationHealth, FoundationMaxHealth);
}

void AOGWorldPrototypeCharacter::UpdateDiagnosticAnimationState()
{
    if (!DiagnosticAnimations || !GetCharacterMovement() || (InteractionHost && InteractionHost->IsActive())) return;
    FName State = TEXT("Idle");
    const float FacingYaw = GetActorRotation().Yaw;
    const float FacingRate = bHasDiagnosticFacingSample && GetWorld()
        ? FMath::Abs(FMath::FindDeltaAngleDegrees(LastDiagnosticFacingYaw, FacingYaw)) /
            FMath::Max(GetWorld()->GetDeltaSeconds(), .001f) : 0.0f;
    LastDiagnosticFacingYaw = FacingYaw;
    bHasDiagnosticFacingSample = true;
    const auto* Movement = GetCharacterMovement();
    if (DiagnosticCombat && DiagnosticCombat->IsActive() && DiagnosticCombat->IsWorldDefeated()) State = TEXT("Death");
    else if (TraversalCapabilities)
    {
        switch (TraversalCapabilities->GetTraversalMode())
        {
        case EOGTraversalMode::Climbing: State = TEXT("Climb"); break;
        case EOGTraversalMode::Swimming: State = TEXT("Swim"); break;
        case EOGTraversalMode::Diving: State = TEXT("Dive"); break;
        case EOGTraversalMode::Flying: State = TEXT("Fly"); break;
        case EOGTraversalMode::Mounted: State = TEXT("Mount"); break;
        case EOGTraversalMode::Vehicle: State = TEXT("Vehicle"); break;
        default:
            if (Movement->IsFalling()) State = GetVelocity().Z > 0 ? TEXT("Jump") : TEXT("Fall");
            else if (GetVelocity().SizeSquared2D() > 16.0f) State = bSprinting ? TEXT("Sprint") : GetVelocity().Size2D() > 280.0f ? TEXT("Run") : TEXT("Walk");
            if (!Movement->IsFalling() && GetVelocity().Size2D() < 280.0f && FacingRate > 45.0f)
                State = TEXT("Turn");
            break;
        }
    }
    DiagnosticAnimations->SetPresentedState(State);
}

void AOGWorldPrototypeCharacter::UpdateProtagonistRepresentation(const FOGEntityId& Entity, const FOGContentId& Identity)
{
    if (!DiagnosticCombat || !DiagnosticCombat->IsActive() || !GetWorld()) return;
    FOGFoundationCharacterContext Context;
    Context.EntityId = Entity; Context.OwnerEntityId = DiagnosticCombat->GetRulerId();
    FOGEntityId Controlled; FOGContentId CurrentIdentity; bool bCopy = false;
    FOGCharacterManifestationRecord Copy; FString PresentationError;
    if (DiagnosticCombat->TryGetControlledContext(Controlled, CurrentIdentity, bCopy, Copy, PresentationError) && bCopy) Context.ManifestationId = Entity;
    RefreshCanonicalPresentation(Context);
    if (Entity == DiagnosticCombat->GetRulerId())
    {
        if (ProtagonistRepresentation)
        {
            const FTransform ReturningBody = ProtagonistRepresentation->GetActorTransform();
            ResetTraversalForRelocation();
            SetActorTransform(ReturningBody, false, nullptr, ETeleportType::TeleportPhysics);
            ProtagonistRepresentation->Destroy(); ProtagonistRepresentation = nullptr;
        }
        DiagnosticCombat->SetProtagonistWorldActor(this);
        return;
    }
    if (!ProtagonistRepresentation)
    {
        FActorSpawnParameters Spawn;
        Spawn.Owner = this; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ProtagonistRepresentation = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), GetActorTransform(), Spawn);
        if (!ProtagonistRepresentation) return;
        ProtagonistRepresentation->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        ProtagonistRepresentation->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        auto* Body = ProtagonistRepresentation->GetMesh();
        Body->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
        Body->SetRelativeTransform(GetMesh()->GetRelativeTransform());
        Body->SetAnimInstanceClass(GetMesh()->GetAnimClass());
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ProtagonistRepresentation->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        auto* Presenter = NewObject<UOGDiagnosticAnimationPresentation>(ProtagonistRepresentation);
        ProtagonistRepresentation->AddInstanceComponent(Presenter); Presenter->RegisterComponent();
        Presenter->BindMesh(Body); Presenter->SetPresentedState(TEXT("Idle"));
        auto* Canonical = NewObject<UOGCanonicalCharacterPresentation>(ProtagonistRepresentation);
        ProtagonistRepresentation->AddInstanceComponent(Canonical); Canonical->RegisterComponent();
        Canonical->bUseNeutralDiagnosticTint = true;
        FOGFoundationCharacterContext RulerContext; RulerContext.EntityId = DiagnosticCombat->GetRulerId(); RulerContext.OwnerEntityId = RulerContext.EntityId;
        Canonical->Configure(RulerContext, Body, FName(*(FString(TEXT("rig:")) + Body->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName().ToLower())), PresentationError);
        Canonical->SetPrivacyPresentation(bSfwPresentation);
        if (auto* Mode = GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
            Mode->RegisterWorldPresentationActor(ProtagonistRepresentation);
        DiagnosticCombat->RegisterScenarioActor(ProtagonistRepresentation);
    }
    DiagnosticCombat->SetProtagonistWorldActor(ProtagonistRepresentation);
}

void AOGWorldPrototypeCharacter::ShutdownFoundationRuntime()
{
    if (InteractionHost) InteractionHost->Cancel(TEXT("canonical_store_release"));
    if (DiagnosticAnimations) DiagnosticAnimations->UnbindMesh();
    if (CanonicalPresentation) CanonicalPresentation->ClearBinding();
    // Fixtures hold the current canonical store. Destroy them at its lifetime
    // boundary rather than retaining old store references after import/clear.
    if (IntegrationWorld) { IntegrationWorld->Destroy(); IntegrationWorld = nullptr; }
    bIntegrationCourseBuilt = false;
    bHasDiagnosticFacingSample = false;
    if (DiagnosticCombat) DiagnosticCombat->ReleaseCanonicalStore();
    if (ProtagonistRepresentation) { ProtagonistRepresentation->Destroy(); ProtagonistRepresentation = nullptr; }
    if (SemanticAudio) SemanticAudio->CancelOwnedPresentation();
}

void AOGWorldPrototypeCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bOwnsFoundationInputSuppression)
        if (auto* PC = FoundationInputController.Get())
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
        }
    bOwnsFoundationInputSuppression = false;
    FoundationInputController.Reset();
    if (auto* GI = GetGameInstance())
        if (auto* Core = GI->GetSubsystem<UOGGameCoreSubsystem>()) Core->OnCanonicalRuntimeReleasing.RemoveAll(this);
    ShutdownFoundationRuntime();
    if (OwnedDynamicRangeMix) UGameplayStatics::PopSoundMixModifier(this, OwnedDynamicRangeMix);
    OwnedDynamicRangeMix = nullptr;
    Super::EndPlay(Reason);
}

bool AOGWorldPresentationHud::DrawCanonicalTurnSurface(AOGWorldPrototypeCharacter* Character, float Scale)
{
    auto* Combat = Character ? Character->FindComponentByClass<UOGDiagnosticCombatComponent>() : nullptr;
    if (!Combat || !Combat->IsInTurn()) return false;
    const float S=FMath::Min(Canvas->ClipX,Canvas->ClipY)/1080.f;
    const float W=Canvas->ClipX,H=Canvas->ClipY,M=32*S;
    const FLinearColor White(.94f,.94f,.89f,1),Gold(.83f,.69f,.4f,1),Teal(.38f,.72f,.75f,1);
    DrawGameBackdrop(0,0,W,H);
    DrawGameText(TEXT("Battle"),M,28*S,48*S,White,W*.5f);
    DrawGameText(FString::Printf(TEXT("Energy %d"),Combat->GetControlledEnergy()),W-280*S,38*S,32*S,Gold,248*S);
    DrawGameText(Combat->IsWaitingForTurnPlayer()?TEXT("Choose your action"):Combat->GetStatus(),
        M,100*S,32*S,White,W-2*M);
    const auto& Units=Combat->GetTurnState().Units;
    const int32 Cols=H>W?2:4,PerPage=Cols*2;
    TurnPage=TurnPage%FMath::Max(1,FMath::DivideAndRoundUp(Units.Num(),PerPage));
    const float Gap=24*S,CW=(W-2*M-(Cols-1)*Gap)/Cols,CH=(H-360*S-Gap)/2;
    for(int32 I=TurnPage*PerPage;I<Units.Num() && I<(TurnPage+1)*PerPage;++I)
    {
        const auto& Unit=Units[I];const int32 Slot=I-TurnPage*PerPage;
        float X=M+(Slot%Cols)*(CW+Gap),Y=170*S+(Slot/Cols)*(CH+Gap);
        DrawFoundationCharacterStandIn(X,Y,CW,CH,Unit.IsAlive()?Teal:FLinearColor(.3f,.32f,.35f,1));
        DrawGameText(FoundationIdentityLabel(Unit.IdentityId),X+18*S,Y+16*S,32*S,White,CW-36*S);
        const int64 Difference=static_cast<int64>(Unit.CurrentHp.Exponent10)-Unit.Stats.MaxHp.Exponent10;
        double Ratio=0;
        if(Unit.CurrentHp.Significand>0 && Unit.Stats.MaxHp.Significand>0)
            Ratio=Difference>16?1.0:Difference<-16?0.0:
                static_cast<double>(Unit.CurrentHp.Significand)/Unit.Stats.MaxHp.Significand*FMath::Pow(10.0,static_cast<double>(Difference));
        DrawRect(FLinearColor(.02f,.03f,.045f,1),X+18*S,Y+CH-74*S,CW-36*S,12*S);
        DrawRect(Teal,X+18*S,Y+CH-74*S,(CW-36*S)*FMath::Clamp(static_cast<float>(Ratio),0.f,1.f),12*S);
        DrawGameText(Unit.IsAlive()?FString::Printf(TEXT("HP %s"),*Unit.CurrentHp.ToDebugString()):TEXT("Defeated"),
            X+18*S,Y+CH-48*S,28*S,White,CW-36*S);
    }
    if(Units.Num()>PerPage)DrawGameButton(TEXT("More combatants"),TEXT("OG.Presentation.TurnNext"),W-350*S,94*S,318*S,60*S);
    const bool Running=Combat->GetTurnOutcome()==EOGDiagnosticOutcome::Running;
    const TCHAR* Ids[]={TEXT("Basic"),TEXT("Skill1"),TEXT("Skill2"),TEXT("Ultimate"),TEXT("Return")};
    const TCHAR* Labels[]={TEXT("Attack"),TEXT("Skill 1"),TEXT("Skill 2"),TEXT("Ultimate"),TEXT("Return")};
    const float BW=(W-2*M-4*16*S)/5;
    for(int32 I=0;I<5;++I)
        DrawGameButton(Labels[I],FName(*FString::Printf(TEXT("OG.Turn.%s"),Ids[I])),
            M+I*(BW+16*S),H-118*S,BW,86*S,I==0,I==4?!Running:Combat->IsWaitingForTurnPlayer());
    return true;
}

bool AOGWorldPresentationHud::DrawFoundationDiagnosticSurface(AOGWorldPrototypeCharacter* Character, float Scale, bool bOnlyModal)
{
    if (!Character) return false;
    auto* Menu = Character->FindComponentByClass<UOGFoundationDiagnosticMenu>();
    auto* Host = Character->FindComponentByClass<UOGFoundationInteractionHost>();
    const bool bActiveInteraction = Host && Host->IsActive();
    const bool bModal = bDiagnosticMenuOpen || bInteractionMenuOpen || bActiveInteraction;
    if (bOnlyModal && !bModal) return false;
    const float ReadableHeight = FMath::Min(Canvas->ClipX, Canvas->ClipY) / 27.0f;
    const float ButtonHeight = FMath::Max(40 * Scale, ReadableHeight + 22.0f);
    const float RowStep = ButtonHeight + 12.0f * Scale;
    auto Button = [&](FName Id, const FString& Label, float X, float Y, float W, bool bEnabled)
    {
        const float H = ButtonHeight;
        DrawGamePanel(bEnabled ? FLinearColor(.10f, .19f, .25f, .96f) : FLinearColor(.06f, .08f, .09f, .96f), X, Y, W, H);
        DrawGameText(Label,X+12*Scale,Y+(H-ReadableHeight)*.5f,ReadableHeight,
            bEnabled?FLinearColor::White:FLinearColor(.45f,.5f,.55f),W-24*Scale);
        if (bEnabled) AddFoundationHitBox(FVector2D(X,Y), FVector2D(W,H), Id, true, 150);
    };
    if (!bModal)
    {
        if (FoundationSurface == TEXT("world"))
        {
            const float W = FMath::Max(215 * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) * .22f);
            const float Y = 238 * Scale;
            Button(TEXT("OG.Diagnostics.Open"), TEXT("Menu"), 28 * Scale, Y, W, Menu != nullptr);
            Button(TEXT("OG.InteractionMenu.Open"), TEXT("Interaction"), 28 * Scale, Y + 72 * Scale, W, Host != nullptr);
        }
        return false;
    }
    const float W = FMath::Min(Canvas->ClipX - 32 * Scale, FMath::Max(430 * Scale, FMath::Min(Canvas->ClipX, Canvas->ClipY) * .80f));
    const float X = (Canvas->ClipX - W) * .5f;
    const float Top = 65 * Scale;
    const int32 RowCount = (bInteractionMenuOpen || bActiveInteraction) && Host
        ? Host->GetRows().Num() : Menu ? Menu->GetRows().Num() : 0;
    const float PanelHeight = FMath::Min(Canvas->ClipY - Top - 24 * Scale,
        FMath::Max(460 * Scale, RowCount * RowStep + 3 * ReadableHeight + ButtonHeight + 70 * Scale));
    const float CloseY = Top + PanelHeight - ButtonHeight - 24 * Scale;
    DrawRect(FLinearColor(.008f,.012f,.02f,.72f),0,0,Canvas->ClipX,Canvas->ClipY);
    DrawGamePanel(FLinearColor(.025f,.045f,.075f,.98f),
        X - 8 * Scale, Top - 12 * Scale, W + 16 * Scale, PanelHeight);
    DrawFoundationText(bInteractionMenuOpen || bActiveInteraction ? TEXT("Interaction") : TEXT("Activities"),
        FLinearColor::White, X + 8 * Scale, Top, nullptr, .6f * Scale);
    float Y = Top + FMath::Max(35 * Scale, ReadableHeight + 18.0f);
    auto DrawStatus = [&](const FString& Status)
    {
        UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
        const float FontHeight = Font ? FMath::Max(1.0f, Font->GetMaxCharHeight()) : 1.0f;
        TArray<FString> Words; Status.ParseIntoArrayWS(Words);
        FString Line;
        for (const FString& Word : Words)
        {
            const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
            float TextW = 0, TextH = 0;
            GetTextSize(Candidate, TextW, TextH, Font, ReadableHeight / FontHeight);
            if (!Line.IsEmpty() && TextW > W - 16 * Scale)
            {
                if (Y + ReadableHeight > CloseY - 8 * Scale) return;
                DrawFoundationText(Line, FLinearColor::White, X, Y, Font, .4f * Scale);
                Y += ReadableHeight + 8 * Scale; Line = Word;
            }
            else Line = Candidate;
        }
        if (!Line.IsEmpty() && Y + ReadableHeight <= CloseY - 8 * Scale)
            DrawFoundationText(Line, FLinearColor::White, X, Y, Font, .4f * Scale);
    };
    const int32 PerPage=FMath::Max(1,FMath::FloorToInt((CloseY-Y-RowStep-ReadableHeight*2)/RowStep));
    DiagnosticMenuPageCount=FMath::Max(1,FMath::DivideAndRoundUp(RowCount,PerPage));
    DiagnosticMenuPage=FMath::Clamp(DiagnosticMenuPage,0,DiagnosticMenuPageCount-1);
    auto DrawRows=[&](const auto& Rows)
    {
        for(int32 I=DiagnosticMenuPage*PerPage;I<Rows.Num() && I<(DiagnosticMenuPage+1)*PerPage;++I)
        { const auto& Row=Rows[I]; Button(Row.Id,Row.Label,X,Y,W,Row.bEnabled);Y+=RowStep; }
        if(DiagnosticMenuPageCount>1)
        {
            Button(TEXT("OG.Presentation.MenuNext"),FString::Printf(TEXT("More  /  %d of %d"),
                DiagnosticMenuPage+1,DiagnosticMenuPageCount),X,Y,W,true);
            Y+=RowStep;
        }
    };
    if ((bInteractionMenuOpen || bActiveInteraction) && Host)
    {
        DrawRows(Host->GetRows());
        DrawStatus(Host->GetStatus());
    }
    else if (Menu)
    {
        DrawRows(Menu->GetRows());
        DrawStatus(Menu->GetStatus());
    }
    if (!bActiveInteraction) Button(TEXT("OG.Diagnostics.Close"), TEXT("Close"), X, CloseY, W, true);
    return true;
}

bool AOGWorldPresentationHud::HandleFoundationDiagnosticClick(FName Name, AOGWorldPrototypeCharacter* Character)
{
    if (!Character) return false;
    if (Name == TEXT("OG.Diagnostics.Open")) { bDiagnosticMenuOpen = true; bInteractionMenuOpen = false; return true; }
    if (Name == TEXT("OG.InteractionMenu.Open")) { bDiagnosticMenuOpen = false; bInteractionMenuOpen = true; return true; }
    if (Name == TEXT("OG.Diagnostics.Close")) { bDiagnosticMenuOpen = bInteractionMenuOpen = false; return true; }
    if (Name.ToString().StartsWith(TEXT("OG.Diagnostic.")))
    {
        if (auto* Menu = Character->FindComponentByClass<UOGFoundationDiagnosticMenu>()) Menu->ActivateRow(Name);
        return true;
    }
    if (auto* Host = Character->FindComponentByClass<UOGFoundationInteractionHost>())
    {
        if (bInteractionMenuOpen || Host->IsActive())
        {
            for (const auto& Row : Host->GetRows())
                if (Row.Id == Name) { Host->ActivateRow(Name); return true; }
        }
    }
    return false;
}


void AOGWorldPrototypeCharacter::RefreshCanonicalPresentation(const FOGFoundationCharacterContext& Context)
{
    if (!CanonicalPresentation || !DiagnosticCombat || !DiagnosticCombat->IsActive()) return;
    FOGEntityId Controlled; FOGContentId Identity; bool bManifestation = false;
    FOGCharacterManifestationRecord Manifestation; FString Error;
    if (DiagnosticCombat->TryGetControlledContext(Controlled, Identity, bManifestation, Manifestation, Error) && Controlled == Context.EntityId)
    {
        if (!CanonicalPresentation->HasProjection())
            CanonicalPresentation->Configure(Context, GetMesh(), FName(*(FString(TEXT("rig:")) + GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName().ToLower())), Error);
        else CanonicalPresentation->Refresh(Context, Error);
        CanonicalPresentation->SetPrivacyPresentation(bSfwPresentation);
    }
    if (Context.EntityId == DiagnosticCombat->GetRulerId() && ProtagonistRepresentation)
    {
        if (auto* Presenter = ProtagonistRepresentation->FindComponentByClass<UOGCanonicalCharacterPresentation>())
        { Presenter->Refresh(Context, Error); Presenter->SetPrivacyPresentation(bSfwPresentation); }
    }
}

void AOGWorldPrototypeCharacter::ApplyInteractionCharacterPresentation(const FOGInteractionParticipantState& State)
{
    AActor* Actor = State.Binding.Actor.Get();
    if (!Actor) return;
    auto* Character = Cast<ACharacter>(Actor);
    if (!Character) return;
    auto* Presenter = Actor->FindComponentByClass<UOGCanonicalCharacterPresentation>();
    if (!Presenter)
    {
        Presenter = NewObject<UOGCanonicalCharacterPresentation>(Actor);
        Actor->AddInstanceComponent(Presenter); Presenter->RegisterComponent();
    }
    FString Error;
    if (!Presenter->HasProjection()) Presenter->Configure(State.Binding.Character,
        Character->GetMesh(), FName(*State.Binding.RigFamilyId.ToString()), Error);
    else Presenter->Refresh(State.Binding.Character, Error);
    Presenter->SetPrivacyPresentation(bSfwPresentation);
}
