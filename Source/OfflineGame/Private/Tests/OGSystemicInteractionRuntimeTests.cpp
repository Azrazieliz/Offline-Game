#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/OGSystemicInteractionRuntime.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "Runtime/OGCanonicalClockRuntime.h"
#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include <limits>

namespace
{
struct FInteractionFixture
{
    FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FString DatabasePath = FPaths::Combine(Directory, TEXT("interaction.db"));
    FOGSQLiteWorldStore Store;
    FOGEntityId Ruler = FOGEntityId::NewId();
    FOGEntityId Location = FOGEntityId::NewId();
    FOGEntityId ContextEvent = FOGEntityId::NewId();
    FOGEntityId Memory = FOGEntityId::NewId();
    FOGInteractionEntry Entry;
    FString Error;
    bool Open(int32 Count = 13)
    {
        if (!Store.Open(DatabasePath, Error)) return false;
        FOGLocationRecord Place; Place.LocationId = Location; Place.Kind = TEXT("test_context");
        if (!Store.UpsertLocation(Place, 0, Error)) return false;
        FOGWorldEvent Context; Context.EventId = ContextEvent;
        Context.EventType = TEXT("test.authored_context"); Context.PrimaryEntity = Ruler;
        Context.PayloadJson = TEXT("{\"authored\":true}");
        Entry.Context = TEXT("authored_extension"); Entry.ContextEventId = ContextEvent;
        Entry.LocationId = Location;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FOGInteractionParticipant Participant;
            Participant.Character.EntityId = Index == 0 ? Ruler : FOGEntityId::NewId();
            Participant.Character.OwnerEntityId = Participant.Character.EntityId;
            Participant.Role = Index == 0 ? FName(TEXT("player")) : FName(TEXT("participant"));
            Participant.RigFamilyId = FOGContentId(TEXT("test:rig.neutral"));
            if (!Store.UpsertEntity(Participant.Character.EntityId,
                Index == 0 ? FName(TEXT("ruler")) : FName(TEXT("npc")), 0,
                TEXT("{\"history\":\"retained\",\"progression\":17}"), Error)) return false;
            FOGWorldPresenceRecord Presence; Presence.EntityId = Participant.Character.EntityId;
            Presence.LocationId = Location; Presence.MovementContext = TEXT("ground");
            Presence.LocalPosition = FVector(Index * 20.0, 10, 0);
            if (!Store.UpsertWorldPresence(Presence, Error)) return false;
            Entry.Participants.Add(Participant);
        }
        FOGEntityClassRecord Class; Class.OwnerEntityId = Ruler;
        Class.ClassId = FOGContentId(TEXT("test:class.neutral"));
        Class.RecognizedWorldTick = 2; Class.UpdatedWorldTick = 3;
        Class.StateJson = TEXT("{\"mastery\":\"retained\"}");
        if (!Store.UpsertEntityClass(Class, Error)) return false;
        return Store.AppendWorldEvent(Context, Error) &&
            Store.SetResourceBalance(Ruler, FOGContentId(TEXT("test:resource.unrelated")), 29, Error);
    }
    FOGInteractionActionGraph GraphWithConsequences() const
    {
        auto Graph = FOGSystemicInteractionRuntime::MakeNeutralDiagnosticGraph();
        FOGKnowledgeFactRecord Fact; Fact.OwnerEntityId = Ruler;
        Fact.SubjectEntityId = Entry.Participants.Last().Character.EntityId;
        Fact.FactKey = TEXT("neutral_handoff_observed"); Fact.SourceEntityId = Fact.SubjectEntityId;
        Fact.ValueJson = TEXT("{\"observed\":\"neutral_handoff\"}");
        Fact.LearnedWorldTick = 5;
        Graph.Nodes[0].Consequences.Knowledge.Add(Fact);
        FOGSemanticMemoryRecord Record; Record.MemoryId = Memory; Record.OwnerEntityId = Ruler;
        Record.SubjectEntityId = Fact.SubjectEntityId;
        Record.MemoryTypeId = FOGContentId(TEXT("test:memory.neutral_handoff"));
        Record.SalienceBps = 5000; Record.StateJson = TEXT("{\"authored\":\"handoff\"}");
        Graph.Nodes[0].Consequences.Memories.Add(Record);
        FOGWorldPresenceRecord Presence; Presence.EntityId = Ruler;
        Presence.LocationId = Location; Presence.LocalPosition = FVector(50, 60, 0);
        Presence.MovementContext = TEXT("ground");
        Graph.Nodes[0].Consequences.Presence.Add(Presence);
        return Graph;
    }
    bool Events(TArray<FOGWorldEvent>& Out)
    {
        if (!Store.ListWorldEvents(Ruler, NAME_None, false, 1000, Out, Error)) return false;
        Out.RemoveAll([](const FOGWorldEvent& Event)
            { return !Event.EventType.ToString().StartsWith(TEXT("interaction.")); });
        return true;
    }
    ~FInteractionFixture()
    {
        Store.Close(); IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }
};

struct FInteractionPresentationProbe : IOGSystemicInteractionPresentation
{
    TSet<FOGEntityId> Projected;
    TSet<FOGEntityId> Significant;
    int32 CharacterCalls = 0;
    int32 RestoreCalls = 0;
    int32 ConstraintCalls = 0;
    bool bPrivacy = false;
    virtual void ApplyCharacter(const FOGInteractionParticipantState& State) override
    { ++CharacterCalls; Projected.Add(State.Binding.Character.EntityId); }
    virtual void SetSignificance(const FOGEntityId& Entity, bool, bool) override
    { Significant.Add(Entity); }
    virtual void ApplyConstraint(const FOGInteractionConstraint&,
        const TArray<FOGInteractionParticipantState>&) override { ++ConstraintCalls; }
    virtual void ApplyAnimation(const FOGEntityId&, UObject*) override {}
    virtual void ApplyReaction(const FOGEntityId&, UObject*, UObject*) override {}
    virtual void SetPrivacy(bool bValue) override { bPrivacy = bValue; }
    virtual void RestoreGameplay() override { ++RestoreCalls; }
};

TSharedPtr<FJsonObject> Payload(const FOGWorldEvent& Event)
{
    TSharedPtr<FJsonObject> Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Event.PayloadJson), Object);
    return Object;
}
FString WithoutGeneratedSessionId(const FOGWorldEvent& Event)
{
    const auto Object = Payload(Event);
    if (!Object.IsValid()) return TEXT("invalid JSON");
    // Only this runtime-generated identifier is excluded. Every authored field,
    // participant binding, context, reason and payload field remains compared.
    Object->RemoveField(TEXT("session_id"));
    FString Json; FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSystemicInteractionBindingsTest,
    "OfflineGame.Foundation.Interaction.CanonicalRulerVariableArityAndGraphReferences",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGSystemicInteractionBindingsTest::RunTest(const FString& Parameters)
{
    FInteractionFixture F;
    if (!TestTrue(TEXT("Open shared canonical fixture"), F.Open())) return false;
    auto Graph = FOGSystemicInteractionRuntime::MakeNeutralDiagnosticGraph();
    FInteractionPresentationProbe Presentation;
    FOGSystemicInteractionRuntime Runtime(F.Store, F.Ruler);
    Runtime.SetPresentationBridge(&Presentation);
    auto RejectEntry = [&](const TCHAR* Label, const FOGInteractionEntry& Entry,
        const FOGInteractionActionGraph& Candidate)
    {
        TestFalse(Label, Runtime.Enter(Entry, Candidate, 5, F.Error));
        TestFalse(TEXT("Rejected entry stays inactive"), Runtime.IsActive());
        TestEqual(TEXT("Rejected entry leaves no logical participants"), Runtime.GetParticipants().Num(), 0);
        TestEqual(TEXT("Rejected entry cannot take character presentation ownership"), Presentation.CharacterCalls, 0);
    };
    auto Bad = F.Entry; Bad.Participants[0].Role = TEXT("participant");
    RejectEntry(TEXT("Ruler cannot be a nonplayer participant"), Bad, Graph);
    Bad = F.Entry; Bad.Participants[1].Role = TEXT("player");
    RejectEntry(TEXT("Non-Ruler cannot take the physical player role"), Bad, Graph);
    Bad = F.Entry; Bad.Participants.RemoveAt(0);
    RejectEntry(TEXT("Missing canonical player rejected"), Bad, Graph);
    Bad = F.Entry; Bad.Participants.Add(F.Entry.Participants[0]);
    RejectEntry(TEXT("Duplicate canonical player rejected"), Bad, Graph);
    Bad = F.Entry; Bad.Participants[2].Character.EntityId = Bad.Participants[1].Character.EntityId;
    RejectEntry(TEXT("Duplicate persistent participant rejected"), Bad, Graph);
    auto Broken = Graph; Broken.StartNode = TEXT("absent");
    RejectEntry(TEXT("Missing graph start rejected"), F.Entry, Broken);
    Broken = Graph; Broken.Nodes[0].NextNodes.Add(TEXT("absent"));
    RejectEntry(TEXT("Missing graph transition rejected"), F.Entry, Broken);
    Broken = Graph; Broken.Nodes[0].RequiredRoles.Add(TEXT("absent_role"));
    RejectEntry(TEXT("Unbound action role rejected"), F.Entry, Broken);
    Broken = Graph;
    const FOGInteractionActionNode DuplicateNode = Broken.Nodes[0];
    Broken.Nodes.Add(DuplicateNode);
    RejectEntry(TEXT("Duplicate action node rejected"), F.Entry, Broken);
    FOGInteractionConstraint Constraint;
    Constraint.ParticipantRole = TEXT("player"); Constraint.TargetRole = TEXT("absent_role");
    Broken = Graph; Broken.Nodes[0].Constraints.Add(Constraint);
    RejectEntry(TEXT("Unbound constraint target rejected"), F.Entry, Broken);
    Constraint.TargetRole = TEXT("participant");
    Constraint.TargetOffset.X = std::numeric_limits<double>::quiet_NaN();
    Broken = Graph; Broken.Nodes[0].Constraints.Add(Constraint);
    RejectEntry(TEXT("Nonfinite constraint placement rejected"), F.Entry, Broken);
    FOGInteractionAnimationBinding Animation;
    Animation.Role = TEXT("absent_role"); Animation.RigFamilyId = FOGContentId(TEXT("test:rig.neutral"));
    Broken = Graph; Broken.Nodes[0].AnimationBindings.Add(Animation);
    RejectEntry(TEXT("Unbound animation role rejected"), F.Entry, Broken);
    TArray<FOGWorldEvent> Events;
    TestTrue(TEXT("Read events after rejections"), F.Events(Events));
    TestEqual(TEXT("Invalid bindings and graphs create no interaction event"), Events.Num(), 0);

    Constraint.TargetOffset = FVector(10, 0, 0);
    Graph.Nodes[0].Constraints.Add(Constraint);
    Animation.Role = TEXT("participant");
    // Empty optional assets are intentional; logical action validity is independent.
    Graph.Nodes[0].AnimationBindings.Add(Animation);
    if (!TestTrue(TEXT("Thirteen persistent participants bind without an architectural cap"),
        Runtime.Enter(F.Entry, Graph, 5, F.Error))) return false;
    TestEqual(TEXT("Every logical participant retained"), Runtime.GetParticipants().Num(), 13);
    TestEqual(TEXT("All current character projections reach the ordinary bridge"), Presentation.Projected.Num(), 13);
    TestEqual(TEXT("Significance does not drop logical participants"), Presentation.Significant.Num(), 13);
    TestTrue(TEXT("Bound constraints reach the presentation extension"), Presentation.ConstraintCalls > 0);
    int32 Players = 0;
    for (int32 Index = 0; Index < Runtime.GetParticipants().Num(); ++Index)
    {
        const auto& State = Runtime.GetParticipants()[Index];
        TestTrue(TEXT("Participant order and canonical identity remain bound"),
            State.Binding.Character.EntityId == F.Entry.Participants[Index].Character.EntityId);
        TestTrue(TEXT("Current canonical history reaches presentation"), State.Character.EntityStateJson.Contains(TEXT("retained")));
        TestFalse(TEXT("Optional presentation absence is reported safely"), State.bHasOptionalPresentation);
        if (State.Binding.Role == TEXT("player"))
        { ++Players; TestTrue(TEXT("Only canonical Ruler is player"), State.Binding.Character.EntityId == F.Ruler); }
    }
    TestEqual(TEXT("Exactly one Ruler player role"), Players, 1);
    TestTrue(TEXT("Read accepted entry event"), F.Events(Events));
    if (TestEqual(TEXT("One accepted entry event"), Events.Num(), 1))
    {
        const auto Object = Payload(Events[0]);
        if (TestTrue(TEXT("Entry payload is structured"), Object.IsValid()))
        {
            TestEqual(TEXT("Extension context is provenance rather than a whitelist"),
                Object->GetStringField(TEXT("context")), F.Entry.Context.ToString());
            TestEqual(TEXT("Authored context event retained"), Object->GetStringField(TEXT("context_event_id")), F.ContextEvent.ToString());
            TestEqual(TEXT("Authored location retained"), Object->GetStringField(TEXT("location_id")), F.Location.ToString());
            TestEqual(TEXT("All bindings persist in canonical payload"), Object->GetArrayField(TEXT("participants")).Num(), 13);
        }
    }
    TestTrue(TEXT("Optional absence permits action completion"), Runtime.Advance(1, 6, F.Error));
    TestTrue(TEXT("Optional absence permits ordinary exit"), Runtime.Exit(TEXT("player_exit"), 7, F.Error));
    TestEqual(TEXT("Bridge restored once on native exit"), Presentation.RestoreCalls, 1);
    Runtime.SetPresentationBridge(nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSystemicInteractionClockPersistenceTest,
    "OfflineGame.Foundation.Interaction.ActionsSequenceZeroPacingAndSharedClockPersistence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGSystemicInteractionClockPersistenceTest::RunTest(const FString& Parameters)
{
    FInteractionFixture F;
    if (!TestTrue(TEXT("Open canonical fixture"), F.Open())) return false;
    FOGCanonicalClockPolicy Policy;
    Policy.PolicyId = TEXT("test.interaction_clock"); Policy.Version = 1;
    Policy.RateDenominator = 1000; Policy.WorldRateNumerator = 2;
    Policy.RulerRateNumerator = 1; Policy.OfflineRateNumerator = 1;
    Policy.TurnLocalRateDivisor = 2; Policy.MaxDueActionsPerPump = 4;
    Policy.CalendarId = FOGContentId(TEXT("test:calendar.interaction"));
    Policy.LocalTicksPerDay = 10; Policy.DaysPerMonth = 3;
    FOGCanonicalClockRuntime Clock(F.Store);
    if (!TestTrue(TEXT("Existing canonical clock owns the same Store"), Clock.LoadOrCreate(Policy, 1000, F.Error))) return false;
    Clock.ResetOnlineAnchor(10);
    {
        FOGSystemicInteractionRuntime Runtime(F.Store, F.Ruler);
        const auto Graph = F.GraphWithConsequences();
        if (!TestTrue(TEXT("Enter neutral authored action graph"), Runtime.Enter(F.Entry, Graph, Clock.GetCanonicalWorldTick(), F.Error))) return false;
        TestFalse(TEXT("Cannot replace an unfinished action"), Runtime.SelectAction(TEXT("handoff"), F.Error));
        TestFalse(TEXT("Unknown sequence node rejected atomically"), Runtime.SetSequence({FName(TEXT("handoff")), FName(TEXT("absent"))}, F.Error));
        TestFalse(TEXT("Negative pacing rejected"), Runtime.SetPacing(-1, F.Error));
        TestFalse(TEXT("Nonfinite pacing rejected"), Runtime.SetPacing(std::numeric_limits<double>::infinity(), F.Error));
        TestTrue(TEXT("Player can pause action pacing"), Runtime.SetPacing(0, F.Error));
        const int64 Before = Clock.GetCanonicalWorldTick();
        TestTrue(TEXT("Canonical Core clock pump is independent of action pacing"), Clock.PumpOnline(13, 4000, F.Error));
        TestTrue(TEXT("Canonical world advances during zero pacing"), Clock.GetCanonicalWorldTick() > Before);
        TestTrue(TEXT("Paused action accepts independently advanced canonical tick"), Runtime.Advance(100, Clock.GetCanonicalWorldTick(), F.Error));
        TestEqual(TEXT("Zero pacing leaves action uncompleted"), Runtime.GetCurrentNode(), FName(TEXT("align")));
        TArray<FOGWorldEvent> Events; TestTrue(TEXT("Read paused interaction events"), F.Events(Events));
        TestEqual(TEXT("Pause cannot manufacture completed-action events"), Events.Num(), 1);
        TestTrue(TEXT("Valid sequence may be chosen by player"), Runtime.SetSequence({FName(TEXT("handoff")), FName(TEXT("align"))}, F.Error));
        TestTrue(TEXT("Player resumes pacing"), Runtime.SetPacing(1, F.Error));
        TestTrue(TEXT("Authored consequence commits at canonical clock tick"), Runtime.Advance(1, Clock.GetCanonicalWorldTick(), F.Error));
        TestEqual(TEXT("Queued graph transition begins handoff"), Runtime.GetCurrentNode(), FName(TEXT("handoff")));
        TestTrue(TEXT("Independent clock continues during handoff"), Clock.PumpOnline(14, 5000, F.Error));
        TestTrue(TEXT("Handoff completes and next player-selected action begins"), Runtime.Advance(2, Clock.GetCanonicalWorldTick(), F.Error));
        TestEqual(TEXT("Sequence returns to align"), Runtime.GetCurrentNode(), FName(TEXT("align")));
        TestTrue(TEXT("Safe interruption persists on shared clock"), Runtime.Exit(TEXT("application_deactivated"), Clock.GetCanonicalWorldTick(), F.Error));
    }
    F.Store.Close();
    if (!TestTrue(TEXT("Reopen same canonical database"), F.Store.Open(F.DatabasePath, F.Error))) return false;
    TArray<FOGWorldEvent> Events; TestTrue(TEXT("Read persisted enter, two completions and exit"), F.Events(Events));
    TestEqual(TEXT("No replay store or invented action event"), Events.Num(), 4);
    int32 Entered = 0, Completed = 0, Exited = 0;
    FOGEntityId ConsequenceEvent;
    for (const auto& Event : Events)
    {
        if (Event.EventType == TEXT("interaction.entered")) ++Entered;
        if (Event.EventType == TEXT("interaction.action_completed"))
        {
            ++Completed;
            if (Payload(Event)->GetStringField(TEXT("reason")) == TEXT("align")) ConsequenceEvent = Event.EventId;
        }
        if (Event.EventType == TEXT("interaction.exited"))
        { ++Exited; TestEqual(TEXT("Interruption reason persists"), Payload(Event)->GetStringField(TEXT("reason")), FString(TEXT("application_deactivated"))); }
    }
    TestEqual(TEXT("Exactly one canonical entry"), Entered, 1);
    TestEqual(TEXT("Exactly two completed actions"), Completed, 2);
    TestEqual(TEXT("Exactly one canonical exit"), Exited, 1);
    bool Found = false; FOGKnowledgeFactRecord Fact;
    TestTrue(TEXT("Read causally authored knowledge after restart"), F.Store.TryReadKnowledgeFact(F.Ruler,
        TEXT("neutral_handoff_observed"), F.Entry.Participants.Last().Character.EntityId, Found, Fact, F.Error));
    TestTrue(TEXT("Authored knowledge survives restart"), Found);
    TestTrue(TEXT("Knowledge source is the actual completed-action event"), Fact.SourceEventId == ConsequenceEvent);
    TestEqual(TEXT("Consequence uses independently committed world tick"), Fact.UpdatedWorldTick, int64(6));
    FOGSemanticMemoryRecord Memory;
    TestTrue(TEXT("Read authored semantic memory"), F.Store.TryReadSemanticMemory(F.Memory, Found, Memory, F.Error));
    TestTrue(TEXT("Memory shares the same causal event"), Found && Memory.SourceEventId == ConsequenceEvent);
    FOGWorldPresenceRecord Presence;
    TestTrue(TEXT("Read persisted authored presence"), F.Store.TryReadWorldPresence(F.Ruler, Found, Presence, F.Error));
    TestTrue(TEXT("Authored world consequence persists"), Found && Presence.LocalPosition.Equals(FVector(50, 60, 0)));
    TestTrue(TEXT("Reload sole canonical clock from same Store"), Clock.LoadOrCreate(Policy, 5000, F.Error));
    TestEqual(TEXT("Interaction never forks or rewinds canonical time"), Clock.GetCanonicalWorldTick(), int64(8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSystemicInteractionPrivacyParityTest,
    "OfflineGame.Foundation.Interaction.PrivacySubstitutionPreservesAuthoritativeParity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGSystemicInteractionPrivacyParityTest::RunTest(const FString& Parameters)
{
    FInteractionFixture F;
    if (!TestTrue(TEXT("Open canonical parity fixture"), F.Open())) return false;
    const auto Graph = F.GraphWithConsequences();
    TArray<FOGWorldEvent> Visible;
    FOGKnowledgeFactRecord VisibleFact; FOGSemanticMemoryRecord VisibleMemory;
    FOGWorldPresenceRecord VisiblePresence;
    FString InitialJson; FName InitialKind; int64 InitialRevision = 0; bool Found = false;
    TestTrue(TEXT("Capture existing canonical history and progression"), F.Store.TryReadEntity(F.Ruler,
        Found, InitialKind, InitialJson, InitialRevision, F.Error));
    TArray<FOGEntityClassRecord> InitialClasses;
    TestTrue(TEXT("Capture actual canonical class progression"), F.Store.ListEntityClasses(F.Ruler, InitialClasses, F.Error));
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        FInteractionPresentationProbe Presentation;
        FOGSystemicInteractionRuntime Runtime(F.Store, F.Ruler); Runtime.SetPresentationBridge(&Presentation);
        auto Entry = F.Entry; Entry.bPrivacyPresentation = Pass == 1;
        if (!TestTrue(TEXT("Visible and private entry accept identical logical scene"), Runtime.Enter(Entry, Graph, 5, F.Error))) return false;
        const FName Current = Runtime.GetCurrentNode();
        Runtime.SetPrivacyPresentation(Pass == 0);
        Runtime.SetPrivacyPresentation(Pass == 1);
        TestEqual(TEXT("Substitution leaves selected action intact"), Runtime.GetCurrentNode(), Current);
        TestEqual(TEXT("Substitution leaves player pacing intact"), Runtime.GetPacing(), 1.0);
        TestEqual(TEXT("Substitution retains every participant"), Runtime.GetParticipants().Num(), 13);
        for (int32 Index = 0; Index < Runtime.GetParticipants().Num(); ++Index)
            TestTrue(TEXT("Substitution cannot replace canonical bindings"),
                Runtime.GetParticipants()[Index].Binding.Character.EntityId == Entry.Participants[Index].Character.EntityId);
        TestEqual(TEXT("Bridge receives only presentation choice"), Presentation.bPrivacy, Pass == 1);
        TestTrue(TEXT("Same authored action completes under both presentations"), Runtime.Advance(1, 6, F.Error));
        TestTrue(TEXT("Same authored transition remains selectable"), Runtime.SelectAction(TEXT("handoff"), F.Error));
        TestTrue(TEXT("Same action sequence completes under both presentations"), Runtime.Advance(2, 8, F.Error));
        TestTrue(TEXT("Both presentations return through same exit"), Runtime.Exit(TEXT("player_exit"), 9, F.Error));
        Runtime.SetPresentationBridge(nullptr);
        TArray<FOGWorldEvent> Events; TestTrue(TEXT("Read shared canonical event ledger"), F.Events(Events));
        Events.Sort([](const auto& A, const auto& B)
        { return A.WorldTick == B.WorldTick ? A.EventType.ToString() < B.EventType.ToString() : A.WorldTick < B.WorldTick; });
        FOGKnowledgeFactRecord Fact; FOGSemanticMemoryRecord Memory; FOGWorldPresenceRecord Presence;
        TestTrue(TEXT("Read identical authored fact"), F.Store.TryReadKnowledgeFact(F.Ruler,
            TEXT("neutral_handoff_observed"), Entry.Participants.Last().Character.EntityId, Found, Fact, F.Error));
        TestTrue(TEXT("Authored fact exists"), Found);
        TestTrue(TEXT("Read identical authored memory"), F.Store.TryReadSemanticMemory(F.Memory, Found, Memory, F.Error));
        TestTrue(TEXT("Authored memory exists"), Found);
        TestTrue(TEXT("Read identical authored presence"), F.Store.TryReadWorldPresence(F.Ruler, Found, Presence, F.Error));
        TestTrue(TEXT("Authored presence exists"), Found);
        if (Pass == 0)
        { Visible = Events; VisibleFact = Fact; VisibleMemory = Memory; VisiblePresence = Presence; }
        else
        {
            TSet<FOGEntityId> PriorIds; for (const auto& Event : Visible) PriorIds.Add(Event.EventId);
            Events.RemoveAll([&PriorIds](const auto& Event) { return PriorIds.Contains(Event.EventId); });
            if (TestEqual(TEXT("Privacy creates identical event categories and counts"), Events.Num(), Visible.Num()))
                for (int32 Index = 0; Index < Events.Num(); ++Index)
                {
                    TestEqual(TEXT("Privacy preserves event category"), Events[Index].EventType, Visible[Index].EventType);
                    TestEqual(TEXT("Privacy preserves canonical event timestamp"), Events[Index].WorldTick, Visible[Index].WorldTick);
                    TestTrue(TEXT("Privacy preserves event authority"), Events[Index].PrimaryEntity == Visible[Index].PrimaryEntity);
                    TestTrue(TEXT("Privacy preserves participant causality"), Events[Index].RelatedEntities == Visible[Index].RelatedEntities);
                    TestEqual(TEXT("Privacy preserves Chronicle flags"), Events[Index].bChronicleEligible, Visible[Index].bChronicleEligible);
                    TestEqual(TEXT("Privacy preserves every authored payload field"), WithoutGeneratedSessionId(Events[Index]), WithoutGeneratedSessionId(Visible[Index]));
                    const auto Object = Payload(Events[Index]);
                    TestFalse(TEXT("Privacy preference cannot leak into canonical payload"), Object->HasField(TEXT("privacy")) || Object->HasField(TEXT("privacy_presentation")));
                }
            TestEqual(TEXT("Same authored knowledge outcome"), Fact.ValueJson, VisibleFact.ValueJson);
            TestEqual(TEXT("Same knowledge confidence"), Fact.ConfidenceBps, VisibleFact.ConfidenceBps);
            TestEqual(TEXT("Same knowledge belief"), Fact.BeliefState, VisibleFact.BeliefState);
            TestEqual(TEXT("Same learned history tick"), Fact.LearnedWorldTick, VisibleFact.LearnedWorldTick);
            TestEqual(TEXT("Same consequence timestamp"), Fact.UpdatedWorldTick, VisibleFact.UpdatedWorldTick);
            TestTrue(TEXT("Same knowledge source individual"), Fact.SourceEntityId == VisibleFact.SourceEntityId);
            TestEqual(TEXT("Same semantic history outcome"), Memory.StateJson, VisibleMemory.StateJson);
            TestEqual(TEXT("Same semantic history type"), Memory.MemoryTypeId.ToString(), VisibleMemory.MemoryTypeId.ToString());
            TestEqual(TEXT("Same semantic salience"), Memory.SalienceBps, VisibleMemory.SalienceBps);
            TestTrue(TEXT("Same persistent world outcome"), Presence.LocationId == VisiblePresence.LocationId && Presence.LocalPosition.Equals(VisiblePresence.LocalPosition));
            // SourceEventId is generated, but must identify the matching causal completion in each run.
            const auto* Action = Events.FindByPredicate([](const auto& Event)
                { return Event.EventType == TEXT("interaction.action_completed") && Event.WorldTick == 6; });
            TestTrue(TEXT("Privacy knowledge and memory retain actual canonical causality"),
                Action && Fact.SourceEventId == Action->EventId && Memory.SourceEventId == Action->EventId);
        }
        FString Json; FName Kind; int64 Revision = 0;
        TestTrue(TEXT("Read retained authoritative character"), F.Store.TryReadEntity(F.Ruler, Found, Kind, Json, Revision, F.Error));
        TestEqual(TEXT("History and progression never rewritten by presentation"), Json, InitialJson);
        TestEqual(TEXT("Character kind remains canonical"), Kind, InitialKind);
        TestEqual(TEXT("Existing entity metadata remains canonical"), Revision, InitialRevision);
        TArray<FOGEntityClassRecord> Classes;
        TestTrue(TEXT("Read actual canonical class progression"), F.Store.ListEntityClasses(F.Ruler, Classes, F.Error));
        if (TestEqual(TEXT("Privacy and neutral actions preserve class count"), Classes.Num(), InitialClasses.Num()))
            for (int32 Index = 0; Index < Classes.Num(); ++Index)
            {
                TestTrue(TEXT("Canonical class identity and owner retained"), Classes[Index].OwnerEntityId == InitialClasses[Index].OwnerEntityId && Classes[Index].ClassId == InitialClasses[Index].ClassId);
                TestEqual(TEXT("Canonical attained tier retained"), Classes[Index].AttainedTier, InitialClasses[Index].AttainedTier);
                TestEqual(TEXT("Canonical expression retained"), Classes[Index].CurrentExpressionState, InitialClasses[Index].CurrentExpressionState);
                TestEqual(TEXT("Canonical recognition history retained"), Classes[Index].RecognizedWorldTick, InitialClasses[Index].RecognizedWorldTick);
                TestEqual(TEXT("Canonical progression timestamp retained"), Classes[Index].UpdatedWorldTick, InitialClasses[Index].UpdatedWorldTick);
                TestEqual(TEXT("Canonical progression state retained"), Classes[Index].StateJson, InitialClasses[Index].StateJson);
            }
        int64 Balance = 0;
        TestTrue(TEXT("Read existing unrelated resource"), F.Store.TryReadResourceBalance(F.Ruler,
            FOGContentId(TEXT("test:resource.unrelated")), Found, Balance, F.Error));
        TestEqual(TEXT("No universal interaction reward economy"), Balance, int64(29));
    }
    return true;
}

namespace
{
struct FInteractionTestWorld
{
    UWorld* World = nullptr;
    FInteractionTestWorld()
    {
        UWorld::InitializationValues Values;
        Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, true, NAME_None, nullptr, true,
            ERHIFeatureLevel::SM5, &Values, false);
    }
    ~FInteractionTestWorld() { if (World) World->DestroyWorld(true); }
    void AddFloor()
    {
        AActor* Floor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
        Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(5000, 5000, 50));
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetCollisionObjectType(ECC_WorldStatic); Box->RegisterComponent();
        Floor->SetActorLocation(FVector(0, 0, -50));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGSystemicInteractionGameplayRestoreTest,
    "OfflineGame.Foundation.Interaction.StagingFailureInterruptionAndFailedExitRestoreGameplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOGSystemicInteractionGameplayRestoreTest::RunTest(const FString& Parameters)
{
    FInteractionFixture F;
    if (!TestTrue(TEXT("Open canonical embodied fixture"), F.Open(1))) return false;
    FInteractionTestWorld W;
    if (!TestNotNull(TEXT("Isolated staging world"), W.World)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACharacter* Actor = W.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(700, 100, 200), FRotator(0, 35, 0), Spawn);
    APlayerController* Controller = W.World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Native participant body"), Actor) || !TestNotNull(TEXT("Native return controller"), Controller)) return false;
    Controller->SpawnPlayerCameraManager();
    if (!TestNotNull(TEXT("Synthetic controller has native camera manager"), Controller->PlayerCameraManager.Get())) return false;
    auto* Movement = Actor->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Flying); Movement->Velocity = FVector(5, 6, 7);
    const FTransform Before = Actor->GetActorTransform(); const FVector Velocity = Movement->Velocity;
    Controller->SetViewTarget(Actor); Controller->SetControlRotation(FRotator(10, 25, 0));
    const FRotator ControlRotation = Controller->GetControlRotation();
    // Prior suppression belongs to ordinary gameplay and must survive our exit.
    Controller->SetIgnoreMoveInput(true);
    F.Entry.World = W.World; F.Entry.ReturnController = Controller;
    F.Entry.Participants[0].Actor = Actor;
    FInteractionPresentationProbe Presentation;
    FOGSystemicInteractionRuntime Runtime(F.Store, F.Ruler);
    Runtime.SetPresentationBridge(&Presentation);
    const auto Graph = FOGSystemicInteractionRuntime::MakeNeutralDiagnosticGraph();
    TestFalse(TEXT("No physical floor fails staging safely"), Runtime.Enter(F.Entry, Graph, 5, F.Error));
    TestFalse(TEXT("Failed staging never activates"), Runtime.IsActive());
    TestTrue(TEXT("Failed staging preserves original body transform"), Actor->GetActorTransform().Equals(Before));
    TestTrue(TEXT("Failed staging preserves original velocity"), Movement->Velocity.Equals(Velocity));
    TestTrue(TEXT("Failed staging preserves original movement mode"), Movement->MovementMode == MOVE_Flying);
    TestFalse(TEXT("Failed staging preserves visibility"), Actor->IsHidden());
    TestTrue(TEXT("Failed staging preserves existing move suppression"), Controller->IsMoveInputIgnored());
    TestFalse(TEXT("Failed staging does not own look suppression"), Controller->IsLookInputIgnored());
    TestTrue(TEXT("Failed staging preserves ordinary camera"), Controller->GetViewTarget() == Actor);
    TestEqual(TEXT("Failed staging does not touch ordinary character presentation"), Presentation.CharacterCalls, 0);
    TArray<FOGWorldEvent> Events; TestTrue(TEXT("Read failed staging events"), F.Events(Events));
    TestEqual(TEXT("Failed staging persists no entry"), Events.Num(), 0);
    W.AddFloor();
    if (!TestTrue(TEXT("Nearby clear floor permits embodied staging"), Runtime.Enter(F.Entry, Graph, 5, F.Error))) return false;
    TestTrue(TEXT("Staging owns ordinary movement temporarily"), Movement->MovementMode == MOVE_None);
    TestTrue(TEXT("Staging owns look suppression"), Controller->IsLookInputIgnored());
    Runtime.SetPrivacyPresentation(true); TestTrue(TEXT("Privacy masks actual body"), Actor->IsHidden());
    Runtime.SetPrivacyPresentation(false); TestFalse(TEXT("Privacy reversal preserves saved visibility"), Actor->IsHidden());
    TestTrue(TEXT("Player camera uses native camera hook"), Runtime.SetCamera(FTransform(FRotator(0, 180, 0), FVector(400, 0, 200)), F.Error));
    TestTrue(TEXT("Interaction camera temporarily becomes view target"), Controller->GetViewTarget() != Actor);
    const FTransform Staged = Actor->GetActorTransform();
    TestFalse(TEXT("Invalid restaging rejected without corrupting current placement"),
        Runtime.Restage(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), F.Error));
    TestTrue(TEXT("Restaging failure preserves current safe placement"), Actor->GetActorTransform().Equals(Staged));
    TestTrue(TEXT("Native application-context interruption exits cleanly"), Runtime.Exit(TEXT("application_deactivated"), 6, F.Error));
    TestFalse(TEXT("Interrupted interaction inactive"), Runtime.IsActive());
    TestTrue(TEXT("Interruption restores original transform"), Actor->GetActorTransform().Equals(Before));
    TestTrue(TEXT("Interruption restores velocity"), Movement->Velocity.Equals(Velocity));
    TestTrue(TEXT("Interruption restores movement mode"), Movement->MovementMode == MOVE_Flying);
    TestFalse(TEXT("Interruption restores original visibility"), Actor->IsHidden());
    TestTrue(TEXT("Interruption preserves move suppression owned by ordinary gameplay"), Controller->IsMoveInputIgnored());
    TestFalse(TEXT("Interruption releases only its own look suppression"), Controller->IsLookInputIgnored());
    TestTrue(TEXT("Interruption restores ordinary view target"), Controller->GetViewTarget() == Actor);
    TestTrue(TEXT("Interruption restores ordinary control rotation"), Controller->GetControlRotation().Equals(ControlRotation));
    TestEqual(TEXT("Interruption restores character bridge once"), Presentation.RestoreCalls, 1);
    TestTrue(TEXT("Entry works again after clean interruption"), Runtime.Enter(F.Entry, Graph, 7, F.Error));
    Runtime.SetPrivacyPresentation(true);
    TestFalse(TEXT("Unavailable canonical clock reports failed exit persistence"), Runtime.Exit(TEXT("core_unavailable"), -1, F.Error));
    TestFalse(TEXT("Failed exit persistence cannot trap active runtime"), Runtime.IsActive());
    TestTrue(TEXT("Failed exit persistence still restores body"), Actor->GetActorTransform().Equals(Before));
    TestFalse(TEXT("Failed exit persistence still restores visibility"), Actor->IsHidden());
    TestTrue(TEXT("Failed exit persistence still restores movement"), Movement->MovementMode == MOVE_Flying && Movement->Velocity.Equals(Velocity));
    TestTrue(TEXT("Failed exit persistence preserves ordinary move ownership"), Controller->IsMoveInputIgnored());
    TestFalse(TEXT("Failed exit persistence releases look ownership"), Controller->IsLookInputIgnored());
    TestTrue(TEXT("Failed exit persistence restores ordinary camera"), Controller->GetViewTarget() == Actor);
    TestEqual(TEXT("Every active session restores character bridge once"), Presentation.RestoreCalls, 2);
    Runtime.SetPresentationBridge(nullptr);
    return true;
}

#endif
