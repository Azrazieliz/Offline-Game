#pragma once
#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "Runtime/OGFoundationCharacterRuntime.h"
#include "Runtime/OGOptionalAssetRuntime.h"
#include "World/OGWorldStateRecords.h"
#include "World/OGItemKnowledgeCharacterRecords.h"

class AActor;
class APlayerController;
class UWorld;
class IOGWorldStore;

/** Context labels are extensible; they are provenance, not access gates. */
struct OFFLINEGAME_API FOGInteractionParticipant
{
    FOGFoundationCharacterContext Character;
    FName Role = NAME_None;
    FOGContentId RigFamilyId;
    TWeakObjectPtr<AActor> Actor;
};
struct OFFLINEGAME_API FOGInteractionConstraint
{
    FName ParticipantRole;
    FName TargetRole;
    FName Effector;
    FVector TargetOffset = FVector::ZeroVector;
};
struct OFFLINEGAME_API FOGInteractionAnimationBinding
{
    FName Role;
    FOGContentId RigFamilyId;
    // Empty means shared; a nonempty ID is an exact current Manifestation override.
    FOGEntityId ManifestationId;
    FOGContentId VersionId;
    FOGOptionalAssetReference Animation;
    FOGOptionalAssetReference Expression;
    FOGOptionalAssetReference Reaction;
};
struct OFFLINEGAME_API FOGInteractionAuthoredConsequences
{
    TArray<FOGKnowledgeFactRecord> Knowledge;
    TArray<FOGSemanticMemoryRecord> Memories;
    TArray<FOGManifestationPresentationStateRecord> Presentation;
    TArray<FOGWorldPresenceRecord> Presence;
};
struct OFFLINEGAME_API FOGInteractionActionNode
{
    FName NodeId;
    FOGContentId ActionId;
    double DurationSeconds = 1.0;
    TArray<FName> RequiredRoles;
    TArray<FName> NextNodes;
    TArray<FOGInteractionAnimationBinding> AnimationBindings;
    TArray<FOGInteractionConstraint> Constraints;
    FOGInteractionAuthoredConsequences Consequences;
    FName EnvironmentAnchorTag = NAME_None;
};
struct OFFLINEGAME_API FOGInteractionActionGraph
{
    FOGContentId GraphId;
    FName StartNode;
    TArray<FOGInteractionActionNode> Nodes;
};
struct OFFLINEGAME_API FOGInteractionEntry
{
    FName Context = FName(TEXT("world")); // roster/ruler/world/event/npc/post_combat/extension
    FOGEntityId ContextEventId;
    FOGEntityId LocationId;
    TArray<FOGInteractionParticipant> Participants;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APlayerController> ReturnController;
    FVector PreferredLocation = FVector::ZeroVector;
    bool bPrivacyPresentation = false;
};
struct OFFLINEGAME_API FOGInteractionParticipantState
{
    FOGInteractionParticipant Binding;
    FOGFoundationCharacterProjection Character;
    FTransform StagingTransform;
    bool bActionRelevant = false;
    bool bCameraRelevant = false;
    bool bAnimationCompatible = false;
    bool bHasOptionalPresentation = false;
    FString PresentationReason;
};

/** Presentation consumers may implement actual rig-specific IK/expression hooks.
 * Ordinary character significance is fed through this same bridge, without
 * removing logical participants or changing their current Manifestations. */
class OFFLINEGAME_API IOGSystemicInteractionPresentation
{
public:
    virtual ~IOGSystemicInteractionPresentation() = default;
    virtual void ApplyCharacter(const FOGInteractionParticipantState&) = 0;
    virtual void SetSignificance(const FOGEntityId&, bool bActionRelevant, bool bCameraRelevant) = 0;
    virtual void ApplyConstraint(const FOGInteractionConstraint&, const TArray<FOGInteractionParticipantState>&) = 0;
    virtual void ApplyAnimation(const FOGEntityId&, UObject* Animation) = 0;
    virtual void ApplyReaction(const FOGEntityId&, UObject* Expression, UObject* Reaction) = 0;
    virtual void SetPrivacy(bool bPrivacy) = 0;
    virtual void RestoreGameplay() = 0;
};

/** Content-neutral native runtime. All consequences use the existing world Store.
 * Host advances the canonical clock normally, even while Pacing is zero.
 * UWorld and actors are optional for logical Ruler/event execution. */
class OFFLINEGAME_API FOGSystemicInteractionRuntime
{
public:
    FOGSystemicInteractionRuntime(IOGWorldStore& Store, const FOGEntityId& CanonicalRulerId,
        FOGOptionalPackageHost* OptionalPackageHost = nullptr);
    ~FOGSystemicInteractionRuntime();
    bool SetOptionalPackageHost(FOGOptionalPackageHost* Host, FString& Error)
    {
        if (bActive) { Error = TEXT("Exit interaction before replacing its optional package host."); return false; }
        return OptionalAssets.SetContainerHost(Host, Error);
    }
    bool Enter(const FOGInteractionEntry&, const FOGInteractionActionGraph&, int64 WorldTick, FString&);
    bool SelectAction(FName NodeId, FString&);
    bool SetSequence(const TArray<FName>& NodeIds, FString&);
    bool SetPacing(double Rate, FString&);
    bool Advance(double DeltaSeconds, int64 WorldTick, FString&);
    bool Restage(const FVector& PreferredLocation, FString&);
    bool SetCamera(const FTransform& CameraTransform, FString&);
    void SetPrivacyPresentation(bool bPrivacy);
    void SetPresentationBridge(IOGSystemicInteractionPresentation* Bridge) { Presentation = Bridge; }
    bool Exit(FName Reason, int64 WorldTick, FString&);
    bool IsActive() const { return bActive; }
    bool IsAwaitingAction() const { return bActive && CurrentNode.IsNone(); }
    FName GetCurrentNode() const { return CurrentNode; }
    double GetPacing() const { return Pacing; }
    const TArray<FOGInteractionParticipantState>& GetParticipants() const { return Participants; }
    static FOGInteractionActionGraph MakeNeutralDiagnosticGraph();
private:
    struct FActorRestore
    {
        TWeakObjectPtr<AActor> Actor;
        FTransform Transform;
        FVector Velocity = FVector::ZeroVector;
        uint8 MovementMode = 0;
        uint8 CustomMovementMode = 0;
        bool bHidden = false;
        bool bHasPresence = false;
        FOGWorldPresenceRecord Presence;
    };
    const FOGInteractionActionNode* FindNode(FName Id) const;
    bool RefreshCharacters(FString&);
    bool SolveStaging(const FVector&, FName AnchorTag, FString&);
    bool StartNode(FName, FString&);
    bool CommitEvent(FName Type, FName Reason, int64 Tick,
        const FOGInteractionAuthoredConsequences* Consequences, FString&);
    void RestoreGameplay();
    void UpdatePresentation(const FOGInteractionActionNode&);
    bool LoadOptional(const FOGOptionalAssetReference&, UObject*&, FString&);
    void UpdateSignificance(const FOGInteractionActionNode&);
    IOGWorldStore& Store;
    FOGEntityId RulerId;
    FOGEntityId SessionId;
    FOGFoundationCharacterRuntime Characters;
    FOGOptionalAssetRuntime OptionalAssets;
    TMap<FString, TWeakObjectPtr<UObject>> SessionAssets;
    FOGInteractionEntry Entry;
    FOGInteractionActionGraph Graph;
    TArray<FOGInteractionParticipantState> Participants;
    TArray<FActorRestore> Restore;
    TArray<FName> Sequence;
    FName CurrentNode;
    FName LastCompletedNode;
    double Elapsed = 0;
    double Pacing = 1;
    bool bActive = false;
    int64 LastWorldTick = -1;
    TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UObject>> PlayedMontages;
    bool bInputSuppressed = false;
    bool bMoveSuppressed = false;
    bool bLookSuppressed = false;
    TWeakObjectPtr<AActor> ReturnViewTarget;
    TWeakObjectPtr<AActor> CameraActor;
    FRotator ReturnControlRotation;
    IOGSystemicInteractionPresentation* Presentation = nullptr;
};