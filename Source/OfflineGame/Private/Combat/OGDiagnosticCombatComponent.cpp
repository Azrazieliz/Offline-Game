#include "Combat/OGDiagnosticCombatComponent.h"
#include "Combat/OGDiagnosticGacha.h"
#include "Combat/OGDiagnosticHumanoid.h"
#include "Combat/OGResolvedWorldDamageTarget.h"
#include "Interaction/OGFoundationInteractionHost.h"
#include "Runtime/OGFoundationCharacterDiagnostics.h"
#include "World/OGStartingRegionPresentation.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Runtime/OGPresentationModeSubsystem.h"
#include "Runtime/OGGameCoreSubsystem.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

UOGDiagnosticCombatComponent::UOGDiagnosticCombatComponent()
{ PrimaryComponentTick.bCanEverTick = true; }
AOGWorldPrototypeCharacter* UOGDiagnosticCombatComponent::GetPlayer() const
{ return Cast<AOGWorldPrototypeCharacter>(GetOwner()); }
UOGWorldPartyRuntimeComponent* UOGDiagnosticCombatComponent::GetParty() const
{ return GetOwner() ? GetOwner()->FindComponentByClass<UOGWorldPartyRuntimeComponent>() : nullptr; }
FOGEntityId UOGDiagnosticCombatComponent::GetRulerId() const { return OGDiagnosticGacha::ScopedId(1); }

AActor* UOGDiagnosticCombatComponent::GetProtagonistWorldActor() const
{
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (Party && Party->GetSlots().IsValidIndex(Party->GetControlledSlot()) &&
        Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId == GetRulerId()) return GetOwner();
    return ProtagonistWorldActor.Get();
}
FVector UOGDiagnosticCombatComponent::GetProtagonistWorldPosition() const
{
    if (AActor* Actor = GetProtagonistWorldActor()) return Actor->GetActorLocation();
    return RulerWorldPosition;
}

bool UOGDiagnosticCombatComponent::TryGetControlledContext(FOGEntityId& Entity,
    FOGContentId& Identity, bool& bHasManifestation, FOGCharacterManifestationRecord& Manifestation, FString& Error) const
{
    Error.Reset(); Entity = FOGEntityId(); Identity = FOGContentId(); bHasManifestation = false;
    Manifestation = FOGCharacterManifestationRecord();
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    { Error = TEXT("No controlled living party entity."); return false; }
    const FOGWorldPartySlot& Slot = Party->GetSlots()[Party->GetControlledSlot()];
    if (!Slot.bAvailable || Slot.bDefeated || !Slot.Unit.IsAlive())
    { Error = TEXT("Controlled party entity is unavailable."); return false; }
    Entity = Slot.Unit.UnitEntityId; Identity = Slot.Unit.IdentityId;
    return !Store || Store->TryReadCharacterManifestation(Entity, bHasManifestation, Manifestation, Error);
}

int32 UOGDiagnosticCombatComponent::GetBasicChainForHud() const
{
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot())) return 0;
    const FOGEntityId Id = Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId;
    return GetWorld()->GetTimeSeconds() <= BasicChainEndsAt.FindRef(Id) ? BasicChain.FindRef(Id) : 0;
}

bool UOGDiagnosticCombatComponent::CanAcceptQte(int32 AssistSlot, AActor* Target, FString& Error) const
{
    Error.Reset();
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    const AOGDiagnosticHumanoid* Enemy = Cast<AOGDiagnosticHumanoid>(Target);
    if (IsSuspended() || !Party || !Party->GetSlots().IsValidIndex(AssistSlot) ||
        AssistSlot == Party->GetControlledSlot() || !Party->GetSlots()[AssistSlot].bQteReady ||
        !Party->GetSlots()[AssistSlot].bAvailable || Party->GetSlots()[AssistSlot].bDefeated ||
        !Enemy || Enemy->IsTurnStation() || !Enemy->GetCombatState()->GetSnapshot().IsAlive() ||
        FVector::Dist(GetOwner()->GetActorLocation(), Enemy->GetActorLocation()) > 500 || !Enemy->HasLineOfSight(GetOwner()))
    { Error = TEXT("Companion QTE requires readiness and a living target in assist range/line of sight."); return false; }
    return true;
}

void UOGDiagnosticCombatComponent::RegisterScenarioActor(AActor* Actor)
{
    if (!Actor) return;
    ScenarioActors.AddUnique(Actor);
    if (GetWorld() && GetWorld()->GetGameInstance())
        if (UOGPresentationModeSubsystem* Presentation = GetWorld()->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
            Presentation->RegisterWorldPresentationActor(Actor);
}

TArray<FOGCombatUnitState> UOGDiagnosticCombatComponent::GetWorldParticipantsForInteraction() const
{
    if (bLastEncounterWasTurn && Turn.GetState().Status == EOGTurnBattleStatus::Completed) return Turn.GetState().Units;
    TArray<FOGCombatUnitState> Participants;
    if (const UOGWorldPartyRuntimeComponent* Party = GetParty())
        for (const FOGWorldPartySlot& Slot : Party->GetSlots()) Participants.Add(Slot.Unit);
    for (TWeakObjectPtr<AActor> Actor : ScenarioActors)
        if (const AOGDiagnosticHumanoid* Humanoid = Cast<AOGDiagnosticHumanoid>(Actor.Get()))
        {
            const FOGCombatUnitState& Snapshot = Humanoid->GetCombatState()->GetSnapshot();
            if (!Participants.ContainsByPredicate([&Snapshot](const FOGCombatUnitState& Existing) { return Existing.UnitEntityId == Snapshot.UnitEntityId; }))
                Participants.Add(Snapshot);
        }
    return Participants;
}

bool UOGDiagnosticCombatComponent::PublishEncounterResult(FString& Error)
{
    if (!bInTurn || Turn.GetState().Status != EOGTurnBattleStatus::Completed || ResolvedEncounterEventId.IsValid()) return true;
    int64 Tick = 0;
    if (!Store || !ReadTick || !ReadTick(Tick, Error)) return false;
    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId(); Event.EventType = TEXT("encounter.completed");
    Event.WorldTick = Tick; Event.PrimaryEntity = GetRulerId(); Event.bChronicleEligible = true;
    for (const FOGCombatUnitState& Unit : Turn.GetState().Units) Event.RelatedEntities.AddUnique(Unit.UnitEntityId);
    Event.PayloadJson = FString::Printf(TEXT("{\"battleId\":\"%s\",\"outcome\":\"%s\",\"authoredDiagnostic\":true}"),
        *Turn.GetState().BattleId.ToString(), Turn.GetOutcome() == EOGDiagnosticOutcome::Victory ? TEXT("victory") : TEXT("defeat"));
    if (!Store->AppendWorldEvent(Event, Error)) return false;
    ResolvedEncounterEventId = Event.EventId; bLastEncounterWasTurn = bInTurn;
    OnEncounterCompleted.Broadcast(ResolvedEncounterEventId);
    return true;
}

bool UOGDiagnosticCombatComponent::PublishEnemyDefeat(const FOGCombatUnitState& Enemy, FString& Error)
{
    if (Enemy.IsAlive() || !Enemy.UnitEntityId.IsValid()) { Error = TEXT("Encounter defeat event requires an actual defeated enemy."); return false; }
    if (PublishedEnemyDefeats.Contains(Enemy.UnitEntityId)) return true;
    PendingEnemyDefeatEvents.AddUnique(Enemy.UnitEntityId);
    int64 Tick = 0;
    if (!Store || !ReadTick || !ReadTick(Tick, Error)) return false;
    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId(); Event.EventType = TEXT("encounter.completed");
    Event.WorldTick = Tick; Event.PrimaryEntity = GetRulerId(); Event.bChronicleEligible = true;
    Event.RelatedEntities.Add(Enemy.UnitEntityId);
    Event.PayloadJson = TEXT("{\"outcome\":\"victory\",\"mode\":\"world\",\"authoredDiagnostic\":true}");
    if (!Store->AppendWorldEvent(Event, Error)) return false;
    PendingEnemyDefeatEvents.Remove(Enemy.UnitEntityId); PublishedEnemyDefeats.Add(Enemy.UnitEntityId);
    ResolvedEncounterEventId = Event.EventId; bLastEncounterWasTurn = bInTurn;
    OnEncounterCompleted.Broadcast(ResolvedEncounterEventId);
    return true;
}

bool UOGDiagnosticCombatComponent::StartScenario(IOGWorldStore& InStore,
    FTickReader TickReader, FVector InOrigin, FString& Error)
{
#if UE_BUILD_SHIPPING
    Error = TEXT("Training is unavailable in shipping."); return false;
#else
    if (bActive) { Error = TEXT("Training is already active."); return false; }
    AOGWorldPrototypeCharacter* Player = GetPlayer();
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    int64 Tick = 0;
    if (!Player || !Party || !TickReader || !TickReader(Tick, Error)) return false;
    if (!OGDiagnosticGacha::EnsureScopedFixture(InStore, Tick, Error)) return false;
    OriginalSlots = Party->GetSlots(); OriginalControl = Party->GetControlledSlot();
    OriginalTransform = Player->GetActorTransform(); SiteOrigin = InOrigin;
    Store = &InStore; ReadTick = MoveTemp(TickReader);
    if (!SetTurnLocalClock(false, Error)) { Store = nullptr; ReadTick = nullptr; return false; }
    Energy.Reset(); SkillReadyAt.Reset(); BasicReadyAt.Reset();
    BasicChain.Reset(); BasicChainEndsAt.Reset(); UltimateReadyAt.Reset();
    bool bLoaded = false;
    if (!LoadParty(bLoaded, Error)) { Store = nullptr; ReadTick = nullptr; return false; }
    if (!bLoaded)
    {
        TArray<FOGCombatUnitState> Units = {
            OGDiagnosticContent::MakeUnit(GetRulerId(), 0),
            OGDiagnosticContent::MakeUnit(OGDiagnosticGacha::ScopedId(6), 1),
            OGDiagnosticContent::MakeUnit(OGDiagnosticGacha::ScopedId(7), 2)};
        Units[1].Presence = Units[2].Presence = EOGCombatPresence::Reserve;
        if (!Party->ConfigureParty(Units, Error)) { Store = nullptr; return false; }
        for (const FOGCombatUnitState& Unit : Units) Energy.Add(Unit.UnitEntityId, 50);
    }
    bActive = true; ResolvedEncounterEventId = FOGEntityId();
    PendingEnemyDefeatEvents.Reset(); PublishedEnemyDefeats.Reset();
    for (uint32 Ordinal : {20u, 21u})
    {
        bool bFound = false; FName Kind; FString Json; int64 Created = 0;
        const FOGEntityId Id = OGDiagnosticGacha::ScopedId(Ordinal);
        if (!Store->TryReadEntity(Id, bFound, Kind, Json, Created, Error) ||
            (bFound && Kind != FName(TEXT("diagnostic_humanoid"))) ||
            (!bFound && !Store->UpsertEntity(Id, TEXT("diagnostic_humanoid"), Tick,
                Ordinal == 20 ? TEXT("{\"role\":\"world_enemy\"}") : TEXT("{\"role\":\"turn_sparring\"}"), Error)))
        { FString Ignored; LeaveScenario(Ignored); if (Error.IsEmpty()) Error = TEXT("Diagnostic enemy entity conflicts with existing content."); return false; }
    }
    if (!Qte.Configure(*Party, Error)) { LeaveScenario(Error); return false; }
    Player->SetActorLocation(SiteOrigin + FVector(0,0,98));
    RulerWorldPosition = Player->GetActorLocation();
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AOGDiagnosticHumanoid* Enemy = GetWorld()->SpawnActor<AOGDiagnosticHumanoid>(
        AOGDiagnosticHumanoid::StaticClass(), SiteOrigin + FVector(900,350,98), FRotator::ZeroRotator, Spawn);
    AOGDiagnosticHumanoid* Station = GetWorld()->SpawnActor<AOGDiagnosticHumanoid>(
        AOGDiagnosticHumanoid::StaticClass(), SiteOrigin + FVector(600,-350,98), FRotator::ZeroRotator, Spawn);
    RegisterScenarioActor(Enemy); RegisterScenarioActor(Station);
    if (!Enemy || !Station) { FString Ignored; LeaveScenario(Ignored); Error = TEXT("Training humanoid spawning failed."); return false; }
    Enemy->Initialize(*this, OGDiagnosticContent::MakeUnit(OGDiagnosticGacha::ScopedId(20), 3, 1), false);
    Station->Initialize(*this, OGDiagnosticContent::MakeUnit(OGDiagnosticGacha::ScopedId(21), 4, 1), true);
    UpdatePresentation();
    if (!PublishWorldPresence(Error) || !SaveParty(Error) || !RefreshCommandContext(Error))
    { FString Ignored; LeaveScenario(Ignored); return false; }
    Status = TEXT("Training: attack the humanoid; interact with blue turn sparring. Gacha earns access after 31 actual training ticks.");
    return true;
#endif
}

bool UOGDiagnosticCombatComponent::LeaveScenario(FString& Error)
{
    if (!bActive) return true;
    if (bInTurn && Turn.GetState().Status == EOGTurnBattleStatus::Running)
    { Error = TEXT("Resolve the turn encounter before leaving training."); return false; }
    if (bInTurn && !ReturnFromTurn(Error)) return false;
    if (!SaveParty(Error)) return false;
    Qte.Reset();
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (Party && !Party->RestorePresentationParty(OriginalSlots, OriginalControl, Error)) return false;
    if (AOGWorldPrototypeCharacter* Player = GetPlayer())
    {
        Player->RestoreDiagnosticHardLock(nullptr);
        Player->SetActorTransform(OriginalTransform);
        Player->SetControlledResourcePresentation(NAME_None, 0, 0, false);
        Player->ClearTargetHudProjection();
        if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
            if (AOGWorldPresentationHud* Hud = Cast<AOGWorldPresentationHud>(Controller->GetHUD()))
            { Hud->ClearActiveGachaPresentation(); Hud->SetFoundationRulerPresentationOwner(FOGEntityId()); }
    }
    for (TWeakObjectPtr<AActor> Actor : ScenarioActors) if (Actor.IsValid())
    {
        if (GetWorld()->GetGameInstance())
            if (UOGPresentationModeSubsystem* Presentation = GetWorld()->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
                Presentation->UnregisterWorldPresentationActor(Actor.Get());
        Actor->Destroy();
    }
    ScenarioActors.Reset(); OriginalSlots.Reset(); MarkedEnemy.Reset(); TurnEnemy.Reset(); ProtagonistWorldActor.Reset(); PresentedControlledEntity = FOGEntityId();
    bActive = false; Store = nullptr; ReadTick = nullptr;
    Error.Reset(); return true;
}

void UOGDiagnosticCombatComponent::ReleaseCanonicalStore()
{
    // Unfinished turn encounters retain their last World snapshot; release is
    // an interruption, not an invented victory, reward or post-combat event.
    if (bActive && Store && !bInTurn) { FString Error; if (!SaveParty(Error)) Status = Error; }
    if (bActive && Store && bInTurn) { FString Error; if (!SetTurnLocalClock(false, Error)) Status = Error; }
    Qte.Reset();
    if (AOGWorldPrototypeCharacter* Player = GetPlayer())
    {
        Player->RestoreDiagnosticHardLock(nullptr);
        Player->SetControlledResourcePresentation(NAME_None, 0, 0, false);
        Player->ClearTargetHudProjection();
        if (bInTurn && Player->GetCharacterMovement())
            Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(EntryMovementMode));
        if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
            if (AOGWorldPresentationHud* Hud = Cast<AOGWorldPresentationHud>(Controller->GetHUD()))
            { Hud->ClearActiveGachaPresentation(); Hud->SetFoundationRulerPresentationOwner(FOGEntityId()); }
    }
    bActive = false; bInTurn = false; Store = nullptr; ReadTick = nullptr;
    for (TWeakObjectPtr<AActor> Actor : ScenarioActors) if (Actor.IsValid())
    {
        if (GetWorld() && GetWorld()->GetGameInstance())
            if (UOGPresentationModeSubsystem* Presentation = GetWorld()->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
                Presentation->UnregisterWorldPresentationActor(Actor.Get());
        Actor->Destroy();
    }
    ScenarioActors.Reset(); OriginalSlots.Reset(); OriginalControl = INDEX_NONE;
    MarkedEnemy.Reset(); TurnEnemy.Reset(); TurnEntryLock.Reset(); ProtagonistWorldActor.Reset();
    PresentedControlledEntity = FOGEntityId(); ResolvedEncounterEventId = FOGEntityId();
    PendingEnemyDefeatEvents.Reset(); PublishedEnemyDefeats.Reset();
    Energy.Reset(); BasicChain.Reset(); BasicChainEndsAt.Reset(); SkillReadyAt.Reset();
    BasicReadyAt.Reset(); UltimateReadyAt.Reset(); Turn = FOGDiagnosticEncounter();
    bLastEncounterWasTurn = false;
}

void UOGDiagnosticCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseCanonicalStore();
    Super::EndPlay(Reason);
}

bool UOGDiagnosticCombatComponent::IsWorldDefeated() const
{ UOGWorldPartyRuntimeComponent* Party = GetParty(); return bActive && (!Party || Party->GetControlledSlot() == INDEX_NONE); }
bool UOGDiagnosticCombatComponent::IsSuspended() const
{
    const AOGWorldPrototypeCharacter* Player = GetPlayer();
    const UOGPresentationModeSubsystem* Presentation = GetWorld() && GetWorld()->GetGameInstance() ?
        GetWorld()->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>() : nullptr;
    return !bActive || bInTurn || IsWorldDefeated() || !Player ||
        (Presentation && Presentation->IsWorldPresentationSuspended()) ||
        Player->IsFoundationPauseMenuOpen() || Player->IsFoundationSettingsOpen() ||
        (Player->FindComponentByClass<UOGFoundationInteractionHost>() &&
         Player->FindComponentByClass<UOGFoundationInteractionHost>()->IsActive());
}

int32 UOGDiagnosticCombatComponent::GetControlledEnergy() const
{
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot())) return 0;
    const int32* Value = Energy.Find(Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId);
    return Value ? *Value : 0;
}
FString UOGDiagnosticCombatComponent::GetHpText() const
{
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot())) return TEXT("Defeated");
    const FOGCombatUnitState& Unit = Party->GetSlots()[Party->GetControlledSlot()].Unit;
    return FOGUiNumberFormatter::Format(Unit.CurrentHp, true, true) + TEXT(" / ") +
        FOGUiNumberFormatter::Format(Unit.Stats.MaxHp, true, true);
}

bool UOGDiagnosticCombatComponent::TryWorldAction(EOGDiagnosticCommand Command, AActor* Target, FString& Error)
{
    Error.Reset();
    if (!bActive) { Error = TEXT("Diagnostic combat is not active."); return false; }
    if (!RefreshCanonicalPartyProjection(Error)) { Status = Error; return false; }
    if (bInTurn) { const bool Accepted = SubmitTurn(Command, Error); if (!Accepted) Status = Error; return Accepted; }
    AOGDiagnosticHumanoid* Enemy = Cast<AOGDiagnosticHumanoid>(Target);
    IOGResolvedWorldDamageTarget* WorldTarget = Target ? Cast<IOGResolvedWorldDamageTarget>(Target) : nullptr;
    if (!Enemy && !WorldTarget) { Error = TEXT("Choose a target with a resolved combat damage contract."); Status = Error; return false; }
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (IsSuspended() || !Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    { Error = TEXT("Combat is unavailable in this state."); Status = Error; return false; }
    const FOGCombatUnitState Source = Party->GetSlots()[Party->GetControlledSlot()].Unit;
    FOGCombatUnitState Defender;
    if (Enemy) Defender = Enemy->GetCombatState()->GetSnapshot();
    else if (!WorldTarget->ReadResolvedWorldCombatSnapshot(Defender, Error)) { Status = Error; return false; }
    const bool TargetAccessible = Enemy ? Enemy->HasLineOfSight(GetOwner()) : WorldTarget->CanReceiveResolvedWorldDamage(GetOwner());
    if (!Defender.IsAlive() || (Enemy && Enemy->IsTurnStation()) || !TargetAccessible ||
        FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation()) > 360.0)
    { Error = TEXT("Target is outside living melee range/line of sight."); Status = Error; return false; }
    const double Now = GetWorld()->GetTimeSeconds();
    FVector2D& Cooldowns = SkillReadyAt.FindOrAdd(Source.UnitEntityId);
    const double Ready = Command == EOGDiagnosticCommand::Skill1 ? Cooldowns.X :
        Command == EOGDiagnosticCommand::Skill2 ? Cooldowns.Y :
        Command == EOGDiagnosticCommand::Ultimate ? UltimateReadyAt.FindRef(Source.UnitEntityId) : BasicReadyAt.FindRef(Source.UnitEntityId);
    if (Now < Ready) { Error = TEXT("Selected action is cooling down."); Status = Error; return false; }
    if (Command == EOGDiagnosticCommand::Ultimate && Energy.FindRef(Source.UnitEntityId) < 100)
    { Error = TEXT("Ultimate requires 100 energy. Basic attacks restore 25."); Status = Error; return false; }
    FOGDiagnosticActionDefinition Definition;
    if (!OGDiagnosticContent::ResolveAction(Source, Command, Definition, Error)) { Status = Error; return false; }
    if (Command == EOGDiagnosticCommand::Basic)
    {
        const int32 Chain = Now <= BasicChainEndsAt.FindRef(Source.UnitEntityId) ? BasicChain.FindRef(Source.UnitEntityId) : 0;
        Definition.AttackMultiplierBps = Chain == 2 ? 15000 : Chain == 1 ? 11500 : 10000;
        Definition.SkillId = FOGContentId(FString::Printf(TEXT("diagnostic:skill.basic_%d"), Chain + 1));
    }
    int32 Rank = 10000;
    if (!FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(Source, Defender,
        OGRankSuppressionChannels::Damage(), {}, Rank, Error)) { Status = Error; return false; }
    FOGDeterministicRng CandidateRng = Rng;
    const FOGDamageResolution Damage = OGDiagnosticContent::ResolveDamage(Definition, Source, Defender, Rank, CandidateRng,
        Enemy ? Enemy->GetCombatState()->GetExposure().DamageMultiplier(false, Now) : 10000);
    if (!(Enemy ? Enemy->GetCombatState()->ApplyResolvedDamage(Damage.TotalDamage, Error) :
        WorldTarget->ApplyResolvedWorldDamage(Damage.TotalDamage, GetOwner(), Error))) { Status = Error; return false; }
    Rng = CandidateRng;
    OnCombatPoseRequested.Broadcast(static_cast<uint8>(Command), GetBasicChainForHud());
    FOGCombatUnitState ResolvedDefender = Defender;
    if (Enemy) ResolvedDefender = Enemy->GetCombatState()->GetSnapshot();
    else if (!WorldTarget->ReadResolvedWorldCombatSnapshot(ResolvedDefender, Error)) Status = Error;
    const bool bAlive = ResolvedDefender.IsAlive();
    if (Enemy && Command == EOGDiagnosticCommand::Skill2 && Damage.Hit.ResolvedHitInstances > 0 && bAlive)
    { Enemy->GetCombatState()->GetExposure().Apply(Defender.UnitEntityId, Now); MarkedEnemy = Enemy; }
    if (Enemy && Damage.Hit.ResolvedHitInstances > 0) Qte.ConfirmSkillHit(Source.UnitEntityId, Defender.UnitEntityId, Definition.SkillId, bAlive, Now);
    if (Command == EOGDiagnosticCommand::Skill1) Cooldowns.X = Now + 3.0;
    if (Command == EOGDiagnosticCommand::Skill2) Cooldowns.Y = Now + 5.0;
    if (Command == EOGDiagnosticCommand::Ultimate) UltimateReadyAt.Add(Source.UnitEntityId, Now + 8.0);
    if (Command == EOGDiagnosticCommand::Basic)
    {
        BasicChain.FindOrAdd(Source.UnitEntityId) = ((Now <= BasicChainEndsAt.FindRef(Source.UnitEntityId) ? BasicChain.FindRef(Source.UnitEntityId) : 0) + 1) % 3;
        BasicChainEndsAt.Add(Source.UnitEntityId, Now + 1.2);
    }
    else { BasicChain.Add(Source.UnitEntityId, 0); BasicChainEndsAt.Add(Source.UnitEntityId, 0.0); }
    BasicReadyAt.Add(Source.UnitEntityId, Now + 0.30);
    int32& Resource = Energy.FindOrAdd(Source.UnitEntityId);
    Resource = Command == EOGDiagnosticCommand::Ultimate ? 0 : FMath::Min(100, Resource + (Command == EOGDiagnosticCommand::Basic ? 25 : 15));
    Status = FString::Printf(TEXT("%s dealt %s%s"), *Definition.SkillId.ToString(),
        *FOGUiNumberFormatter::Format(Damage.TotalDamage, true, true), bAlive ? TEXT("") : TEXT(" - defeated"));
    UpdatePresentation();
    if (!SaveParty(Error)) Status = Error;
    if (Enemy && !bAlive && !PublishEnemyDefeat(Enemy->GetCombatState()->GetSnapshot(), Error)) Status = Error;
    return true;
}

bool UOGDiagnosticCombatComponent::ReceiveStrike(const FOGCombatUnitState& Source, FString& Error)
{
    if (IsSuspended() || !RefreshCanonicalPartyProjection(Error)) return false;
    AOGWorldPrototypeCharacter* Player = GetPlayer();
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    const int32 Slot = Party->GetControlledSlot();
    if (!Party->GetSlots().IsValidIndex(Slot)) return false;
    const FOGCombatUnitState Target = Party->GetSlots()[Slot].Unit;
    if (Player->IsDodgeInvulnerable())
    {
        Player->NotifyPerfectDodge();
        Qte.ConfirmPerfectDodge(Target.UnitEntityId, GetWorld()->GetTimeSeconds());
        Status = TEXT("Perfect dodge: authored companion assist ready."); return true;
    }
    FOGDiagnosticActionDefinition Definition;
    if (!OGDiagnosticContent::ResolveAction(Source, EOGDiagnosticCommand::Basic, Definition, Error)) return false;
    int32 Rank = 10000;
    if (!FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(Source, Target,
        OGRankSuppressionChannels::Damage(), {}, Rank, Error)) return false;
    const FOGDamageResolution Damage = OGDiagnosticContent::ResolveDamage(Definition, Source, Target, Rank, Rng);
    if (!ApplyResolvedWorldDamage(Damage.TotalDamage, Error)) return false;
    Status = IsWorldDefeated() ? TEXT("Party defeated. Leave training returns to your original World party.") :
        FString::Printf(TEXT("Enemy dealt %s; HP %s"), *FOGUiNumberFormatter::Format(Damage.TotalDamage, true, true), *GetHpText());
    return true;
}

void UOGDiagnosticCombatComponent::OnPartyChanged()
{ if (bActive) { UpdatePresentation(); FString Error; if (!SaveParty(Error)) Status = Error; } }
void UOGDiagnosticCombatComponent::OnQteAccepted(int32 AssistSlot, AActor* Target)
{
    AOGDiagnosticHumanoid* Enemy = Cast<AOGDiagnosticHumanoid>(Target);
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!bActive || bInTurn || !Enemy || !Party->GetSlots().IsValidIndex(AssistSlot) || !Enemy->GetCombatState()->GetSnapshot().IsAlive()) return;
    const FOGCombatUnitState Source = Party->GetSlots()[AssistSlot].Unit;
    if (FVector::Dist(GetOwner()->GetActorLocation(), Enemy->GetActorLocation()) > 500 || !Enemy->HasLineOfSight(GetOwner())) return;
    FOGDiagnosticActionDefinition Definition; FString Error;
    if (!OGDiagnosticContent::ResolveAction(Source, EOGDiagnosticCommand::Skill1, Definition, Error)) return;
    int32 Rank = 10000;
    if (!FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(Source, Enemy->GetCombatState()->GetSnapshot(),
        OGRankSuppressionChannels::Damage(), {}, Rank, Error)) { Status = Error; return; }
    const FOGDamageResolution Damage = OGDiagnosticContent::ResolveDamage(Definition, Source,
        Enemy->GetCombatState()->GetSnapshot(), Rank, Rng,
        Enemy->GetCombatState()->GetExposure().DamageMultiplier(false, GetWorld()->GetTimeSeconds()));
    if (Enemy->GetCombatState()->ApplyResolvedDamage(Damage.TotalDamage, Error))
    { if (!Enemy->GetCombatState()->GetSnapshot().IsAlive()) PublishEnemyDefeat(Enemy->GetCombatState()->GetSnapshot(), Error); Status = TEXT("Companion QTE assist landed; control/resource state preserved."); Energy.FindOrAdd(Source.UnitEntityId) = FMath::Min(100, Energy.FindRef(Source.UnitEntityId) + 15); }
    OnPartyChanged();
}

bool UOGDiagnosticCombatComponent::BeginTurn(AOGDiagnosticHumanoid& Enemy, FString& Error)
{
    if (IsSuspended() || !RefreshCanonicalPartyProjection(Error)) { if (Error.IsEmpty()) Error = TEXT("Turn entry unavailable."); return false; }
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    TArray<FOGCombatUnitState> Players;
    for (const FOGWorldPartySlot& Slot : Party->GetSlots()) if (Slot.bAvailable) Players.Add(Slot.Unit);
    FOGCombatUnitState Opponent = OGDiagnosticContent::MakeUnit(Enemy.GetCombatState()->GetSnapshot().UnitEntityId, 4, 1);
    Opponent.Stats.MaxHp = Opponent.CurrentHp = FOGLargeNumber::FromInt64(250);
    FOGDiagnosticEncounter CandidateTurn = Turn;
    FOGDeterministicRng CandidateRng = Rng;
    if (!CandidateTurn.Start(Players, {Opponent}, CandidateRng.NextUInt64(), {}, Error, Energy)) return false;
    if (!SaveParty(Error) || !SetTurnLocalClock(true, Error)) return false;
    Turn = MoveTemp(CandidateTurn); Rng = CandidateRng;
    Qte.Reset(); TurnEnemy = &Enemy; bInTurn = true; ResolvedEncounterEventId = FOGEntityId();
    TurnEntryTransform = GetOwner()->GetActorTransform();
    TurnEntryLock = GetPlayer()->GetHardLockedTarget();
    GetPlayer()->RestoreDiagnosticHardLock(nullptr);
    EntryMovementMode = static_cast<uint8>(GetPlayer()->GetCharacterMovement()->MovementMode);
    GetPlayer()->GetCharacterMovement()->DisableMovement();
    NextEnemyTurnAt = GetWorld()->GetTimeSeconds() + 0.75;
    Status = TEXT("Turn sparring: canonical timeline chooses the next actor."); return true;
}

bool UOGDiagnosticCombatComponent::SubmitTurn(EOGDiagnosticCommand Command, FString& Error)
{
    if (!bInTurn || !TurnEnemy.IsValid()) { Error = TEXT("No turn encounter."); return false; }
    if (!Turn.SubmitPlayerAction(Command, TurnEnemy->GetCombatState()->GetSnapshot().UnitEntityId, Error)) return false;
    NextEnemyTurnAt = GetWorld()->GetTimeSeconds() + 0.75;
    if (!PublishEncounterResult(Error)) return false;
    Status = Turn.GetOutcome() == EOGDiagnosticOutcome::Running ? TEXT("Action resolved. Next actor selected by canonical timeline.") :
        Turn.GetOutcome() == EOGDiagnosticOutcome::Victory ? TEXT("Victory. Return to World.") : TEXT("Defeat. Return to World.");
    return true;
}

bool UOGDiagnosticCombatComponent::ReturnFromTurn(FString& Error)
{
    if (!bInTurn) { Error = TEXT("No turn encounter."); return false; }
    if (!PublishEncounterResult(Error)) return false;
    TArray<FOGCombatUnitState> Results;
    if (!Turn.CollectReturnSnapshots(Results, Error) || !SetTurnLocalClock(false, Error)) return false;
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    const int32 Controlled = Party->GetControlledSlot();
    for (int32 Pass = 0; Pass < 2; ++Pass)
        for (int32 Slot = 0; Slot < Party->GetSlots().Num(); ++Slot)
        {
            if ((Slot == Controlled) != (Pass == 1)) continue;
            const FOGEntityId Id = Party->GetSlots()[Slot].Unit.UnitEntityId;
            const FOGCombatUnitState* Result = Results.FindByPredicate([&Id](const FOGCombatUnitState& Unit) { return Unit.UnitEntityId == Id; });
            if (Result && !Party->ApplyResolvedHp(Slot, Id, Result->CurrentHp, Error)) return false;
        }
    Energy = Turn.GetResources(); bInTurn = false;
    GetPlayer()->SetActorTransform(TurnEntryTransform);
    GetPlayer()->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(EntryMovementMode));
    if (!Qte.Configure(*Party, Error)) return false;
    UpdatePresentation(); TurnEnemy.Reset();
    GetPlayer()->RestoreDiagnosticHardLock(TurnEntryLock.Get()); TurnEntryLock.Reset();
    Status = IsWorldDefeated() ? TEXT("Defeat returned to World; no free healing.") : TEXT("World restored with battle HP, kits and resources.");
    return SaveParty(Error);
}

bool UOGDiagnosticCombatComponent::PublishWorldPresence(FString& Error)
{
    if (!bActive || !Store || !ReadTick) return false;
    int64 Tick; if (!ReadTick(Tick, Error)) return false;
    FOGWorldPresenceRecord Presence;
    Presence.EntityId = GetRulerId(); Presence.LocationId = OGDiagnosticGacha::ScopedId(3);
    Presence.LocalPosition = FVector3d(GetProtagonistWorldPosition() - SiteOrigin);
    Presence.MovementContext = TEXT("diagnostic_world"); Presence.UpdatedWorldTick = Tick;
    if (!Store->UpsertWorldPresence(Presence, Error)) return false;
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (Party && Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    {
        const FOGEntityId Controlled = Party->GetSlots()[Party->GetControlledSlot()].Unit.UnitEntityId;
        if (Controlled != GetRulerId())
        {
            Presence.EntityId = Controlled; Presence.LocalPosition = FVector3d(GetOwner()->GetActorLocation() - SiteOrigin);
            if (!Store->UpsertWorldPresence(Presence, Error)) return false;
        }
    }
    for (TWeakObjectPtr<AActor> Actor : ScenarioActors)
        if (const AOGDiagnosticHumanoid* Humanoid = Cast<AOGDiagnosticHumanoid>(Actor.Get()))
        {
            Presence.EntityId = Humanoid->GetCombatState()->GetSnapshot().UnitEntityId;
            Presence.LocalPosition = FVector3d(Humanoid->GetActorLocation() - SiteOrigin);
            Presence.MovementContext = Humanoid->GetCombatState()->GetSnapshot().IsAlive() ? TEXT("diagnostic_world") : TEXT("defeated");
            if (!Store->UpsertWorldPresence(Presence, Error)) return false;
        }
    return true;
}

bool UOGDiagnosticCombatComponent::DeployManifestation(const FOGEntityId& Copy, int32 CompanionSlot, FString& Error)
{
    if (!bActive || bInTurn || !Store) { Error = TEXT("Deploy requires diagnostic World context."); return false; }
    // The physical training site is the sole authored extent of this Location.
    const FVector Offset = GetProtagonistWorldPosition() - SiteOrigin;
    if (FMath::Abs(Offset.X) > 3600 || FMath::Abs(Offset.Y) > 3600)
    { Error = TEXT("Return physically to controlled training Territory before first deploy."); return false; }
    if (!PublishWorldPresence(Error)) return false;
    bool bFound = false; FOGWorldPresenceRecord Presence;
    if (!Store->TryReadWorldPresence(GetRulerId(), bFound, Presence, Error) || !bFound || Presence.LocationId != OGDiagnosticGacha::ScopedId(3))
    { Error = TEXT("Physical Ruler presence does not match controlled training Territory."); return false; }
    TArray<FOGTerritoryClaimRecord> Claims;
    if (!Store->ListActiveClaimsForLocation(Presence.LocationId, Claims, Error)) return false;
    const bool bEffective = Claims.ContainsByPredicate([this](const FOGTerritoryClaimRecord& Claim)
    { return Claim.RulerId == GetRulerId() && Claim.TerritoryId == OGDiagnosticGacha::ScopedId(2) && (Claim.ControlState == FName(TEXT("controlled")) || Claim.ControlState == FName(TEXT("effective"))); });
    if (!bEffective) { Error = TEXT("Training Territory is not effectively controlled."); return false; }
    FOGCharacterManifestationRecord Manifestation;
    if (!Store->TryReadCharacterManifestation(Copy, bFound, Manifestation, Error) || !bFound || Manifestation.OwningRulerId != GetRulerId())
    { Error = TEXT("Selected Manifestation is not owned by the diagnostic Ruler."); return false; }
    const int32 Kit = Manifestation.IdentityId == FOGContentId(TEXT("diagnostic:identity.kit_1")) ? 1 :
        Manifestation.IdentityId == FOGContentId(TEXT("diagnostic:identity.kit_2")) ? 2 : INDEX_NONE;
    if (Kit == INDEX_NONE) { Error = TEXT("Selected copy has no installed diagnostic kit definition."); return false; }
    int64 Tick; if (!ReadTick(Tick, Error)) return false;
    FOGTerritoryControlService Territory(*Store); FOGEntityId Anchor;
    FOGCombatUnitState Unit = OGDiagnosticContent::MakeUnit(Copy, Kit);
    // Re-selecting an already deployed copy preserves its existing HP/state.
    for (const FOGWorldPartySlot& Slot : GetParty()->GetSlots()) if (Slot.Unit.UnitEntityId == Copy) Unit = Slot.Unit;
    TArray<FOGCombatUnitState> Candidate;
    for (const FOGWorldPartySlot& Slot : GetParty()->GetSlots()) Candidate.Add(Slot.Unit);
    if (CompanionSlot < 1 || CompanionSlot > 2 || !Candidate.IsValidIndex(CompanionSlot))
    { Error = TEXT("Choose companion slot 1 or 2."); return false; }
    Candidate[CompanionSlot] = Unit;
    if (!FOGActionCombatAdapter::ValidateSwitchParty(Candidate, Error)) return false;
    if (!Territory.AnchorManifestationForWorldMode(GetRulerId(), Copy, Tick, Anchor, Error)) return false;
    if (!GetParty()->AssignResolvedCompanion(CompanionSlot, Unit, Error)) return false;
    Energy.FindOrAdd(Copy) = FMath::Clamp(Energy.FindRef(Copy), 0, 100);
    if (!Qte.Configure(*GetParty(), Error)) return false;
    Status = TEXT("Selected anchored Manifestation assigned; other party slots retain state.");
    OnPartyChanged(); return SaveParty(Error);
}

bool UOGDiagnosticCombatComponent::RefreshCommandContext(FString& Error)
{
    if (!bActive || !Store || !ReadTick) return false;
    int64 Tick; if (!ReadTick(Tick, Error)) return false;
    if (!OGDiagnosticGacha::EnsureScopedFixture(*Store, Tick, Error) || !RefreshCanonicalPartyProjection(Error)) return false;
    if (APlayerController* Controller = Cast<APlayerController>(GetPlayer()->GetController()))
        if (AOGWorldPresentationHud* Hud = Cast<AOGWorldPresentationHud>(Controller->GetHUD()))
        {
            Hud->SetFoundationRulerPresentationOwner(GetRulerId());
            Hud->SetActiveGachaPresentation(OGDiagnosticGacha::MakeBanner(), Tick, static_cast<int64>(Rng.GetState() & MAX_int64));
        }
    return true;
}

void UOGDiagnosticCombatComponent::UpdatePresentation()
{
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (Party && Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    {
        const FOGCombatUnitState& Unit = Party->GetSlots()[Party->GetControlledSlot()].Unit;
        if (PresentedControlledEntity != Unit.UnitEntityId)
        {
            if (PresentedControlledEntity == GetRulerId()) RulerWorldPosition = GetOwner()->GetActorLocation();
            PresentedControlledEntity = Unit.UnitEntityId; OnControlledCharacterChanged.Broadcast(Unit.UnitEntityId, Unit.IdentityId); }
    }
    else if (PresentedControlledEntity.IsValid())
    { PresentedControlledEntity = FOGEntityId(); OnControlledCharacterChanged.Broadcast(FOGEntityId(), FOGContentId()); }
    GetPlayer()->SetControlledResourcePresentation(TEXT("Training energy"), GetControlledEnergy(), 100, bActive);
    FString Error; RefreshCommandContext(Error);
}

bool UOGDiagnosticCombatComponent::SaveParty(FString& Error)
{
    if (!bActive || !Store || !ReadTick || bInTurn) return true;
    int64 Tick; if (!ReadTick(Tick, Error)) return false;
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("owner"), GetRulerId().ToString()); Root->SetNumberField(TEXT("version"), 2);
    Root->SetNumberField(TEXT("controlled"), GetParty()->GetControlledSlot());
    TArray<TSharedPtr<FJsonValue>> JsonSlots;
    for (const FOGWorldPartySlot& Slot : GetParty()->GetSlots())
    {
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        if (!FJsonObjectConverter::UStructToJsonObject(FOGWorldPartySlot::StaticStruct(), &Slot, Row, 0, 0))
        { Error = TEXT("Party snapshot serialization failed."); return false; }
        Row->SetNumberField(TEXT("diagnosticEnergy"), Energy.FindRef(Slot.Unit.UnitEntityId));
        const FVector2D Ready = SkillReadyAt.FindRef(Slot.Unit.UnitEntityId);
        const double Now = GetWorld()->GetTimeSeconds();
        Row->SetNumberField(TEXT("skill1Remaining"), FMath::Max(0.0, Ready.X - Now));
        Row->SetNumberField(TEXT("skill2Remaining"), FMath::Max(0.0, Ready.Y - Now));
        Row->SetNumberField(TEXT("ultimateRemaining"), FMath::Max(0.0, UltimateReadyAt.FindRef(Slot.Unit.UnitEntityId) - Now));
        JsonSlots.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("slots"), JsonSlots);
    FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    bool bFound = false; FName Kind; FString Existing; int64 Created;
    if (!Store->TryReadEntity(OGDiagnosticGacha::ScopedId(8), bFound, Kind, Existing, Created, Error)) return false;
    if (bFound && Kind != FName(TEXT("diagnostic_party_snapshot")))
    { Error = TEXT("Party snapshot ID conflicts with existing content."); return false; }
    return Store->UpsertEntity(OGDiagnosticGacha::ScopedId(8), TEXT("diagnostic_party_snapshot"), bFound ? Created : Tick, Json, Error);
}

bool UOGDiagnosticCombatComponent::LoadParty(bool& bFound, FString& Error)
{
    FName Kind; FString Json; int64 Created;
    if (!Store->TryReadEntity(OGDiagnosticGacha::ScopedId(8), bFound, Kind, Json, Created, Error) || !bFound) return !bFound && Error.IsEmpty();
    TSharedPtr<FJsonObject> Root;
    if (Kind != FName(TEXT("diagnostic_party_snapshot")) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) ||
        !Root.IsValid() || !Root->HasTypedField<EJson::String>(TEXT("owner")) ||
        !Root->HasTypedField<EJson::Number>(TEXT("version")) || !Root->HasTypedField<EJson::Number>(TEXT("controlled")) ||
        Root->GetStringField(TEXT("owner")) != GetRulerId().ToString() || Root->GetIntegerField(TEXT("version")) != 2)
    { Error = TEXT("Canonical diagnostic party snapshot is invalid; refusing replacement."); return false; }
    TArray<FOGWorldPartySlot> Slots;
    if (!Root->HasTypedField<EJson::Array>(TEXT("slots"))) { Error = TEXT("Canonical party slots are missing."); return false; }
    for (const TSharedPtr<FJsonValue>& JsonSlot : Root->GetArrayField(TEXT("slots")))
    {
        FOGWorldPartySlot Slot;
        if (!JsonSlot.IsValid() || JsonSlot->Type != EJson::Object || !JsonSlot->AsObject().IsValid())
        { Error = TEXT("Canonical party slot is malformed."); return false; }
        if (!FJsonObjectConverter::JsonObjectToUStruct(JsonSlot->AsObject().ToSharedRef(), FOGWorldPartySlot::StaticStruct(), &Slot, 0, 0))
        { Error = TEXT("Canonical party snapshot could not reload."); return false; }
        // QTE/cancel expires on a cold launch; remaining cooldown/resource
        // values reload without a free reset or new party identity.
        if (Slots.IsEmpty())
        {
            if (Slot.Unit.UnitEntityId != GetRulerId() || Slot.Unit.IdentityId != FOGContentId(TEXT("diagnostic:identity.kit_0")))
            { Error = TEXT("Party protagonist does not match canonical Ruler entity."); return false; }
        }
        else
        {
            bool bCopyFound = false; FOGCharacterManifestationRecord Copy;
            if (!Store->TryReadCharacterManifestation(Slot.Unit.UnitEntityId, bCopyFound, Copy, Error) || !bCopyFound ||
                Copy.OwningRulerId != GetRulerId() || Copy.IdentityId != Slot.Unit.IdentityId || !Copy.WorldModeAnchorTerritoryId.IsValid())
            { Error = TEXT("Saved companion does not match owned anchored Manifestation."); return false; }
        }
        for (const TCHAR* Field : {TEXT("diagnosticEnergy"), TEXT("skill1Remaining"), TEXT("skill2Remaining"), TEXT("ultimateRemaining")})
            if (!JsonSlot->AsObject()->HasTypedField<EJson::Number>(Field))
            { Error = TEXT("Saved resource/cooldown state is malformed."); return false; }
        Slot.bQteReady = false;
        Energy.Add(Slot.Unit.UnitEntityId, FMath::Clamp(JsonSlot->AsObject()->GetIntegerField(TEXT("diagnosticEnergy")), 0, 100));
        const double Now = GetWorld()->GetTimeSeconds();
        SkillReadyAt.Add(Slot.Unit.UnitEntityId, FVector2D(Now + JsonSlot->AsObject()->GetNumberField(TEXT("skill1Remaining")),
            Now + JsonSlot->AsObject()->GetNumberField(TEXT("skill2Remaining"))));
        UltimateReadyAt.Add(Slot.Unit.UnitEntityId, Now + JsonSlot->AsObject()->GetNumberField(TEXT("ultimateRemaining")));
        Slots.Add(Slot);
    }
    return GetParty()->RestorePresentationParty(Slots, Root->GetIntegerField(TEXT("controlled")), Error);
}

void UOGDiagnosticCombatComponent::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Tick, Function);
    if (!bActive) return;
    if (GetPlayer()->IsFoundationPauseMenuOpen() || GetPlayer()->IsFoundationSettingsOpen() ||
        (GetPlayer()->FindComponentByClass<UOGFoundationInteractionHost>() &&
         GetPlayer()->FindComponentByClass<UOGFoundationInteractionHost>()->IsActive())) return;
    if (GetWorld()->GetGameInstance())
        if (const UOGPresentationModeSubsystem* Presentation = GetWorld()->GetGameInstance()->GetSubsystem<UOGPresentationModeSubsystem>())
            if (Presentation->IsWorldPresentationSuspended()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (bInTurn)
    {
        if (Turn.GetState().Status == EOGTurnBattleStatus::Running && !Turn.IsWaitingForPlayer() && Now >= NextEnemyTurnAt)
        {
            FString Error;
            if (!Turn.StepEnemy(Error)) Status = Error;
            else Status = Turn.GetOutcome() == EOGDiagnosticOutcome::Running ? TEXT("Enemy action resolved.") :
                Turn.GetOutcome() == EOGDiagnosticOutcome::Victory ? TEXT("Victory. Return to World.") : TEXT("Defeat. Return to World.");
            NextEnemyTurnAt = Now + 0.75;
        }
        FString EventError; if (!PublishEncounterResult(EventError)) Status = EventError;
        return;
    }
    const TArray<FOGEntityId> PendingEvents = PendingEnemyDefeatEvents;
    for (const FOGEntityId& Pending : PendingEvents)
        for (TWeakObjectPtr<AActor> Actor : ScenarioActors)
            if (const AOGDiagnosticHumanoid* Enemy = Cast<AOGDiagnosticHumanoid>(Actor.Get()))
                if (Enemy->GetCombatState()->GetSnapshot().UnitEntityId == Pending)
                { FString Error; if (!PublishEnemyDefeat(Enemy->GetCombatState()->GetSnapshot(), Error)) Status = Error; break; }
    Qte.Refresh(Now, MarkedEnemy.IsValid() && MarkedEnemy->GetCombatState()->GetSnapshot().IsAlive());
    if (Now >= NextPresenceSaveAt)
    {
        FString Error; if (!PublishWorldPresence(Error) || !RefreshCommandContext(Error)) Status = Error;
        NextPresenceSaveAt = Now + 1.0;
    }
    AOGDiagnosticHumanoid* Target = Cast<AOGDiagnosticHumanoid>(GetPlayer()->GetHardLockedTarget());
    if (Target && Target->GetCombatState()->GetSnapshot().IsAlive())
    {
        FOGKnowledgeFactRecord Hp; Hp.ValueJson = TEXT("{\"known\":true}");
        TArray<FOGHudStatusEffectInput> Effects;
        if (Target->GetCombatState()->GetExposure().IsActive(false, Now)) Effects.Add(Target->GetCombatState()->GetExposure().ToHud(false));
        FOGWorldTargetViewModel Projection; FString Error;
        FOGUiViewModelService::BuildWorldTarget(Target->GetCombatState()->GetSnapshot().UnitEntityId,
            Target->GetCombatState()->GetSnapshot().CurrentHp, &Hp, nullptr, nullptr, Effects, Projection, Error);
        GetPlayer()->SetTargetHudProjection(Projection);
    }
    else GetPlayer()->ClearTargetHudProjection();
}


bool UOGDiagnosticCombatComponent::ApplyResolvedWorldDamage(const FOGLargeNumber& Damage, FString& Error)
{
    Error.Reset();
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (IsSuspended() || !Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()) || Damage.GetSign() < 0)
    { Error = TEXT("Resolved World damage requires an active living canonical party target and nonnegative damage."); return false; }
    if (Damage.GetSign() == 0 || GetPlayer()->IsDodgeInvulnerable()) return true;
    const int32 Slot = Party->GetControlledSlot();
    const FOGCombatUnitState Target = Party->GetSlots()[Slot].Unit;
    FOGLargeNumber Hp = FOGLargeNumber::Add(Target.CurrentHp, FOGLargeNumber::ScaleByBasisPoints(Damage, -10000));
    if (Hp.GetSign() < 0) Hp = FOGLargeNumber();
    if (!Party->ApplyResolvedHp(Slot, Target.UnitEntityId, Hp, Error)) return false;
    UpdatePresentation();
    return SaveParty(Error);
}

bool UOGDiagnosticCombatComponent::RefreshCanonicalPartyProjection(FString& Error)
{
    if (!bActive || !Store || bInTurn) return true;
    UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (!Party) { Error = TEXT("Canonical party projection unavailable."); return false; }
    FOGFoundationCharacterRuntime Characters(*Store);
    for (int32 SlotIndex = 0; SlotIndex < Party->GetSlots().Num(); ++SlotIndex)
    {
        const FOGCombatUnitState Existing = Party->GetSlots()[SlotIndex].Unit;
        FOGFoundationCharacterContext Context;
        Context.EntityId = Existing.UnitEntityId; Context.OwnerEntityId = GetRulerId();
        if (Existing.UnitEntityId != GetRulerId()) Context.ManifestationId = Existing.UnitEntityId;
        FOGFoundationCharacterProjection Character;
        if (!Characters.Project(Context, Character, Error)) return false;
        FOGCombatUnitState Resolved = Existing;
        if (CharacterCombatResolver)
        { if (!CharacterCombatResolver(Character, Resolved, Error)) return false; }
        else
        {
            // This is the installed diagnostic content resolver, not a universal
            // progression multiplier or a second character state authority.
            int32 Kit = INDEX_NONE;
            for (int32 Index = 0; Index <= 4; ++Index)
                if (Existing.IdentityId == FOGContentId(FString::Printf(TEXT("diagnostic:identity.kit_%d"), Index))) Kit = Index;
            if (Kit == INDEX_NONE) { Error = TEXT("No installed character combat content resolver."); return false; }
            const FOGCombatUnitState Base = OGDiagnosticContent::MakeUnit(Existing.UnitEntityId, Kit, Existing.TeamIndex);
            Resolved.Stats = Base.Stats; Resolved.SkillSet = Base.SkillSet;
            Resolved.RankProjection = Character.bHasRank ? Character.EffectiveRank : FOGResolvedRankProjection();
            const FOGContentId PracticeSkill(TEXT("foundationdiag:skill.field_practice"));
            for (const FOGEntitySkillRecord& Skill : Character.Skills)
            {
                if (Skill.SkillId == PracticeSkill && Skill.CurrentState == FName(TEXT("integrated")))
                    Resolved.SkillSet.PassiveSkills.AddUnique(Skill.SkillId);
                for (int32 Index = Resolved.SkillSet.ActiveSkills.Num() - 1; Index >= 0; --Index)
                    if (Resolved.SkillSet.ActiveSkills[Index] == Skill.SkillId && Skill.CurrentState != FName(TEXT("integrated")))
                        Resolved.SkillSet.ActiveSkills.RemoveAt(Index);
                if (Resolved.SkillSet.UltimateSkill == Skill.SkillId && Skill.CurrentState != FName(TEXT("integrated")))
                    Resolved.SkillSet.UltimateSkill = FOGContentId();
            }
            FOGFoundationEquipmentFunctions Functions;
            if (!Characters.ProjectEquipmentFunctions(Context,
                FOGFoundationCharacterDiagnostics::ResolveEquipmentFunctions, Functions, Error)) return false;
            if (const FOGLargeNumber* Guard = Functions.StatContributions.Find(FName(TEXT("guard"))))
                Resolved.Stats.Defense = FOGLargeNumber::Add(Resolved.Stats.Defense, *Guard);
            // Current form is selected presentation/canonical state, not merely
            // an unlocked form; unlocking must never activate it implicitly.
        }
        if (!Party->ApplyResolvedCharacterProjection(SlotIndex, Resolved, Error)) return false;
    }
    return true;
}


bool UOGDiagnosticCombatComponent::SetTurnLocalClock(bool bTurnActive, FString& Error)
{
    UOGGameCoreSubsystem* Core = GetWorld() && GetWorld()->GetGameInstance() ?
        GetWorld()->GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>() : nullptr;
    if (!Store || !Core || Core->GetWorldStore() != Store || !Core->GetCanonicalClockRuntime())
    { Error = TEXT("Local turn time requires the shared canonical clock/store."); return false; }
    bool Found = false; FOGRealityNodeRecord Reality;
    if (!Store->TryReadRealityNode(OGDiagnosticGacha::TrainingRealityId(), Found, Reality, Error)) return false;
    if (!Found || Reality.TimeDomainId != OGDiagnosticGacha::ScopedId(4))
    { Error = TEXT("Training Reality has no installed canonical local time domain."); return false; }
    return Core->GetCanonicalClockRuntime()->SetLocalTurnBattle(Reality.TimeDomainId, bTurnActive, Error);
}

bool UOGDiagnosticCombatComponent::IsWorldActionReady(EOGDiagnosticCommand Command, FString& Reason) const
{
    Reason.Reset();
    const UOGWorldPartyRuntimeComponent* Party = GetParty();
    if (IsSuspended() || !Party || !Party->GetSlots().IsValidIndex(Party->GetControlledSlot()))
    { Reason = TEXT("Action unavailable in the current gameplay state."); return false; }
    const FOGWorldPartySlot& Slot = Party->GetSlots()[Party->GetControlledSlot()];
    if (!Slot.bAvailable || Slot.bDefeated || !Slot.Unit.IsAlive())
    { Reason = TEXT("Controlled character is unavailable."); return false; }
    FOGDiagnosticActionDefinition Definition;
    if (!OGDiagnosticContent::ResolveAction(Slot.Unit, Command, Definition, Reason)) return false;
    const FVector2D Cooldown = SkillReadyAt.FindRef(Slot.Unit.UnitEntityId);
    const double Ready = Command == EOGDiagnosticCommand::Skill1 ? Cooldown.X :
        Command == EOGDiagnosticCommand::Skill2 ? Cooldown.Y :
        Command == EOGDiagnosticCommand::Ultimate ? UltimateReadyAt.FindRef(Slot.Unit.UnitEntityId) :
        BasicReadyAt.FindRef(Slot.Unit.UnitEntityId);
    if (!GetWorld() || GetWorld()->GetTimeSeconds() < Ready)
    { Reason = TEXT("Action is cooling down."); return false; }
    if (Command == EOGDiagnosticCommand::Ultimate && Energy.FindRef(Slot.Unit.UnitEntityId) < 100)
    { Reason = TEXT("Ultimate requires 100 training energy."); return false; }
    return true;
}
