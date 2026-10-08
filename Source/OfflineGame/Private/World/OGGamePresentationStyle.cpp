#include "World/OGStartingRegionPresentation.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include <initializer_list>

namespace
{
const FLinearColor Ink(.025f,.038f,.067f,1);
const FLinearColor Slate(.065f,.10f,.16f,1);
const FLinearColor Gold(.83f,.69f,.40f,1);
const FLinearColor Paper(.94f,.94f,.89f,1);
}

void AOGWorldPresentationHud::DrawGamePolygon(const TArray<FVector2D>& Points, FLinearColor Color)
{
    if (!Canvas || Points.Num() < 3) return;
    // Ear clipping also handles the concave mountain and cloak outlines.
    TArray<FCanvasUVTri> Triangles;
    TArray<int32> Remaining;
    double Area=0;
    auto Cross=[](const FVector2D& A,const FVector2D& B,const FVector2D& C)
    { return (B.X-A.X)*(C.Y-A.Y)-(B.Y-A.Y)*(C.X-A.X); };
    for(int32 I=0;I<Points.Num();++I)
    {
        Remaining.Add(I);
        const auto& A=Points[I];const auto& B=Points[(I+1)%Points.Num()];
        Area+=A.X*B.Y-B.X*A.Y;
    }
    const double Sign=Area>=0?1.0:-1.0;
    while(Remaining.Num()>=3)
    {
        bool Found=false;
        for(int32 I=0;I<Remaining.Num();++I)
        {
            int32 A=Remaining[(I+Remaining.Num()-1)%Remaining.Num()];
            int32 B=Remaining[I],C=Remaining[(I+1)%Remaining.Num()];
            if(Cross(Points[A],Points[B],Points[C])*Sign<=0.0001) continue;
            bool Contains=false;
            for(int32 P:Remaining)
            {
                if(P==A||P==B||P==C) continue;
                if(Cross(Points[A],Points[B],Points[P])*Sign>=0 &&
                   Cross(Points[B],Points[C],Points[P])*Sign>=0 &&
                   Cross(Points[C],Points[A],Points[P])*Sign>=0)
                {Contains=true;break;}
            }
            if(Contains) continue;
            FCanvasUVTri T;
            T.V0_Pos=Points[A];T.V1_Pos=Points[B];T.V2_Pos=Points[C];
            T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;
            T.V0_Color=T.V1_Color=T.V2_Color=Color;
            Triangles.Add(T);Remaining.RemoveAt(I);Found=true;break;
        }
        if(!Found) break;
    }
    Canvas->K2_DrawTriangle(nullptr, Triangles);
}

void AOGWorldPresentationHud::DrawGamePanel(FLinearColor Fill, float X,float Y,float W,float H)
{
    const float C=FMath::Min(12.f, H*.18f);
    DrawGamePolygon({{X+C,Y},{X+W,Y},{X+W,Y+H-C},{X+W-C,Y+H},{X,Y+H},{X,Y+C}},Fill);
    DrawLine(X+C,Y,X+W,Y,FLinearColor(.52f,.64f,.72f,.24f),1.f);
    DrawLine(X,Y+H,X+W-C,Y+H,FLinearColor(0,0,0,.5f),2.f);
}

void AOGWorldPresentationHud::DrawGameText(const FString& Text,float X,float Y,float Height,
    FLinearColor Color,float MaxWidth)
{
    UFont* Font=GEngine?GEngine->GetLargeFont():nullptr;
    if (!Font) return;
    const float S=Height/FMath::Max(1.f,Font->GetMaxCharHeight());
    FString Line=Text;
    Line.ReplaceInline(TEXT("diagnostic:identity.kit_0"),TEXT("Protagonist"));
    Line.ReplaceInline(TEXT("diagnostic:identity.kit_1"),TEXT("Guardian"));
    Line.ReplaceInline(TEXT("diagnostic:identity.kit_2"),TEXT("Striker"));
    float W=0,H=0;
    GetTextSize(Line,W,H,Font,S);
    if (MaxWidth>0 && W>MaxWidth)
    {
        while (Line.Len()>1)
        {
            Line.LeftChopInline(1);
            GetTextSize(Line+TEXT("..."),W,H,Font,S);
            if(W<=MaxWidth) break;
        }
        Line+=TEXT("...");
    }
    Super::DrawText(Line,FLinearColor(0,0,0,Color.A*.65f),X+1,Y+2,Font,S);
    Super::DrawText(Line,Color,X,Y,Font,S);
}

void AOGWorldPresentationHud::DrawGameIcon(FName Id,float X,float Y,float Size,FLinearColor Color)
{
    const FString Name=Id.ToString().ToLower();
    const float R=Size*.43f,CX=X+Size*.5f,CY=Y+Size*.5f;
    const float T=FMath::Max(2.f,Size*.045f);
    auto L=[&](float A,float B,float C,float D){DrawLine(CX+A*R,CY+B*R,CX+C*R,CY+D*R,Color,T);};
    if(Name.Contains(TEXT("attack")) || Name==TEXT("basic"))
    { L(-.65f,.7f,.62f,-.7f); L(.62f,-.7f,.55f,-.1f);L(.62f,-.7f,.02f,-.6f);L(-.65f,.0f,.0f,.6f); }
    else if(Name.Contains(TEXT("jump")) || Name.Contains(TEXT("rise")) || Name.Contains(TEXT("ascend")))
    { L(0,.75f,0,-.75f);L(-.55f,-.15f,0,-.75f);L(.55f,-.15f,0,-.75f);L(-.65f,.8f,.65f,.8f); }
    else if(Name.Contains(TEXT("descend")) || Name.Contains(TEXT("dive")) || Name.Contains(TEXT("let go")))
    { L(0,-.75f,0,.65f);L(-.55f,.05f,0,.65f);L(.55f,.05f,0,.65f); }
    else if(Name.Contains(TEXT("dodge")) || Name.Contains(TEXT("sprint")))
    { L(-.85f,.55f,-.15f,0);L(-.15f,0,-.85f,-.55f);L(-.1f,.55f,.6f,0);L(.6f,0,-.1f,-.55f); }
    else if(Name.Contains(TEXT("lock")))
    { Canvas->K2_DrawPolygon(nullptr,{CX,CY},{R*.65f,R*.65f},24,FLinearColor(Color.R,Color.G,Color.B,.13f));
      L(-1,0,-.4f,0);L(.4f,0,1,0);L(0,-1,0,-.4f);L(0,.4f,0,1); }
    else if(Name.Contains(TEXT("characters")) || Name.Contains(TEXT("party")))
    { Canvas->K2_DrawPolygon(nullptr,{CX,CY-R*.4f},{R*.32f,R*.32f},14,Color);
      DrawGamePolygon({{CX-R*.7f,CY+R*.85f},{CX-R*.5f,CY+R*.15f},{CX,CY-R*.03f},
        {CX+R*.5f,CY+R*.15f},{CX+R*.7f,CY+R*.85f}},Color); }
    else if(Name.Contains(TEXT("home")))
    { L(-.85f,0,0,-.8f);L(0,-.8f,.85f,0);L(-.65f,-.1f,-.65f,.8f);L(-.65f,.8f,.65f,.8f);L(.65f,.8f,.65f,-.1f); }
    else if(Name.Contains(TEXT("territory")) || Name.Contains(TEXT("map")))
    { L(-.9f,-.6f,-.9f,.8f);L(-.9f,.8f,-.3f,.5f);L(-.3f,.5f,.3f,.8f);L(.3f,.8f,.9f,.5f);
      L(.9f,.5f,.9f,-.8f);L(.9f,-.8f,.3f,-.5f);L(.3f,-.5f,-.3f,-.8f);L(-.3f,-.8f,-.9f,-.6f);L(-.3f,-.8f,-.3f,.5f);L(.3f,-.5f,.3f,.8f); }
    else if(Name.Contains(TEXT("records")) || Name.Contains(TEXT("codex")))
    { L(-.8f,-.7f,-.8f,.7f);L(-.8f,.7f,0,.85f);L(0,.85f,.8f,.7f);L(.8f,.7f,.8f,-.7f);
      L(.8f,-.7f,0,-.55f);L(0,-.55f,-.8f,-.7f);L(0,-.55f,0,.85f); }
    else if(Name.Contains(TEXT("settings")) || Name.Contains(TEXT("menu")))
    { for(int32 I=-1;I<=1;++I){L(-.8f,I*.55f,.8f,I*.55f);L(I*.4f,I*.55f-.18f,I*.4f,I*.55f+.18f);} }
    else if(Name.Contains(TEXT("pause")))
    { DrawRect(Color,CX-R*.55f,CY-R*.75f,R*.35f,R*1.5f);DrawRect(Color,CX+R*.2f,CY-R*.75f,R*.35f,R*1.5f); }
    else // Summon, skills and ultimate share a readable four-point energy mark.
    { DrawGamePolygon({{CX,CY-R},{CX+R*.25f,CY-R*.25f},{CX+R,CY},{CX+R*.25f,CY+R*.25f},
        {CX,CY+R},{CX-R*.25f,CY+R*.25f},{CX-R,CY},{CX-R*.25f,CY-R*.25f}},Color); }
}

void AOGWorldPresentationHud::DrawGameButton(const FString& Label,FName Id,
    float X,float Y,float W,float H,bool bPrimary,bool bEnabled,int32 Priority)
{
    const FLinearColor Accent=bPrimary?Gold:FLinearColor(.42f,.72f,.76f,1);
    DrawGamePanel(bPrimary?FLinearColor(.22f,.19f,.12f,.98f):FLinearColor(.055f,.095f,.15f,.96f),X,Y,W,H);
    DrawRect(FLinearColor(Accent.R,Accent.G,Accent.B,bEnabled?.95f:.2f),X,Y+H-3,W,3);
    DrawGameText(Label,X+H*.23f,Y+H*.24f,FMath::Min(H*.43f,FMath::Min(Canvas->ClipX,Canvas->ClipY)/30.f),
        bEnabled?Paper:FLinearColor(.4f,.46f,.53f,1),W-H*.4f);
    if(bEnabled) AddFoundationHitBox({X,Y},{W,H},Id,true,Priority);
}

void AOGWorldPresentationHud::DrawGameBackdrop(float X,float Y,float W,float H)
{
    for(int32 I=0;I<24;++I)
    {
        float A=static_cast<float>(I)/24;
        DrawRect(FMath::Lerp(FLinearColor(.028f,.045f,.09f,1),FLinearColor(.09f,.18f,.22f,1),A),X,Y+H*A,W,H/24+1);
    }
    Canvas->K2_DrawPolygon(nullptr,{X+W*.72f,Y+H*.24f},{W*.09f,W*.09f},48,FLinearColor(.53f,.70f,.70f,.18f));
    DrawGamePolygon({{X,Y+H*.74f},{X+W*.14f,Y+H*.36f},{X+W*.28f,Y+H*.66f},{X+W*.48f,Y+H*.31f},
        {X+W*.7f,Y+H*.69f},{X+W*.84f,Y+H*.4f},{X+W,Y+H*.64f},{X+W,Y+H},{X,Y+H}},FLinearColor(.045f,.085f,.12f,1));
    DrawGamePolygon({{X,Y+H*.87f},{X+W*.3f,Y+H*.66f},{X+W*.55f,Y+H*.86f},{X+W*.85f,Y+H*.67f},
        {X+W,Y+H*.85f},{X+W,Y+H},{X,Y+H}},Ink);
    for(int32 I=0;I<24;++I)
    {
        float U=FMath::Frac(I*.6180339f), V=FMath::Frac(I*.381966f);
        const float S=I%3==0?2.f:1.f;
        DrawRect(FLinearColor(.8f,.86f,.82f,.35f),X+U*W,Y+V*H*.55f,S,S);
    }
}

void AOGWorldPresentationHud::DrawFoundationCharacterStandIn(
    float X,float Y,float W,float H,const FLinearColor& Accent)
{
    // Original scalable portrait illustration, not a fabricated content asset.
    // Authored character portraits can replace this renderer without changing UI.
    DrawGamePanel(FLinearColor(.045f,.075f,.12f,.96f),X,Y,W,H);
    const float ArtW=FMath::Min(W,H*.75f), AX=X+(W-ArtW)*.5f;
    auto P=[&](float U,float V){return FVector2D(AX+ArtW*U,Y+H*V);};
    auto Poly=[&](std::initializer_list<FVector2D> Points,FLinearColor C)
    {TArray<FVector2D> A;for(const auto& V:Points)A.Add(V);DrawGamePolygon(A,C);};
    const FLinearColor Rim(Accent.R,Accent.G,Accent.B,.24f);
    Canvas->K2_DrawPolygon(nullptr,P(.5f,.38f),{ArtW*.42f,H*.29f},48,Rim);
    for(int32 I=0;I<8;++I)
    {
        float A=I*PI/4;
        FVector2D C=P(.5f,.38f);
        DrawLine(C.X+FMath::Cos(A)*ArtW*.32f,C.Y+FMath::Sin(A)*H*.24f,
            C.X+FMath::Cos(A)*ArtW*.39f,C.Y+FMath::Sin(A)*H*.29f,Rim,2);
    }
    const FLinearColor Dark(.025f,.04f,.07f,1), Cloth(.10f,.16f,.23f,1), Skin(.72f,.66f,.56f,1);
    // Cape, fitted shoulders, hair and face replace the old four-rectangle dummy.
    Poly({P(.4f,.36f),P(.21f,.44f),P(.08f,.98f),P(.82f,.98f),P(.73f,.46f),P(.6f,.36f)},Dark);
    Poly({P(.23f,.48f),P(.42f,.40f),P(.57f,.4f),P(.77f,.48f),P(.65f,.79f),P(.72f,.98f),P(.29f,.98f),P(.35f,.78f)},Cloth);
    Poly({P(.29f,.46f),P(.41f,.43f),P(.5f,.65f),P(.39f,.75f)},Accent);
    Poly({P(.58f,.43f),P(.72f,.48f),P(.63f,.73f),P(.51f,.65f)},FLinearColor(Accent.R*.55f,Accent.G*.55f,Accent.B*.55f,1));
    Poly({P(.44f,.32f),P(.56f,.32f),P(.59f,.43f),P(.50f,.48f),P(.41f,.43f)},Skin);
    Poly({P(.34f,.18f),P(.4f,.11f),P(.59f,.12f),P(.66f,.23f),P(.62f,.4f),P(.5f,.44f),P(.34f,.36f)},Dark);
    Poly({P(.39f,.22f),P(.59f,.2f),P(.61f,.31f),P(.54f,.37f),P(.45f,.36f),P(.39f,.30f)},Skin);
    Poly({P(.33f,.24f),P(.4f,.13f),P(.58f,.15f),P(.66f,.25f),P(.53f,.20f),P(.42f,.27f)},Cloth);
    DrawLine(P(.43f,.275f).X,P(.43f,.275f).Y,P(.48f,.275f).X,P(.48f,.275f).Y,Dark,2);
    DrawLine(P(.54f,.27f).X,P(.54f,.27f).Y,P(.58f,.27f).X,P(.58f,.27f).Y,Dark,2);
    Poly({P(.48f,.50f),P(.52f,.50f),P(.53f,.61f),P(.5f,.65f),P(.47f,.61f)},Gold);
    DrawLine(P(.29f,.81f).X,P(.29f,.81f).Y,P(.69f,.81f).X,P(.69f,.81f).Y,Gold,FMath::Max(2.f,H*.006f));
    // Bottom scrim gives names a stable contrast over every portrait.
    for(int32 I=0;I<8;++I)
        DrawRect(FLinearColor(Ink.R,Ink.G,Ink.B,I/8.f),X,Y+H*(.76f+.03f*I),W,H*.031f);
}
