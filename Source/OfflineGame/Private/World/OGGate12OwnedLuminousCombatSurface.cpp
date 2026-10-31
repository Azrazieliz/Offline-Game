// Actual owned Luminous turn controls OVER the native rendered First World.
// This is not the diagnostic black-screen arena and does not create an enemy,
// a phantom-owned Manifestation, an animation or a second gameplay executor.
#include "World/OGStartingRegionPresentation.h"
#include "Koikatsu/OGLuminousV3PlayableCharacter.h"
#include "Characters/OGContentCharacterActors.h"
#include "Runtime/OGGate12RulerSessionSubsystem.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Combat/OGTurnBattle.h"
#include "UI/OGNativeBattleWidget.h"
#include "Combat/OGCanonicalCombatActorBindingComponent.h"
#include "Combat/OGNativeBattleCommands.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool MatchesActualOwnedLuminousBody(const AOGWorldPrototypeCharacter* Character,
    const UOGGate12RulerSessionSubsystem& Session, const FOGCombatUnitState& Unit)
{
    if (!Character || Unit.UnitEntityId != Session.SelectedManifestationId) return false;
    const AOGLuminousV3PlayableCharacter* Legacy = Cast<AOGLuminousV3PlayableCharacter>(Character);
    const AOGContentPlayableCharacter* Content = Cast<AOGContentPlayableCharacter>(Character);
    return (Legacy && Legacy->IsBoundToOwnedLuminousCombatUnit(Session.BoundRulerId,
        Unit.UnitEntityId, Unit.SkillSet.CurrentForm)) ||
        (Content && Content->IsBoundToOwnedCombatUnit(Session.BoundRulerId,
            Unit.UnitEntityId, Unit.SkillSet.CurrentForm));
}
}

bool AOGWorldPrototypeCharacter::SelectOwnedLuminousBattleTarget(
    int32 UnitIndex, FString& OutError)
{
    OutError.Reset();
    if (!GetWorld() || !GetWorld()->IsGameWorld() || !IsLocallyControlled() ||
        !HasNativeOwnedLuminousEncounter())
    {
        OutError = TEXT("No real locally controlled owned battle is active.");
        return false;
    }
    UGameInstance* GI = GetGameInstance();
    const UOGGameCoreSubsystem* Core = GI
        ? GI->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    const UOGGate12RulerSessionSubsystem* Session = GI
        ? GI->GetSubsystem<UOGGate12RulerSessionSubsystem>() : nullptr;
    const FOGTurnBattle* Battle = Core ? Core->GetOwnedLuminousTurnBattle() : nullptr;
    if (!Battle || !Session || !Session->IsBoundToStoredRuler())
    {
        OutError = TEXT("Owned encounter/Registry session was not available.");
        return false;
    }
    const FOGTurnBattleState& State = Battle->GetState();
    const int32 ActingIndex = Battle->SelectNextActingUnitIndex();
    FOGEntityId Controlled;
    if (!ReadControlledCanonicalBattleUnit(Controlled,OutError)) return false;
    const auto* Source=State.Units.FindByPredicate([&](const auto& U){return U.UnitEntityId==Controlled;});
    if (State.Status != EOGTurnBattleStatus::Running || !Source ||
        (State.ExecutionMode==EOGCombatExecutionMode::Turn &&
            (!State.Units.IsValidIndex(ActingIndex) || State.Units[ActingIndex].UnitEntityId!=Controlled)) ||
        !State.Units.IsValidIndex(UnitIndex))
    {
        OutError = TEXT("No legal actor or selected target exists in the live turn state.");
        return false;
    }
    const FOGCombatUnitState& Target = State.Units[UnitIndex];
    // Revival may target a genuine defeated ally with no living actor.
    // Never reinterpret a removed unit or an enemy corpse as a player focus.
    // The canonical skill resolver still decides whether this precise
    // ability can actually revive the selected participant.
    const bool bDefeatedAlly=Target.Presence==EOGCombatPresence::Defeated &&
        !Target.IsAlive() && Target.TeamIndex==Source->TeamIndex;
    const bool bLivingBody=Target.CanAct() &&
        UOGCanonicalCombatActorBindingComponent::FindActualWorldActor(
            GetWorld(),State.BattleId,Target.UnitEntityId)!=nullptr;
    if (!Source->CanAct() || (!bLivingBody && !bDefeatedAlly))
    {
        OutError = TEXT("Select a living field participant or a genuine defeated ally for an approved revival.");
        return false;
    }
    // Tap selects only a PRIMARY focus for primary-specific skill bonuses;
    // AoE auto-expands to every eligible active field participant in the
    // canonical executor. No manual cap or cherry-picking of AoE victims.
    if (SelectedCanonicalBattleId!=State.BattleId)
    { SelectedCanonicalTurnTargets.Reset(); SelectedCanonicalBattleId=State.BattleId; }
    if (SelectedCanonicalTurnTargets.Contains(Target.UnitEntityId))
        SelectedCanonicalTurnTargets.Remove(Target.UnitEntityId);
    else
    {
        SelectedCanonicalTurnTargets.Reset();
        SelectedCanonicalTurnTargets.Add(Target.UnitEntityId);
    }
    // Keep the established knowledge/privacy projection for an already
    // observed target; NEVER reveal undiscovered HP/identity from battle math.
    FOGWorldTargetViewModel Projection;
    if (HasTargetHudProjectionForHud() &&
        GetTargetHudProjectionForHud().TargetEntityId == Target.UnitEntityId)
        Projection = GetTargetHudProjectionForHud();
    if (SelectedCanonicalTurnTargets.IsEmpty())
    { ClearTargetHudProjection(); return true; }
    Projection.TargetEntityId = SelectedCanonicalTurnTargets.Last();
    Projection.bPrincipalTarget = true;
    SetTargetHudProjection(Projection);
    return true;
}

void AOGWorldPresentationHud::RegisterCanonicalCombatImpact(
    AActor* Target, const FOGLargeNumber& Before,
    const FOGLargeNumber& After)
{
    if (!IsValid(Target) || !GetWorld()) return;
    if (FOGLargeNumber::Compare(Before,After)==0) return;
    FCanonicalCombatImpact Cue;
    Cue.ActualActor=Target;
    Cue.bRestoration=FOGLargeNumber::Compare(After,Before)>0;
    // Display only verified real post-commit HP movement, never inferred
    // attack damage, private opponent MaxHP or guessed status consequences.
    const FOGLargeNumber NegativeBefore(-Before.Significand,Before.Exponent10);
    const FOGLargeNumber Delta=FOGLargeNumber::Add(After,NegativeBefore);
    Cue.Number=FString::Printf(TEXT("%s%s"),
        Cue.bRestoration?TEXT("+"):TEXT(""),*Delta.ToDebugString());
    Cue.StartAt=GetWorld()->GetTimeSeconds();
    NativeCombatImpacts.Add(MoveTemp(Cue));
    if(NativeCombatImpacts.Num()>32)
        NativeCombatImpacts.RemoveAt(0,NativeCombatImpacts.Num()-32);
}

void AOGWorldPresentationHud::DrawNativeCombatImpacts(float Scale)
{
    if (!Canvas || !PlayerOwner || !GetWorld()) return;
    const double Now=GetWorld()->GetTimeSeconds();
    for(int32 I=NativeCombatImpacts.Num()-1;I>=0;--I)
    {
        const FCanonicalCombatImpact& Cue=NativeCombatImpacts[I];
        AActor* Target=Cue.ActualActor.Get();
        const double Age=Now-Cue.StartAt;
        if(!Target || Age>1.5 || Age<0)
        { NativeCombatImpacts.RemoveAtSwap(I);continue; }
        FVector Center,Extents;
        Target->GetActorBounds(false,Center,Extents);
        FVector2D Screen;
        if(PlayerOwner->ProjectWorldLocationToScreen(
            Center+FVector(0,0,Extents.Z+30),Screen,true))
        {
            Screen.Y-=static_cast<float>(Age)*42.f*Scale;
            if(Screen.X>0 && Screen.X<Canvas->ClipX &&
               Screen.Y>0 && Screen.Y<Canvas->ClipY)
            {
                const float Alpha=FMath::Clamp(1.f-static_cast<float>(Age/1.5),0.f,1.f);
                const FLinearColor ImpactTint=Cue.bRestoration
                    ?FLinearColor(.43f,.94f,.68f,Alpha)
                    :FLinearColor(.95f,.45f,.46f,Alpha);
                DrawGameText(Cue.Number,Screen.X-32.f*Scale,
                    Screen.Y,22.f*Scale,ImpactTint,155.f*Scale);
            }
        }
    }
}

bool AOGWorldPresentationHud::DrawOwnedLuminousCombatSurface(
    AOGWorldPrototypeCharacter* Character, float Scale)
{
    if (!Canvas || !Character)
        return false;
    UGameInstance* GI = GetGameInstance();
    const UOGGameCoreSubsystem* Core = GI
        ? GI->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    const UOGGate12RulerSessionSubsystem* Session = GI
        ? GI->GetSubsystem<UOGGate12RulerSessionSubsystem>() : nullptr;
    const FOGTurnBattle* Battle = Core ? Core->GetOwnedLuminousTurnBattle() : nullptr;
    if (!Battle || !Session || !Session->IsBoundToStoredRuler())
        return false;
    const FOGTurnBattleState& State = Battle->GetState();
    FOGEntityId Controlled; FString ControlError;
    if (!Character->ReadControlledCanonicalBattleUnit(Controlled,ControlError)) return false;
    // A completed battle may outlive possession; don't display an old
    // Ruler's private battle results over a different character.
    const FOGCombatUnitState* ActualOwned=State.Units.FindByPredicate(
        [&Controlled](const FOGCombatUnitState& Unit)
        { return Unit.UnitEntityId==Controlled; });
    if(!ActualOwned)return false;
    // Completed-before-first-draw encounters still get the native touch results surface.
    if (State.Status==EOGTurnBattleStatus::Running || State.Status==EOGTurnBattleStatus::Completed)
    {
        if (PlayerOwner)
        {
            bool Created=false;
            if (!NativeBattleWidget)
            {
                NativeBattleWidget=CreateWidget<UOGNativeBattleWidget>(PlayerOwner,UOGNativeBattleWidget::StaticClass());
                if (NativeBattleWidget) { NativeBattleWidget->AddToViewport(180); Created=true; }
            }
            if (NativeBattleWidget)
            {
                const bool WasHidden=!NativeBattleWidget->IsVisible();
                NativeBattleWidget->BindCharacter(Character);
                if (WasHidden && !Created) NativeBattleWidget->RefreshBattle();
                if (NativeBattleWidget->IsVisible() && (Created || WasHidden))
                {
                    FInputModeGameAndUI Input;
                    Input.SetHideCursorDuringCapture(false);
                    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                    PlayerOwner->SetInputMode(Input);
                }
                DrawNativeCombatImpacts(Scale);
                return State.Status==EOGTurnBattleStatus::Running || NativeBattleWidget->IsVisible();
            }
        }
    }
    if (State.Status == EOGTurnBattleStatus::Completed)
    {
        const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0.;
        if(CompletedCombatBannerId!=State.BattleId)
        { CompletedCombatBannerId=State.BattleId; CompletedCombatBannerExpires=Now+6.; }
        DrawNativeCombatImpacts(Scale);
        if(Now>CompletedCombatBannerExpires)return false;
        const bool bOwnedSurvived=State.Units.ContainsByPredicate(
            [Session](const FOGCombatUnitState& Unit)
            { return Unit.UnitEntityId==Session->SelectedManifestationId && Unit.IsAlive(); });
        const bool bEnemySurvived=State.Units.ContainsByPredicate(
            [Session,&State](const FOGCombatUnitState& Unit)
            {
                const FOGCombatUnitState* Owned=State.Units.FindByPredicate(
                    [Session](const FOGCombatUnitState& O)
                    { return O.UnitEntityId==Session->SelectedManifestationId; });
                return Owned && Unit.TeamIndex!=Owned->TeamIndex && Unit.IsAlive() &&
                    Unit.Presence!=EOGCombatPresence::Removed;
            });
        const FString Outcome=bOwnedSurvived && !bEnemySurvived
            ?TEXT("VICTORY") :!bOwnedSurvived && bEnemySurvived
            ?TEXT("DEFEAT") :TEXT("ENCOUNTER RESOLVED");
        const float W=Canvas->ClipX,S=FMath::Clamp(Scale,.52f,1.3f);
        DrawGamePanel(FLinearColor(.02f,.018f,.028f,.86f),
            W*.22f,20.f*S,W*.56f,65.f*S);
        DrawGameText(Outcome,W*.32f,33.f*S,25.f*S,
            FLinearColor(.89f,.83f,.96f,1.f),W*.38f);
        return true;
    }
    if (State.Status != EOGTurnBattleStatus::Running)
        return false;
    const int32 ActingIndex = Battle->SelectNextActingUnitIndex();
    if (!State.Units.IsValidIndex(ActingIndex))
        return false;

    const FOGCombatUnitState& Acting = State.Units[ActingIndex];
    const FOGCombatUnitState* Owned = State.Units.FindByPredicate(
        [Session](const FOGCombatUnitState& U)
        { return U.UnitEntityId == Session->SelectedManifestationId; });
    if (!Owned || !MatchesActualOwnedLuminousBody(Character, *Session, *Owned)) return false;
    const bool bOwnTurn =
        (State.ExecutionMode == EOGCombatExecutionMode::World ||
         Acting.UnitEntityId == Owned->UnitEntityId) && Owned->CanAct();

    // Track REAL native visual actors separately from the executor roster.
    // World streaming may replace source actor instances: cache only weak refs.
    const double Now=GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (State.BattleId != NativeBattleCacheBattleId ||
        Now < NativeBattleLastActorScanSeconds ||
        Now - NativeBattleLastActorScanSeconds > 1.0)
    {
        NativeBattleWorldActors.Reset();
        NativeBattleCacheBattleId=State.BattleId;
        NativeBattleLastActorScanSeconds=Now;
        if (UWorld* World=GetWorld())
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor=*It;
            if (!IsValid(Actor)) continue;
            const UOGCanonicalCombatActorBindingComponent* Binding=
                Actor->FindComponentByClass<UOGCanonicalCombatActorBindingComponent>();
            if (!Binding) continue;
            for (const FOGCombatUnitState& Unit:State.Units)
                if (Binding->IsBoundTo(State.BattleId,Unit.UnitEntityId))
                {
                    NativeBattleWorldActors.Add(Unit.UnitEntityId,Actor);
                    break;
                }
        }
    }

    const float W = Canvas->ClipX, H = Canvas->ClipY;
    const float S = FMath::Clamp(FMath::Min(W/1280.f,H/720.f), .52f, 1.30f);
    const float Edge = 14.f*S;
    const float HeaderHeight = 49.f*S;
    const FLinearColor Pale(.89f,.86f,.91f,1.f);
    const FLinearColor Accent(.79f,.65f,.87f,1.f);
    const FLinearColor Dim(.49f,.48f,.55f,1.f);
    const FLinearColor Panel(.018f,.014f,.031f,.88f);

    DrawNativeCombatImpacts(Scale);
    // Keep the REAL rendered level and actual native actors visible between
    // bands. Never DrawGameBackdrop or DrawFoundationCharacterStandIn here.
    DrawGamePanel(Panel,Edge,Edge,W-2.f*Edge,HeaderHeight);
    DrawGameText(FString::Printf(
        TEXT("LIVE ENCOUNTER  |  ACTION VALUE %lld  |  %s"),
        static_cast<long long>(State.CurrentActionValue),
        bOwnTurn ? TEXT("YOUR TURN") : TEXT("OTHER COMBATANT'S TURN")),
        Edge+16.f*S,Edge+10.f*S,22.f*S,Pale,W*.64f);
    DrawGameText(FString::Printf(TEXT("OWN HP %s / %s"),
        *Owned->CurrentHp.ToDebugString(),*Owned->Stats.MaxHp.ToDebugString()),
        W*.68f,Edge+13.f*S,17.f*S,Accent,W*.29f);

    // Actual battle opponents, never synthetic units: each selectable card
    // refers to a live unit INDEX in the canonical state, not an actor name.
    const float CardW = FMath::Min(166.f*S,(W-2.f*Edge-16.f*S)/6.f);
    const float CardH = 64.f*S;
    const float Top = Edge+HeaderHeight+7.f*S;
    int32 ParticipantOrdinal=0;
    for (int32 I=0; I<State.Units.Num(); ++I)
    {
        const FOGCombatUnitState& Unit = State.Units[I];
        // Never hide an eligible thirteenth (or later) unit from the field
        // just because an earlier prototype assumed two six-member teams.
        if (Unit.Presence!=EOGCombatPresence::Active)
            continue;
        const float X = Edge+(ParticipantOrdinal%6)*(CardW+5.f*S);
        const float CardTop = Top+(ParticipantOrdinal/6)*(CardH+7.f*S);
        const bool bSelected = Character->IsOwnedBattleTargetSelected(Unit.UnitEntityId) &&
            Character->HasNativeOwnedLuminousEncounter();
        const FLinearColor Border = bSelected ? Accent : Dim;
        DrawGamePanel(Panel,X,CardTop,CardW,CardH);
        DrawLine(X,CardTop,X+CardW,CardTop,Border,bSelected?2.5f:1.f);
        const FString Allegiance=Unit.UnitEntityId==Owned->UnitEntityId
            ? TEXT("SELF") :
            Unit.TeamIndex==Owned->TeamIndex ? TEXT("ALLY") : TEXT("OPPONENT");
        DrawGameText(FString::Printf(TEXT("%s %d"),*Allegiance,I+1),
            X+7.f*S,CardTop+7.f*S,17.f*S,Pale,CardW-14.f*S);
        const TWeakObjectPtr<AActor>* Presence=
            NativeBattleWorldActors.Find(Unit.UnitEntityId);
        AActor* ActualBody=Presence ? Presence->Get() : nullptr;
        DrawGameText(!Unit.IsAlive() ? TEXT("DEFEATED") :
            !ActualBody ? TEXT("BODY NOT YET BOUND") :
            bSelected ? TEXT("TARGET SELECTED") : TEXT("SELECT TARGET"),
            X+7.f*S,CardTop+35.f*S,12.f*S,Unit.IsAlive()?Border:Dim,CardW-14.f*S);
        if (ActualBody && Unit.IsAlive() && PlayerOwner)
        {
            FVector Origin, Extent;
            ActualBody->GetActorBounds(false,Origin,Extent);
            const FVector MarkerWorld=Origin+FVector(0.f,0.f,Extent.Z+28.f);
            FVector2D AtScreen;
            if (PlayerOwner->ProjectWorldLocationToScreen(MarkerWorld,AtScreen,true) &&
                AtScreen.X>12.f*S && AtScreen.X<W-12.f*S &&
                AtScreen.Y>100.f*S && AtScreen.Y<H-85.f*S)
            {
                const float R=19.f*S;
                DrawLine(AtScreen.X-R,AtScreen.Y,AtScreen.X+R,AtScreen.Y,Border,2.f);
                DrawLine(AtScreen.X,AtScreen.Y-R,AtScreen.X,AtScreen.Y+R,Border,2.f);
                DrawGameText(FString::Printf(TEXT("%d"),I+1),
                    AtScreen.X-5.f*S,AtScreen.Y-34.f*S,17.f*S,Border,24.f*S);
                if (bOwnTurn)
                    AddFoundationHitBox(
                        FVector2D(AtScreen.X-R,AtScreen.Y-R),
                        FVector2D(R*2.f,R*2.f),
                        FName(*FString::Printf(TEXT("OG.Battle.Target.%d"),I)),
                        true,163);
            }
        }
        if (bOwnTurn && Unit.CanAct())
        {
            AddFoundationHitBox(FVector2D(X,CardTop),FVector2D(CardW,CardH),
                FName(*FString::Printf(TEXT("OG.Battle.Target.%d"),I)),
                true,162);
        }
        ++ParticipantOrdinal;
    }

    // One command source drives both the native UMG widget and the fallback
    // canvas. Never display form-shaped controls without a real installed
    // executor, and never translate a button into a different form's skill.
    FOGNativeBattleCommandSet NativeCommands;
    FString CommandError;
    if (!Character->ReadOwnedLuminousBattleCommands(NativeCommands,CommandError))
    {
        DrawGameText(CommandError.IsEmpty()
            ? TEXT("No executable native combat kit is installed.") : CommandError,
            Edge+6.f*S,H-85.f*S,17.f*S,Accent,W-2.f*Edge);
        return true;
    }
    const int32 CommandCount=NativeCommands.Commands.Num();
    if (CommandCount>0)
    {
        // Multiple rows retain touch targets on narrow Android displays.
        const int32 Columns=FMath::Min(4,CommandCount);
        const int32 Rows=FMath::DivideAndRoundUp(CommandCount,Columns);
        const float Gap=7.f*S,ButtonH=48.f*S;
        const float ButtonW=FMath::Max(1.f,
            (W-2.f*Edge-(Columns-1)*Gap)/Columns);
        const float Top=H-Edge-Rows*(ButtonH+Gap);
        DrawGameText(TEXT("Select a real participant. Area effects expand by source rules."),
            Edge+6.f*S,Top-27.f*S,13.f*S,Accent,W-2.f*Edge);
        for (int32 Index=0;Index<CommandCount;++Index)
        {
            const FOGNativeBattleCommand& Command=NativeCommands.Commands[Index];
            FString ReadyError;
            const bool bExecutable=bOwnTurn && Command.SkillId.IsValid() &&
                Battle->HasNativeActionExecutor(*Owned,Command.SkillId,ReadyError);
            DrawGameButton(Command.Label,
                FName(*FString::Printf(TEXT("OG.Battle.Command.%s"),*Command.Id.ToString())),
                Edge+(Index%Columns)*(ButtonW+Gap),
                Top+(Index/Columns)*(ButtonH+Gap),
                ButtonW,ButtonH,Command.ActiveSlot==0,bExecutable,158);
        }
    }

    // Error messages are actual canonical preflight errors, not invented
    // animation/gameplay consequences.
    if (!FoundationStatusMessage.IsEmpty())
        DrawGameText(FoundationStatusMessage,Edge+5.f*S,
            Bottom-35.f*S,18.f*S,Accent,W-2.f*Edge);
    return true;
}

void AOGWorldPresentationHud::EndPlay(const EEndPlayReason::Type Reason)
{
    if (NativeBattleWidget) { NativeBattleWidget->RemoveFromParent(); NativeBattleWidget=nullptr; }
    Super::EndPlay(Reason);
}
