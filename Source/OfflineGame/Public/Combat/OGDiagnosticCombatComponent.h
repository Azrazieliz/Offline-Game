#pragma once

#include "Components/ActorComponent.h"
#include "OGDiagnosticEncounter.h"
#include "OGDiagnosticQteConditions.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGFoundationCharacterRuntime.h"
#include "OGDiagnosticCombatComponent.generated.h"

class AOGDiagnosticHumanoid;
class AOGWorldPrototypeCharacter;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOGDiagnosticCharacterChanged, const FOGEntityId&, const FOGContentId&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOGDiagnosticCombatPoseRequested, uint8, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOGDiagnosticEncounterCompleted, const FOGEntityId&);

UCLASS(ClassGroup=(OfflineGame))
class OFFLINEGAME_API UOGDiagnosticCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOGDiagnosticCombatComponent();
    FOGDiagnosticCharacterChanged OnControlledCharacterChanged;
    FOGDiagnosticCombatPoseRequested OnCombatPoseRequested;
    FOGDiagnosticEncounterCompleted OnEncounterCompleted;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    using FTickReader = TFunction<bool(int64&, FString&)>;
    bool StartScenario(IOGWorldStore& Store, FTickReader TickReader, FVector SiteOrigin, FString& Error);
    bool LeaveScenario(FString& Error);
    /** Drops all borrowed store bindings before restore/clear/close. */
    void ReleaseCanonicalStore();
    bool ApplyResolvedWorldDamage(const FOGLargeNumber& Damage, FString& Error);
    bool RefreshCanonicalPartyProjection(FString& Error);
    using FCharacterCombatResolver = TFunction<bool(const FOGFoundationCharacterProjection&, FOGCombatUnitState&, FString&)>;
    void SetCharacterCombatResolver(FCharacterCombatResolver Resolver) { CharacterCombatResolver = MoveTemp(Resolver); }
    bool IsActive() const { return bActive; }
    bool IsInTurn() const { return bInTurn; }
    bool IsWorldDefeated() const;
    bool IsSuspended() const;
    bool TryWorldAction(EOGDiagnosticCommand Command, AActor* Target, FString& Error);
    bool IsWorldActionReady(EOGDiagnosticCommand Command, FString& DisabledReason) const;
    bool ReceiveStrike(const FOGCombatUnitState& Source, FString& Error);
    void OnPartyChanged();
    void OnQteAccepted(int32 AssistSlot, AActor* Target);
    bool BeginTurn(AOGDiagnosticHumanoid& Enemy, FString& Error);
    bool SubmitTurn(EOGDiagnosticCommand Command, FString& Error);
    bool ReturnFromTurn(FString& Error);
    bool DeployManifestation(const FOGEntityId& Copy, int32 CompanionSlot, FString& Error);
    bool PublishWorldPresence(FString& Error);
    bool SaveParty(FString& Error);
    bool RefreshCommandContext(FString& Error);
    FOGEntityId GetRulerId() const;
    void SetProtagonistWorldActor(AActor* Actor) { ProtagonistWorldActor = Actor; }
    AActor* GetProtagonistWorldActor() const;
    FVector GetProtagonistWorldPosition() const;
    FOGEntityId GetResolvedEncounterEventId() const { return ResolvedEncounterEventId; }
    TArray<FOGCombatUnitState> GetWorldParticipantsForInteraction() const;
    const TArray<TWeakObjectPtr<AActor>>& GetScenarioActors() const { return ScenarioActors; }
    bool TryGetControlledContext(FOGEntityId& Entity, FOGContentId& Identity,
        bool& bHasManifestation, FOGCharacterManifestationRecord& Manifestation, FString& Error) const;
    bool CanAcceptQte(int32 AssistSlot, AActor* Target, FString& Error) const;
    int32 GetBasicChainForHud() const;
    const FOGTurnBattleState& GetTurnState() const { return Turn.GetState(); }
    EOGDiagnosticOutcome GetTurnOutcome() const { return Turn.GetOutcome(); }
    FString GetStatus() const { return Status; }
    FString GetHpText() const;
    int32 GetControlledEnergy() const;
    bool IsWaitingForTurnPlayer() const { return bInTurn && Turn.IsWaitingForPlayer(); }
    void SetStatus(const FString& Message) { Status = Message; }
    void RegisterScenarioActor(AActor* Actor);
private:
    bool SetTurnLocalClock(bool bTurnActive, FString& Error);
    bool LoadParty(bool& bFound, FString& Error);
    bool PublishEncounterResult(FString& Error);
    bool PublishEnemyDefeat(const FOGCombatUnitState& Enemy, FString& Error);
    void UpdatePresentation();
    UOGWorldPartyRuntimeComponent* GetParty() const;
    AOGWorldPrototypeCharacter* GetPlayer() const;
    FCharacterCombatResolver CharacterCombatResolver;
    IOGWorldStore* Store = nullptr;
    FTickReader ReadTick;
    FOGDeterministicRng Rng{0x4f474449};
    FOGDiagnosticEncounter Turn;
    FOGDiagnosticQteConditions Qte;
    TMap<FOGEntityId, int32> Energy;
    TMap<FOGEntityId, int32> BasicChain;
    TMap<FOGEntityId, double> BasicChainEndsAt;
    TMap<FOGEntityId, double> UltimateReadyAt;
    TMap<FOGEntityId, FVector2D> SkillReadyAt;
    TMap<FOGEntityId, double> BasicReadyAt;
    TArray<FOGWorldPartySlot> OriginalSlots;
    int32 OriginalControl = INDEX_NONE;
    FTransform OriginalTransform;
    FVector SiteOrigin;
    FVector RulerWorldPosition;
    TWeakObjectPtr<AActor> ProtagonistWorldActor;
    TArray<TWeakObjectPtr<AActor>> ScenarioActors;
    TWeakObjectPtr<AOGDiagnosticHumanoid> MarkedEnemy;
    TWeakObjectPtr<AOGDiagnosticHumanoid> TurnEnemy;
    FTransform TurnEntryTransform;
    TWeakObjectPtr<AActor> TurnEntryLock;
    uint8 EntryMovementMode = 0;
    double NextEnemyTurnAt = 0.0;
    double NextPresenceSaveAt = 0.0;
    bool bActive = false;
    bool bInTurn = false;
    FString Status;
    FOGEntityId PresentedControlledEntity;
    FOGEntityId ResolvedEncounterEventId;
    bool bLastEncounterWasTurn = false;
    TArray<FOGEntityId> PendingEnemyDefeatEvents;
    TSet<FOGEntityId> PublishedEnemyDefeats;
};
