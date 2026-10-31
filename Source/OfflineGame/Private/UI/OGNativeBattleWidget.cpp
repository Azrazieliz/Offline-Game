#include "UI/OGNativeBattleWidget.h"
#include "UI/OGReactiveUiSubsystem.h"
#include "UI/OGNativeUiStyle.h"
#include "Misc/ScopeExit.h"

#include "World/OGStartingRegionPresentation.h"
#include "Combat/OGNativeBattleCommands.h"
#include "Combat/OGTurnBattle.h"
#include "Combat/OGCanonicalCombatActorBindingComponent.h"
#include "EngineUtils.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Runtime/OGGate12RulerSessionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"

namespace
{
FSlateFontInfo BattleFont(int32 Size,bool Title=false)
{
    return FSlateFontInfo(Title?FPaths::ProjectContentDir()/TEXT("Slate/Fonts/Cinzel-Bold.ttf"):
        FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-Medium.ttf"),static_cast<float>(Size));
}
float HealthFraction(const FOGLargeNumber& Current,const FOGLargeNumber& Maximum)
{
    if (Current.GetSign()<=0||Maximum.GetSign()<=0) return 0.f;
    if (FOGLargeNumber::Compare(Current,Maximum)>=0) return 1.f;
    const int64 Difference=static_cast<int64>(Current.Exponent10)-Maximum.Exponent10;
    if (Difference < -38) return 0.f; // Below display precision, without overflowing combat-scale numbers.
    if (Difference > 38) return 1.f;
    const double Ratio=(static_cast<double>(Current.Significand)/Maximum.Significand)*FMath::Pow(10.0,static_cast<double>(Difference));
    return FMath::IsFinite(Ratio)?static_cast<float>(FMath::Clamp(Ratio,0.0,1.0)):0.f;
}
const FLinearColor Gold(.82f,.73f,.57f,1);
const FLinearColor Pale(.93f,.91f,.95f,1);
}
void UOGNativeBattleButton::Invoke() { if (Screen) Screen->Activate(Command,TargetIndex); }
bool UOGNativeBattleWidget::IsForCharacter(const AOGWorldPrototypeCharacter* InCharacter) const { return Character.Get()==InCharacter; }
void UOGNativeBattleWidget::BindCharacter(AOGWorldPrototypeCharacter* InCharacter)
{
    if (Character.Get()==InCharacter) return;
    Character=InCharacter; Signature.Reset(); LastActionMessage.Reset(); Refresh();
}
void UOGNativeBattleWidget::NativeConstruct()
{
    ON_SCOPE_EXIT { if (auto* Theme=UOGReactiveUiSubsystem::Find(this)) Theme->TrackTree(this); };

    Super::NativeConstruct();
    if (!WidgetTree || WidgetTree->RootWidget) return;
    auto* Safe=WidgetTree->ConstructWidget<USafeZone>();
    Safe->SetVisibility(ESlateVisibility::SelfHitTestInvisible); WidgetTree->RootWidget=Safe;
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); Safe->AddChild(Canvas);
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Panel=[&](float Top,float Bottom,UCanvasPanelSlot*& Out)
    {
        auto* Frame=WidgetTree->ConstructWidget<UBorder>();
        Frame->SetBrushColor(FLinearColor(.011f,.008f,.024f,.94f));
        Frame->SetPadding(FMargin(16,12)); Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Out=Canvas->AddChildToCanvas(FOGNativeUiStyle::Surface(*WidgetTree,Frame)); Out->SetAnchors(FAnchors(.02f,Top,.98f,Bottom));
        Out->SetOffsets(FMargin(0)); return Frame;
    };
    UCanvasPanelSlot* HeaderSlot=nullptr; auto* Header=Panel(.02f,.25f,HeaderSlot);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); Header->AddChild(Column);
    Heading=WidgetTree->ConstructWidget<UTextBlock>(); Heading->SetFont(BattleFont(22,true));
    Heading->SetColorAndOpacity(Gold); Heading->SetAutoWrapText(true); Column->AddChildToVerticalBox(Heading);
    Resources=WidgetTree->ConstructWidget<UTextBlock>(); Resources->SetFont(BattleFont(16));
    Resources->SetColorAndOpacity(Pale); Resources->SetAutoWrapText(true);
    Column->AddChildToVerticalBox(Resources)->SetPadding(FMargin(0,4,0,8));
    auto AddMeter=[&](UProgressBar*& Meter,float Height)
    {
        Meter=WidgetTree->ConstructWidget<UProgressBar>();
        Meter->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* Bounds=WidgetTree->ConstructWidget<USizeBox>();Bounds->SetHeightOverride(Height);Bounds->AddChild(Meter);
        Column->AddChildToVerticalBox(Bounds)->SetPadding(FMargin(0,0,0,5));
    };
    UProgressBar* Health=nullptr;UProgressBar* Ultimate=nullptr;
    AddMeter(Health,5);AddMeter(Ultimate,3);HealthBar=Health;UltimateBar=Ultimate;
    Targets=WidgetTree->ConstructWidget<UScrollBox>(); Targets->SetOrientation(Orient_Horizontal);
    Column->AddChildToVerticalBox(Targets)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UCanvasPanelSlot* LowerSlot=nullptr; auto* Lower=Panel(.61f,.98f,LowerSlot); ActionPanelSlot=LowerSlot;
    auto* LowerColumn=WidgetTree->ConstructWidget<UVerticalBox>(); Lower->AddChild(LowerColumn);
    Feedback=WidgetTree->ConstructWidget<UTextBlock>(); Feedback->SetFont(BattleFont(16));
    Feedback->SetColorAndOpacity(Gold); Feedback->SetAutoWrapText(true);
    LowerColumn->AddChildToVerticalBox(Feedback)->SetPadding(FMargin(0,0,0,8));
    ActionScroll=WidgetTree->ConstructWidget<UScrollBox>();
    LowerColumn->AddChildToVerticalBox(ActionScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Commands=WidgetTree->ConstructWidget<UWrapBox>(); Commands->SetExplicitWrapSize(true);
    Commands->SetInnerSlotPadding(FVector2D(8,8)); ActionScroll->AddChild(Commands);
    Refresh();
}
void UOGNativeBattleWidget::AddButton(UPanelWidget* Parent,const FString& Label,const FString& Hint,
    FName Command,int32 TargetIndex,bool Enabled,bool Selected,float Width)
{
    auto* Bounds=WidgetTree->ConstructWidget<USizeBox>();
    Bounds->SetWidthOverride(Width); Bounds->SetMinDesiredHeight(68);
    Parent->AddChild(Bounds);
    auto* Button=WidgetTree->ConstructWidget<UOGNativeBattleButton>();
    Button->Screen=this; Button->Command=Command; Button->TargetIndex=TargetIndex;
    // Native touch, keyboard and gamepad share the same authoritative action.
    Button->SetIsFocusable(true); Button->SetIsEnabled(Enabled); Button->SetToolTipText(FText::FromString(Hint));
    Button->SetBackgroundColor(Selected?FLinearColor(.26f,.17f,.31f,1):FLinearColor(.07f,.045f,.105f,1));
    Button->OnClicked.AddDynamic(Button,&UOGNativeBattleButton::Invoke);
    FOGNativeUiStyle::SkinButton(*Button);
    Bounds->AddChild(FOGNativeUiStyle::Surface(*WidgetTree,Button,Selected?EOGNativeSurface::PrimaryAction:EOGNativeSurface::Action,Selected));
    auto* Text=WidgetTree->ConstructWidget<UTextBlock>(); Text->SetFont(BattleFont(17));
    Text->SetAutoWrapText(true); Text->SetText(FText::FromString(Label));
    Text->SetColorAndOpacity(Selected?Gold:Enabled?Pale:FLinearColor(.49f,.47f,.53f,1));
    if (auto* Slot=Cast<UButtonSlot>(Button->AddChild(Text)))
    { Slot->SetPadding(FMargin(12,10)); Slot->SetVerticalAlignment(VAlign_Center); }
}
void UOGNativeBattleWidget::Refresh()
{
    const FString ThemeSignatureBefore=Signature;
    ON_SCOPE_EXIT { if (Signature!=ThemeSignatureBefore) if (auto* Theme=UOGReactiveUiSubsystem::Find(this)) Theme->TrackTree(this); };

    if (!Heading || !Character.IsValid()) return;
    auto* Body=Character.Get();
    const auto* GI=Body->GetGameInstance();
    const auto* Core=GI?GI->GetSubsystem<UOGGameCoreSubsystem>():nullptr;
    const auto* Session=GI?GI->GetSubsystem<UOGGate12RulerSessionSubsystem>():nullptr;
    const auto* Battle=Core?Core->GetOwnedLuminousTurnBattle():nullptr;
    if (!Core || !Core->IsCoreReady() || !Battle || !Session ||
        Core->GetOwnedBattleRulerId()!=Session->BoundRulerId ||
        Battle->GetState().Status==EOGTurnBattleStatus::NotStarted)
    { SetVisibility(ESlateVisibility::Collapsed); return; }
    const auto& State=Battle->GetState();
    if (State.Status==EOGTurnBattleStatus::Completed)
    {
        const auto& Outcome=Core->GetLastNativeBattleOutcome();
        if (Outcome.BattleId!=State.BattleId || !Outcome.CompletionEventId.IsValid() || DismissedResultReceipt==Outcome.CompletionEventId)
        { SetVisibility(ESlateVisibility::Collapsed); return; }
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        const FString ResultKey=TEXT("result|")+Outcome.CompletionEventId.ToString()+FString::Printf(TEXT("|%d|%d"),FMath::RoundToInt(ViewWidth),FMath::RoundToInt(ViewHeight));
        if (Signature==ResultKey) return;
        Signature=ResultKey; Targets->ClearChildren(); Commands->ClearChildren();
        Heading->SetText(FText::FromString(!Outcome.bHasPlayerTeam?TEXT("ENCOUNTER COMPLETE"):Outcome.bVictory?TEXT("VICTORY"):TEXT("DEFEAT")));
        Resources->SetText(FText::GetEmpty());
        if (HealthBar) HealthBar->SetVisibility(ESlateVisibility::Collapsed);
        if (UltimateBar) UltimateBar->SetVisibility(ESlateVisibility::Collapsed);
        Feedback->SetText(FText::FromString(Outcome.RewardEventIds.IsEmpty()?TEXT("Encounter complete."):TEXT("Rewards saved.")));
        const float Width=FMath::Max(200.f,ViewWidth*.90f-32.f); Commands->SetWrapSize(Width);
        AddButton(Commands,TEXT("CONTINUE"),TEXT("Return to the World."),TEXT("screen_results_close"),INDEX_NONE,true,false,Width);
        AddButton(Commands,TEXT("RULER"),TEXT("Open your Ruler screen."),TEXT("screen_ruler"),INDEX_NONE,true,false,Width);
        for (const auto& Line:Outcome.RewardLines)
        {
            auto* Bounds=WidgetTree->ConstructWidget<USizeBox>(); Bounds->SetWidthOverride(Width); Bounds->SetMinDesiredHeight(40);
            auto* Text=WidgetTree->ConstructWidget<UTextBlock>(); Text->SetFont(BattleFont(17)); Text->SetColorAndOpacity(Gold);
            Text->SetAutoWrapText(true); Text->SetText(FText::FromString(Line)); Bounds->AddChild(Text); Commands->AddChildToWrapBox(Bounds);
        }
        return;
    }
    FOGEntityId Controlled; FString ControlError;
    if (!Body->ReadControlledCanonicalBattleUnit(Controlled,ControlError))
    { SetVisibility(ESlateVisibility::Collapsed); return; }
    const auto* Source=State.Units.FindByPredicate([&](const auto& U){return U.UnitEntityId==Controlled;});
    if (!Source) { SetVisibility(ESlateVisibility::Collapsed); return; }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    int32 Resource=0,Charge=0,OwnerActions=0;
    Battle->ReadLuminousCommandResources(Source->UnitEntityId,Resource,Charge,OwnerActions);
    FString Key=State.BattleId.ToString()+Source->UnitEntityId.ToString()+
        FString::Printf(TEXT("|%lld|%d|%d|%lld|%d|%d|"),static_cast<long long>(Battle->GetLog().GetNextSequence()),
            Resource,Charge,static_cast<long long>(Core->GetCanonicalWorldTick()/1000),
            FMath::RoundToInt(ViewWidth),FMath::RoundToInt(ViewHeight))+LastActionMessage;
    if (State.ExecutionMode==EOGCombatExecutionMode::World) Key+=LexToString(State.WorldElapsedMilliseconds/100);
    for (const auto& Id:Body->GetSelectedCanonicalTurnTargets()) Key+=Id.ToString();
    if (Body->HasTargetHudProjectionForHud()) Key+=Body->GetTargetHudProjectionForHud().TargetEntityId.ToString();
    Key+=Core->NativeEncounterRecoveryStatus;
    if (Key==Signature) return;
    Signature=Key;
    FOGNativeBattleCommandSet Set; FString Error;
    if (!Body->ReadOwnedLuminousBattleCommands(Set,Error))
    { Heading->SetText(FText::FromString(TEXT("ENCOUNTER"))); Feedback->SetText(FText::FromString(Error)); Resources->SetText(FText::GetEmpty()); Targets->ClearChildren(); Commands->ClearChildren(); return; }
    const bool WorldMode=State.ExecutionMode==EOGCombatExecutionMode::World;
    const int32 Acting=Battle->SelectNextActingUnitIndex();
    const bool Own=(WorldMode || (State.Units.IsValidIndex(Acting) && State.Units[Acting].UnitEntityId==Source->UnitEntityId)) &&
        Source->CanAct() && Body->IsFoundationWorldInputAllowed() && !Core->IsNativeEncounterAwaitingWorldRebind();
    Heading->SetText(FText::FromString(WorldMode?TEXT("WORLD COMBAT"):Own?TEXT("YOUR TURN"):TEXT("OPPONENT / ALLY TURN")));
    FString ResourceText=FString::Printf(TEXT("HP %s / %s"),*Source->CurrentHp.ToDebugString(),*Source->Stats.MaxHp.ToDebugString());
    if (!Set.ResourceLabel.IsEmpty())
        ResourceText+=FString::Printf(TEXT("   |   %s %d / %d   |   Ultimate %d%% / %d%%"),
            *Set.ResourceLabel,Resource,Set.ResourceCap,Charge,Set.ChargeCap);
    for (const auto& Value:Source->BattleResources)
        ResourceText+=FString::Printf(TEXT("   |   %s %lld / %lld"),*Value.DisplayName.ToString(),
            static_cast<long long>(Value.Value),static_cast<long long>(Value.Maximum));
    if (WorldMode)
    {
        FOGWorldAbilityCastView Cast;
        if (Battle->ReadWorldCast(Battle->GetUnitWorldCast(Source->UnitEntityId),Cast))
            ResourceText+=FString::Printf(TEXT("   |   Action %.1f / %.1f s"),
                (State.WorldElapsedMilliseconds-Cast.StartedAtMilliseconds)/1000.0,
                (Cast.EndsAtMilliseconds-Cast.StartedAtMilliseconds)/1000.0);
    }
    const FString Status=Battle->DescribeNativeEffects(Source->UnitEntityId);
    if (!Status.IsEmpty()) ResourceText+=TEXT("\n")+Status;
    Resources->SetText(FText::FromString(ResourceText));
    if (HealthBar)
    {
        const float Fraction=HealthFraction(Source->CurrentHp,Source->Stats.MaxHp);
        HealthBar->SetVisibility(ESlateVisibility::HitTestInvisible);HealthBar->SetPercent(Fraction);
        HealthBar->SetFillColorAndOpacity(Fraction<=.25f?FLinearColor(.82f,.25f,.30f,1):FLinearColor(.28f,.72f,.60f,1));
    }
    if (UltimateBar)
    {
        UltimateBar->SetVisibility(Set.ChargeCap>0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        UltimateBar->SetPercent(Set.ChargeCap>0?FMath::Clamp(static_cast<float>(Charge)/Set.ChargeCap,0.f,1.f):0.f);
        UltimateBar->SetFillColorAndOpacity(UOGReactiveUiSubsystem::Find(this)?UOGReactiveUiSubsystem::Find(this)->GetPalette().Accent:Gold);
    }
    Feedback->SetText(FText::FromString(LastActionMessage.IsEmpty()?
        TEXT("Choose a primary target. Area effects include every eligible field participant."):LastActionMessage));
    if (!Own && State.Units.IsValidIndex(Acting))
    {
        bool Executable=false; FString Missing;
        const auto& Next=State.Units[Acting];
        for (const auto& Skill:Next.SkillSet.ActiveSkills)
            if (Battle->HasNativeActionExecutor(Next,Skill,Missing)) { Executable=true; break; }
        if (!Executable)
            for (const auto& Ultimate:Next.SkillSet.GetUltimateSkills())
                if (Battle->HasNativeActionExecutor(Next,Ultimate,Missing)) { Executable=true; break; }
        if (!Executable) Feedback->SetText(FText::FromString(TEXT("Waiting for this combatant's authored abilities.")));
    }
    if (Core->IsNativeEncounterAwaitingWorldRebind())
        Feedback->SetText(FText::FromString(Core->NativeEncounterRecoveryStatus));
    const float TargetOffset=Targets->GetScrollOffset(),ActionOffset=ActionScroll->GetScrollOffset();
    Targets->ClearChildren(); Commands->ClearChildren();
    // One actor pass per refresh; expose only existing native battle bodies.
    TMap<FOGEntityId,AActor*> Bodies; TSet<FOGEntityId> AmbiguousBodies;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        AActor* Actor=*It; if (!IsValid(Actor)) continue;
        const auto* Binding=Actor->FindComponentByClass<UOGCanonicalCombatActorBindingComponent>();
        if (!Binding || Binding->GetBoundBattleId()!=State.BattleId || !Binding->GetBoundUnitId().IsValid()) continue;
        const auto Id=Binding->GetBoundUnitId();
        if (Bodies.Contains(Id)) AmbiguousBodies.Add(Id);
        else Bodies.Add(Id,Actor);
    }
    if (!Bodies.Contains(Source->UnitEntityId)) Bodies.Add(Source->UnitEntityId,Body);
    for (int32 I=0;I<State.Units.Num();++I)
    {
        const auto& Unit=State.Units[I];
        const bool bDefeatedAlly=Unit.Presence==EOGCombatPresence::Defeated &&
            !Unit.IsAlive() && Unit.TeamIndex==Source->TeamIndex;
        if (Unit.Presence!=EOGCombatPresence::Active && !bDefeatedAlly) continue;
        const bool Selected=Body->IsOwnedBattleTargetSelected(Unit.UnitEntityId);
        const FString Side=Unit.UnitEntityId==Source->UnitEntityId?TEXT("SELF"):Unit.TeamIndex==Source->TeamIndex?TEXT("ALLY"):TEXT("OPPONENT");
        const bool Embodied=Bodies.Contains(Unit.UnitEntityId) && !AmbiguousBodies.Contains(Unit.UnitEntityId);
        const FString Suffix=bDefeatedAlly?TEXT(" / DEFEATED - REVIVAL ELIGIBILITY CHECKED ON CAST"):
            !Embodied?TEXT(" / CHARACTER UNAVAILABLE"):Selected?TEXT(" / SELECTED"):TEXT("");
        FString HpLine=Unit.TeamIndex==Source->TeamIndex
            ?FString::Printf(TEXT("HP %s / %s"),
                *Unit.CurrentHp.ToDebugString(),*Unit.Stats.MaxHp.ToDebugString())
            :TEXT("HP Unknown");
        // Never expose unobserved opponent HP from the privileged battle
        // resolver. Use only the actual knowledge-scoped HUD projection.
        if (Unit.TeamIndex!=Source->TeamIndex)
        {
            const FOGWorldTargetViewModel* Observed=nullptr;
            if (Body->HasTargetHudProjectionForHud() &&
                Body->GetTargetHudProjectionForHud().TargetEntityId==Unit.UnitEntityId)
                Observed=&Body->GetTargetHudProjectionForHud();
            if (!Observed)
                for (const auto& Other:Body->GetSecondaryTargetHudProjectionsForHud())
                    if (Other.TargetEntityId==Unit.UnitEntityId) { Observed=&Other;break; }
            if (Observed && !Observed->HpDisplay.IsEmpty())
                HpLine=Observed->HpDisplay;
        }
        AddButton(Targets,FString::Printf(TEXT("%s %d%s\n%s"),
                *Side,I+1,*Suffix,*HpLine),
            TEXT("Select an actual participant; the canonical ability still checks target and revival eligibility."),
            NAME_None,I,Own && (bDefeatedAlly || (Unit.CanAct() && Embodied)),Selected,176);
    }
    const bool Portrait=ViewWidth<ViewHeight;
    const float Available=FMath::Max(200.f,ViewWidth*.96f-32.f);
    Commands->SetWrapSize(Available);
    const float Width=Portrait?Available:FMath::Max(180.f,(Available-16.f)/3.f);
    TArray<FOGEntityId> Selected=Body->GetSelectedCanonicalTurnTargets();
    if (Selected.IsEmpty() && Body->HasTargetHudProjectionForHud()) Selected.Add(Body->GetTargetHudProjectionForHud().TargetEntityId);
    for (const auto& Command:Set.Commands)
    {
        FOGResolvedCombatAction Proposal,Expanded; Command.BuildAction(*Source,Selected,Proposal);
        FString Reason; bool Enabled=false;
        if (WorldMode)
        {
            const auto* Timing=Battle->FindWorldAbility(*Source,Command.SkillId);
            Enabled=Own && Timing && Battle->CanBeginWorldAbility(Proposal,Reason);
            Expanded=Proposal;
            if (!Timing) Reason=TEXT("World staging unavailable.");
            else if (!Enabled && Reason.IsEmpty()) Reason=TEXT("Current action or cooldown is active.");
            const int64 Cooldown=Battle->GetWorldAbilityReadyAt(Source->UnitEntityId,Command.SkillId)-State.WorldElapsedMilliseconds;
            if (Cooldown>0) Reason=FString::Printf(TEXT("Ready in %.1f s"),Cooldown/1000.0);
        }
        else Enabled=Own && Battle->ValidateResolvedAction(Proposal,Expanded,Reason);
        if (Enabled) for (const auto& Id:Expanded.TargetUnitIds)
            if (!Bodies.Contains(Id) || AmbiguousBodies.Contains(Id))
            { Enabled=false; Reason=TEXT("Waiting for the battlefield characters."); break; }
        if (!Own) Reason=TEXT("Available on this character's next turn.");
        if (!WorldMode && Own && !Enabled && Expanded.TargetUnitIds.IsEmpty()) Reason=TEXT("Select a primary target.");
        const FString Label=Command.Label+(Own && !Enabled?TEXT("\n")+Reason:FString());
        AddButton(Commands,Label,Enabled?Command.Hint:Reason,Command.Id,INDEX_NONE,Enabled,false,Width);
    }
    FString ModeReason;
    const bool CanChangeMode=Core->CanSetOwnedCombatExecutionMode(
        WorldMode?EOGCombatExecutionMode::Turn:EOGCombatExecutionMode::World,ModeReason);
    AddButton(Commands,WorldMode?TEXT("TURN MODE"):TEXT("WORLD MODE"),
        CanChangeMode?TEXT("Keep the current combatants and state."):ModeReason,
        TEXT("screen_combat_mode"),INDEX_NONE,CanChangeMode,false,Width);
    AddButton(Commands,TEXT("Pause"),TEXT("Open the existing pause controls."),TEXT("screen_pause"),INDEX_NONE,true,false,Width);
    AddButton(Commands,TEXT("Settings"),TEXT("Open the existing game settings."),TEXT("screen_settings"),INDEX_NONE,true,false,Width);
    AddButton(Commands,TEXT("Ruler"),TEXT("Return to the existing Ruler interface."),TEXT("screen_ruler"),INDEX_NONE,true,false,Width);
    Targets->SetScrollOffset(TargetOffset); ActionScroll->SetScrollOffset(ActionOffset);
}
void UOGNativeBattleWidget::Activate(FName Command,int32 TargetIndex)
{
    if (!Character.IsValid()) return;
    if (Command==TEXT("screen_results_close"))
    {
        const auto* Game=Character->GetGameInstance(); const auto* Core=Game?Game->GetSubsystem<UOGGameCoreSubsystem>():nullptr;
        if (Core) DismissedResultReceipt=Core->GetLastNativeBattleOutcome().CompletionEventId;
        SetVisibility(ESlateVisibility::Collapsed); return;
    }
    if (Command==TEXT("screen_combat_mode"))
    {
        auto* Game=Character->GetGameInstance(); auto* Core=Game?Game->GetSubsystem<UOGGameCoreSubsystem>():nullptr;
        FString Error;
        if (Core && Core->GetOwnedLuminousTurnBattle())
        {
            const auto Mode=Core->GetOwnedLuminousTurnBattle()->GetState().ExecutionMode;
            Core->SetOwnedCombatExecutionMode(Mode==EOGCombatExecutionMode::World?EOGCombatExecutionMode::Turn:EOGCombatExecutionMode::World,Error);
        }
        LastActionMessage=Error; Signature.Reset(); Refresh(); return;
    }
    if (Command==TEXT("screen_pause")) { Character->SetFoundationPauseMenuOpen(true); return; }
    if (Command==TEXT("screen_settings")) { Character->SetFoundationSettingsOpen(true); return; }
    if (Command==TEXT("screen_ruler"))
    {
        if (auto* Player=Cast<APlayerController>(Character->GetController()))
            if (auto* Hud=Cast<AOGWorldPresentationHud>(Player->GetHUD())) Hud->NotifyHitBoxClick(TEXT("OG.World.Ruler"));
        return;
    }
    FString Error; const bool Accepted=TargetIndex!=INDEX_NONE?
        Character->SelectOwnedLuminousBattleTarget(TargetIndex,Error):
        Character->SubmitApprovedLuminousWorldCommand(Command,Error);
    LastActionMessage=Accepted?FString():Error;
    Signature.Reset(); Refresh();
}
void UOGNativeBattleWidget::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    const auto Size=Geometry.GetLocalSize(); ViewWidth=Size.X; ViewHeight=Size.Y;
    // Changing Canvas anchors every frame invalidates layout and can
    // interrupt gamepad focus on Android. Re-anchor only when the screen
    // actually changes between portrait and landscape.
    const bool bPortrait=Size.X<Size.Y;
    if (ActionPanelSlot && (!bActionAnchorsApplied || bActionPortraitLayout!=bPortrait))
    {
        ActionPanelSlot->SetAnchors(FAnchors(.02f,bPortrait?.55f:.61f,.98f,.98f));
        bActionAnchorsApplied=true;
        bActionPortraitLayout=bPortrait;
    }
    RefreshElapsed+=DeltaSeconds;
    if (RefreshElapsed>=.2f) { RefreshElapsed=0; Refresh(); }
}
