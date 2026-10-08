#include "World/OGStartingRegionPresentation.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/HUDHitBox.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGFoundationHumanoidRigTest,
    "OfflineGame.Foundation.Presentation.RiggedDiagnosticHumanoid",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGFoundationHumanoidRigTest::RunTest(const FString& Parameters)
{
    const AOGWorldPrototypeCharacter* Pawn =
        GetDefault<AOGWorldPrototypeCharacter>();
    USkeletalMeshComponent* Mesh = Pawn->GetMesh();
    if (!TestNotNull(TEXT("Diagnostic skeletal mesh component"), Mesh))
    {
        return false;
    }
    const USkeletalMesh* Rig = Mesh->GetSkeletalMeshAsset();
    if (!TestNotNull(TEXT("Diagnostic humanoid asset replaces cylinder"), Rig))
    {
        return false;
    }
    TestTrue(TEXT("Humanoid has articulated limbs"),
        Rig->GetRefSkeleton().GetNum() > 12);
    TestNotNull(TEXT("Humanoid has a compatible skeleton"), Rig->GetSkeleton());
    TestTrue(TEXT("Locomotion anim instance is bound"),
        Mesh->GetAnimClass() != nullptr);
    TestTrue(TEXT("Presentation does not add a second collision body"),
        Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Rig keeps its authored proportions"),
        Mesh->GetRelativeScale3D().Equals(FVector::OneVector));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGFoundationHudCanvasTransformTest,
    "OfflineGame.Foundation.Presentation.HitTargetsFollowCanvasTransform",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FOGFoundationHudCanvasTransformTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues InitValues;
    InitValues.AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, true,
        NAME_None, nullptr, true, ERHIFeatureLevel::SM5, &InitValues, false);
    if (!TestNotNull(TEXT("Isolated HUD test world"), World))
    {
        return false;
    }

    for (const FIntPoint Dimensions : { FIntPoint(1080, 1920), FIntPoint(1920, 1080) })
    {
        FVector2D BasePosition;
        FVector2D BaseSize = FVector2D::ZeroVector;
        const FVector2D SafeInset(24.0f, 96.0f);
        const FName Name(TEXT("OG.Opening.Ruler"));
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            FCanvas DrawCanvas(nullptr, nullptr, World, ERHIFeatureLevel::SM5);
            UCanvas* Canvas = NewObject<UCanvas>();
            Canvas->Init(Dimensions.X, Dimensions.Y, nullptr, &DrawCanvas);
            Canvas->OrgX = 0.0f;
            Canvas->OrgY = 0.0f;
            Canvas->ClipX = Dimensions.X;
            Canvas->ClipY = Dimensions.Y;
            if (Pass == 1)
            {
                // Safe-zone drawing is a transform-stack translation. OrgY can
                // remain zero, so adding OrgY alone cannot align touch targets.
                DrawCanvas.PushRelativeTransform(
                    FTransform(FVector(SafeInset.X, SafeInset.Y, 0.0f)).ToMatrixWithScale());
            }
            APlayerController* PlayerOwner = World->SpawnActor<APlayerController>();
            AOGWorldPresentationHud* Hud =
                World->SpawnActor<AOGWorldPresentationHud>();
            Hud->SetOwner(PlayerOwner);
            Hud->PlayerOwner = PlayerOwner;
            Hud->SetCanvas(Canvas, Canvas);
            Hud->DrawHUD();
            const FHUDHitBox* Box = Hud->GetHitBoxWithName(Name);
            if (TestNotNull(TEXT("Opening Ruler control is registered"), Box))
            {
                if (Pass == 0)
                {
                    // Locate the real registered rectangle through its public
                    // containment contract; engine hit boxes expose no bounds getters.
                    bool bFound = false;
                    for (int32 Y = 0; Y < Dimensions.Y && !bFound; ++Y)
                    {
                        for (int32 X = 0; X < Dimensions.X; ++X)
                        {
                            const FVector2D Point(X + 0.5f, Y + 0.5f);
                            if (Box->Contains(Point))
                            {
                                BasePosition = Point;
                                bFound = true;
                                break;
                            }
                        }
                    }
                    TestTrue(TEXT("Control has a nonempty viewport rectangle"), bFound);
                    if (bFound)
                    {
                        while (Box->Contains(BasePosition + FVector2D(BaseSize.X, 0)))
                        {
                            BaseSize.X += 1.0f;
                        }
                        while (Box->Contains(BasePosition + FVector2D(0, BaseSize.Y)))
                        {
                            BaseSize.Y += 1.0f;
                        }
                    }
                }
                else
                {
                    TestTrue(TEXT("Hit rectangle follows rendered safe-zone origin"),
                        Box->Contains(BasePosition + SafeInset) &&
                        !Box->Contains(BasePosition + SafeInset - FVector2D(2, 0)));
                    const FVector2D VisibleCenter =
                        BasePosition + SafeInset + BaseSize * 0.5f;
                    const FHUDHitBox* Hit = Hud->GetHitBoxAtCoordinates(VisibleCenter);
                    TestTrue(TEXT("Tapping the rendered control center activates it"),
                        Hit == Box);
                }
            }
            Hud->SetCanvas(nullptr, nullptr);
            Hud->Destroy();
            PlayerOwner->Destroy();
            if (Pass == 1)
            {
                DrawCanvas.PopTransform();
            }
        }
    }
    World->DestroyWorld(true);
    return true;
}
#endif