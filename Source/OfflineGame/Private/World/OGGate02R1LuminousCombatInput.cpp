// Native commands for the actual owned character in the rendered World encounter.
#include "World/OGStartingRegionPresentation.h"
#include "Koikatsu/OGLuminousV3PlayableCharacter.h"
#include "Characters/OGContentCharacterActors.h"
#include "Characters/OGCharacterAppearanceComponent.h"
#include "Combat/OGNativeBattleCommands.h"
#include "Combat/OGWorldAbilityExecutionComponent.h"
#include "Combat/OGCanonicalCombatActorBindingComponent.h"
#include "Interaction/OGInteractionParticipantComponent.h"
#include "Animation/OGSharedMotionControllerComponent.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "Runtime/OGGate12RulerSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

bool AOGWorldPrototypeCharacter::HasNativeOwnedLuminousEncounter() const
{
    const auto* Core=GetGameInstance()?GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>():nullptr;
    const auto* Battle=Core?Core->GetOwnedLuminousTurnBattle():nullptr;
    return Battle && Battle->GetState().Status==EOGTurnBattleStatus::Running;
}
bool AOGWorldPrototypeCharacter::ReadControlledCanonicalBattleUnit(FOGEntityId& Out,FString& Error) const
{
    Out=FOGEntityId(); Error.Reset();
    const auto* Core=GetGameInstance()?GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>():nullptr;
    const auto* Session=GetGameInstance()?GetGameInstance()->GetSubsystem<UOGGate12RulerSessionSubsystem>():nullptr;
    const auto* Battle=Core?Core->GetOwnedLuminousTurnBattle():nullptr;
    if (!GetWorld() || !GetWorld()->IsGameWorld() || !IsLocallyControlled() ||
        !Core || !Core->IsCoreReady() || !Session || !Session->IsBoundToStoredRuler() ||
        Core->GetOwnedBattleRulerId()!=Session->BoundRulerId || !Battle)
    { Error=TEXT("This character has no locally owned encounter."); return false; }
    const auto Id=Session->SelectedManifestationId.IsValid()?Session->SelectedManifestationId:Session->BoundRulerId;
    const auto* Unit=Battle->GetState().Units.FindByPredicate([&](const auto& U){return U.UnitEntityId==Id;});
    if (!Unit) { Error=TEXT("The controlled canonical participant is absent from this encounter."); return false; }
    const auto* Legacy=Cast<AOGLuminousV3PlayableCharacter>(this);
    const auto* Content=Cast<AOGContentPlayableCharacter>(this);
    if (Id==Session->BoundRulerId && !Session->SelectedManifestationId.IsValid())
    {
        const auto* Participant=FindComponentByClass<UOGInteractionParticipantComponent>();
        const auto* Binding=FindComponentByClass<UOGCanonicalCombatActorBindingComponent>();
        if (!Participant || Participant->GetBoundCanonicalEntityId()!=Id ||
            !Binding || !Binding->IsBoundTo(Battle->GetState().BattleId,Id))
        { Error=TEXT("Ruler controls require the actual connected canonical World body."); return false; }
        FOGFoundationCharacterProjection Projection;
        if (!Participant->GetCurrentProjection(Projection,Error) || Projection.Context.EntityId!=Id) return false;
    }
    else
    {
        if (!((Legacy && Legacy->IsBoundToOwnedLuminousCombatUnit(Session->BoundRulerId,Id,Unit->SkillSet.CurrentForm)) ||
            (Content && Content->IsBoundToOwnedCombatUnit(Session->BoundRulerId,Id,Unit->SkillSet.CurrentForm))))
        { Error=TEXT("Battle controls require the actual possessed owned character."); return false; }
        FOGResolvedSkillSet Owned;
        if (!Core->ReadOwnedCharacterSkills(Id,Session->BoundRulerId,Owned,Error) || Owned.CurrentForm!=Unit->SkillSet.CurrentForm)
        { if (Error.IsEmpty()) Error=TEXT("The stored character Version changed."); return false; }
    }
    Out=Id; return true;
}
bool AOGWorldPrototypeCharacter::ReadOwnedLuminousBattleCommands(
    FOGNativeBattleCommandSet& Out,FString& Error) const
{
    Out=FOGNativeBattleCommandSet(); FOGEntityId Id;
    if (!ReadControlledCanonicalBattleUnit(Id,Error)) return false;
    const auto* Battle=GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>()->GetOwnedLuminousTurnBattle();
    const auto* Unit=Battle->GetState().Units.FindByPredicate([&](const auto& U){return U.UnitEntityId==Id;});
    return Unit && FOGNativeBattleCommands::BuildFor(*Unit,Out,Error);
}
bool AOGWorldPrototypeCharacter::SubmitApprovedLuminousWorldAction(int32 Slot,FName Branch,FString& Error)
{
    FOGNativeBattleCommandSet Set;
    if (!ReadOwnedLuminousBattleCommands(Set,Error)) return false;
    const auto* Choice=Set.Commands.FindByPredicate([Slot,Branch](const FOGNativeBattleCommand& C)
        { return !C.bFantasm && C.ActiveSlot==Slot && C.Branch==Branch && C.Mode.IsNone(); });
    if (!Choice) { Error=TEXT("This form has no approved command for that skill or branch."); return false; }
    return SubmitApprovedLuminousWorldCommand(Choice->Id,Error);
}
bool AOGWorldPrototypeCharacter::SubmitApprovedLuminousWorldCommand(FName CommandId,FString& Error)
{
    Error.Reset();
    if (!IsFoundationWorldInputAllowed())
    { Error=TEXT("World battle input is currently suspended."); return false; }
    FOGNativeBattleCommandSet Set;
    if (!ReadOwnedLuminousBattleCommands(Set,Error)) return false;
    const auto* Command=Set.Commands.FindByPredicate([CommandId](const FOGNativeBattleCommand& C) { return C.Id==CommandId; });
    if (!Command) { Error=TEXT("The selected command is not part of this approved kit."); return false; }
    auto* Core=GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    const auto* Encounter=Core->GetOwnedLuminousTurnBattle();
    const FOGEntityId BattleId=Encounter->GetState().BattleId;
    FOGEntityId SourceId;
    if (!ReadControlledCanonicalBattleUnit(SourceId,Error)) return false;
    const auto* Source=Encounter->GetState().Units.FindByPredicate(
        [&SourceId](const FOGCombatUnitState& U) { return U.UnitEntityId==SourceId; });
    if (!Source) { Error=TEXT("The owned battle participant is unavailable."); return false; }
    TArray<FOGEntityId> Selected;
    if (SelectedCanonicalBattleId==BattleId) Selected=SelectedCanonicalTurnTargets;
    if (Selected.IsEmpty() && HasTargetHudProjectionForHud() && GetTargetHudProjectionForHud().TargetEntityId.IsValid())
        Selected.Add(GetTargetHudProjectionForHud().TargetEntityId);
    FOGResolvedCombatAction Proposed,Action;
    Command->BuildAction(*Source,Selected,Proposed);
    if (Encounter->GetState().ExecutionMode==EOGCombatExecutionMode::World)
    {
        auto* Execution=FindComponentByClass<UOGWorldAbilityExecutionComponent>();
        if (!Execution) { Error=TEXT("The actual character has no connected World executor."); return false; }
        return Execution->BeginNativeAction(Proposed,false,Error);
    }
    if (!Encounter->ValidateResolvedAction(Proposed,Action,Error)) return false;
    for (const auto& Id:Action.TargetUnitIds)
    {
        const auto* Target=Encounter->GetState().Units.FindByPredicate(
            [&Id](const FOGCombatUnitState& U) { return U.UnitEntityId==Id; });
        // ValidateResolvedAction already verified the exact source-specific
        // revival window/soul gates. Do not impose an unrelated living-only
        // UI restriction on its legitimate defeated allied recipients.
        const bool bDefeatedAlly=Target &&
            Target->Presence==EOGCombatPresence::Defeated &&
            !Target->IsAlive() && Target->TeamIndex==Source->TeamIndex;
        if (!Target || (!Target->CanAct() && !bDefeatedAlly))
        { Error=TEXT("Selected participant is not an eligible living target or defeated ally."); return false; }
        if (!bDefeatedAlly && Id!=SourceId &&
            !UOGCanonicalCombatActorBindingComponent::FindActualWorldActor(GetWorld(),BattleId,Id))
        { Error=TEXT("A required living battle character is unavailable in the World."); return false; }
    }
    const auto BeforeSourceHp=Source->CurrentHp;
    const FName Semantic=Command->MotionSemantic();
    const bool UseAuthoredMap=Command->Branch.IsNone() && Command->Mode.IsNone();
    const FOGContentId Skill=Command->SkillId;
    Action.ActionId=FOGEntityId::NewId();
    if (!Core->ApplyApprovedLuminousResolvedAction(Action,Error)) return false;
    ClearSelectedCanonicalTurnTargets();
    const auto* Updated=Core->GetOwnedLuminousTurnBattle();
    if (!Updated || Updated->GetState().BattleId!=BattleId) return true;
    const auto* SurvivingSource=Updated->GetState().Units.FindByPredicate([&](const auto& U){return U.UnitEntityId==SourceId;});
    if (!SurvivingSource) return true;
    const auto* BoundBody=FindComponentByClass<UOGCanonicalCombatActorBindingComponent>();
    if (BoundBody && BoundBody->IsBoundTo(BattleId,SourceId)) return true;
    // Preserve the verified owned legacy adapter when no canonical visual binding was installed.
    auto* Appearance=FindComponentByClass<UOGCharacterAppearanceComponent>();
    auto* SourceMotion=Appearance?Appearance->GetMotionController():nullptr;
    if (!SourceMotion) SourceMotion=FindComponentByClass<UOGSharedMotionControllerComponent>();
    if (BeforeSourceHp!=SurvivingSource->CurrentHp)
    {
        if (auto* Player=Cast<APlayerController>(GetController()))
            if (auto* Hud=Cast<AOGWorldPresentationHud>(Player->GetHUD()))
                Hud->RegisterCanonicalCombatImpact(this,BeforeSourceHp,SurvivingSource->CurrentHp);
        if (SourceMotion)
        {
            if (!SurvivingSource->IsAlive()) SourceMotion->PlayAction(TEXT("Combat.Reaction.Death"),TEXT("Combat"),false,1);
            else SourceMotion->EnqueueCombatCue(FOGLargeNumber::Compare(SurvivingSource->CurrentHp,BeforeSourceHp)<0?
                FName(TEXT("Combat.Reaction.Hit")):FName(TEXT("Combat.Reaction.Heal")),BattleId,Action.ActionId,SourceId);
        }
    }
    if (!SurvivingSource->IsAlive()) return true;
    for (const auto& TargetId:Action.TargetUnitIds)
        if (auto* Target=UOGCanonicalCombatActorBindingComponent::FindActualWorldActor(GetWorld(),BattleId,TargetId); Target && Target!=this)
        {
            const FVector Direction=Target->GetActorLocation()-GetActorLocation();
            if (!Direction.IsNearlyZero()) SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0));
            break;
        }
    if (auto* Legacy=Cast<AOGLuminousV3PlayableCharacter>(this)) Legacy->PlaySharedGameplayAction(Semantic);
    if (auto* Content=Cast<AOGContentPlayableCharacter>(this); Content && Content->Appearance)
    {
        const FName* Authored=UseAuthoredMap?Content->CombatActionSemantics.Find(Skill):nullptr;
        const FName Key=Authored?*Authored:Semantic;
        auto* Motion=Content->Appearance->GetMotionController();
        if (!Motion || !Motion->IsPlayingAction() || Motion->CurrentAction!=Key)
        { FString MotionError; Content->Appearance->PlaySemanticAction(Key,TEXT("Combat"),false,1,NAME_None,MotionError); }
    }
    return true;
}
