#include "World/OGFoundationIntegrationWorld.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

void AOGFoundationIntegrationWorld::BuildScenery()
{
    // Batched, non-interactive dressing. The existing floor, openings, doorway,
    // traversal fixtures and water volume remain the sole gameplay geometry.
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto Batch=[&](UStaticMesh* Shape,const FLinearColor& Color)
    {
        auto* M=NewObject<UInstancedStaticMeshComponent>(this);
        AddInstanceComponent(M);M->SetupAttachment(RootComponent);
        M->SetStaticMesh(Shape);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        M->SetGenerateOverlapEvents(false);M->SetCanEverAffectNavigation(false);
        M->SetCullDistances(6000,8500);
        if(Parent)
        {
            auto* Material=UMaterialInstanceDynamic::Create(Parent,this);
            Material->SetVectorParameterValue(TEXT("Color"),Color);M->SetMaterial(0,Material);
        }
        M->RegisterComponent();return M;
    };
    auto* Stone=Batch(Cube,FLinearColor(.38f,.40f,.36f));
    auto* Trim=Batch(Cube,FLinearColor(.45f,.37f,.21f));
    auto* Paving=Batch(Cube,FLinearColor(.31f,.33f,.28f));
    auto* Rock=Batch(ScenerySphere,FLinearColor(.25f,.29f,.27f));
    auto* Leaves=Batch(SceneryCone,FLinearColor(.08f,.20f,.16f));
    auto Place=[](UInstancedStaticMeshComponent* M,const FVector& At,const FVector& Size,float Yaw=0.f)
    { M->AddInstance(FTransform(FRotator(0,Yaw,0),At,Size/100.f)); };

    // Worn paving guides the eye through the usable space without covering water.
    for(int32 I=0;I<16;++I)
    {
        Place(Paving,FVector(-390,-1100+I*205,1.4f),FVector(280,185,2));
        if(I<9)Place(Paving,FVector(-190+I*205,430,1.4f),FVector(185,220,2));
    }
    // An architectural silhouette and cornices on the existing roofed room.
    Place(Stone,FVector(2150,1400,355),FVector(1260,1060,24));
    Place(Trim,FVector(2150,1400,371),FVector(1280,1080,8));
    for(float X:{1540.f,2760.f})
        for(float Y:{890.f,1910.f})
        {
            Place(Stone,FVector(X,Y,155),FVector(86,86,310));
            Place(Trim,FVector(X,Y,28),FVector(112,112,24));
            Place(Trim,FVector(X,Y,295),FVector(110,110,18));
        }
    // Recessed front-door trim stays outside the 250cm traversable opening.
    for(float X:{2000.f,2300.f})
        Place(Trim,FVector(X,1940,142),FVector(24,14,280));
    Place(Trim,FVector(2150,1940,292),FVector(324,14,20));

    // Low foliage and stones break up bare perimeter ground. Keep the lake,
    // deep shaft, carriers and fixture approaches unobstructed.
    for(int32 I=0;I<14;++I)
    {
        const float Y=-2600+I*330.f;
        const float X=I%2==0?-3100.f:-2840.f;
        Place(Rock,FVector(X,Y,20),FVector(125,85,45),I*37.f);
        for(int32 J=0;J<3;++J)
            Place(Leaves,FVector(X+70+J*45,Y+J*27,45+J*8),
                FVector(115-J*15,115-J*15,95+J*16),I*19.f);
    }
    for(int32 I=0;I<9;++I)
    {
        const float X=2900+FMath::Sin(I*1.7f)*80, Y=-1000+I*300.f;
        Place(Rock,FVector(X,Y,14),FVector(100,65,32),I*43.f);
        Place(Leaves,FVector(X+150,Y+35,48),FVector(105,105,100),I*29.f);
    }
}
