#include "Runtime/OGCanonicalClockRuntime.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Containers/UnrealString.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "World/OGWorldTimeService.h"

namespace
{
    bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& Out, FString& Error)
    {
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Out) || !Out.IsValid())
        { Error = TEXT("Canonical clock JSON must be an object."); return false; }
        return true;
    }
    // Persist all int64 values as decimal strings: JSON double loses ticks > 2^53.
    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Out, FString& Error)
    {
        FString Value;
        if (!Object->TryGetStringField(Key, Value) || Value.IsEmpty())
        { Error = FString::Printf(TEXT("Clock integer field %s must be a decimal string."), Key); return false; }
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            if (!FChar::IsDigit(Value[Index]))
            { Error = FString::Printf(TEXT("Invalid clock integer field %s."), Key); return false; }
        }
        if (!LexTryParseString(Out, *Value) || Out < 0 || LexToString(Out) != Value)
        { Error = FString::Printf(TEXT("Clock integer field %s is noncanonical or out of range."), Key); return false; }
        return true;
    }
    void Integer(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64 Value)
    { Object->SetStringField(Key, LexToString(Value)); }
    bool ReadPolicy(const TSharedPtr<FJsonObject>& Object, FOGCanonicalClockPolicy& Policy, FString& Error)
    {
        FString Calendar;
        int64 MaxDue = 0;
        if (!Object->TryGetStringField(TEXT("policy_id"), Policy.PolicyId) ||
            !Object->TryGetStringField(TEXT("calendar_id"), Calendar) ||
            !ReadInteger(Object, TEXT("version"), Policy.Version, Error) ||
            !ReadInteger(Object, TEXT("rate_denominator"), Policy.RateDenominator, Error) ||
            !ReadInteger(Object, TEXT("world_rate_numerator"), Policy.WorldRateNumerator, Error) ||
            !ReadInteger(Object, TEXT("ruler_rate_numerator"), Policy.RulerRateNumerator, Error) ||
            !ReadInteger(Object, TEXT("offline_rate_numerator"), Policy.OfflineRateNumerator, Error) ||
            !ReadInteger(Object, TEXT("turn_local_rate_divisor"), Policy.TurnLocalRateDivisor, Error) ||
            !ReadInteger(Object, TEXT("max_due_actions_per_pump"), MaxDue, Error) ||
            !ReadInteger(Object, TEXT("local_ticks_per_day"), Policy.LocalTicksPerDay, Error) ||
            !ReadInteger(Object, TEXT("days_per_month"), Policy.DaysPerMonth, Error) || MaxDue > MAX_int32)
        { if (Error.IsEmpty()) Error = TEXT("Clock authored policy is incomplete."); return false; }
        Policy.CalendarId = FOGContentId(Calendar);
        Policy.MaxDueActionsPerPump = static_cast<int32>(MaxDue);
        return Policy.Validate(Error);
    }
    TSharedPtr<FJsonObject> WritePolicy(const FOGCanonicalClockPolicy& Policy)
    {
        auto Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("policy_id"), Policy.PolicyId);
        Object->SetStringField(TEXT("calendar_id"), Policy.CalendarId.ToString());
        Integer(Object, TEXT("version"), Policy.Version);
        Integer(Object, TEXT("rate_denominator"), Policy.RateDenominator);
        Integer(Object, TEXT("world_rate_numerator"), Policy.WorldRateNumerator);
        Integer(Object, TEXT("ruler_rate_numerator"), Policy.RulerRateNumerator);
        Integer(Object, TEXT("offline_rate_numerator"), Policy.OfflineRateNumerator);
        Integer(Object, TEXT("turn_local_rate_divisor"), Policy.TurnLocalRateDivisor);
        Integer(Object, TEXT("max_due_actions_per_pump"), Policy.MaxDueActionsPerPump);
        Integer(Object, TEXT("local_ticks_per_day"), Policy.LocalTicksPerDay);
        Integer(Object, TEXT("days_per_month"), Policy.DaysPerMonth);
        return Object;
    }
    FString Serialize(const TSharedPtr<FJsonObject>& Object)
    {
        FString Result;
        FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
        return Result;
    }
    bool ReadId(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FOGEntityId& Id, FString& Error)
    {
        FString Value;
        if (!Object->TryGetStringField(Key, Value) || !FGuid::Parse(Value, Id.Value) || !Id.IsValid())
        { Error = FString::Printf(TEXT("Invalid clock registry ID %s."), Key); return false; }
        return true;
    }
}

bool FOGCanonicalClockPolicy::LoadJsonFile(const FString& Path, FOGCanonicalClockPolicy& OutPolicy, FString& OutError)
{
    OutError.Reset();
    FString Json;
    TSharedPtr<FJsonObject> Object;
    if (!FFileHelper::LoadFileToString(Json, *Path))
    { OutError = FString::Printf(TEXT("Cannot load authored clock policy %s."), *Path); return false; }
    return ParseObject(Json, Object, OutError) && ReadPolicy(Object, OutPolicy, OutError);
}

bool FOGCanonicalClockPolicy::Validate(FString& OutError) const
{
    if (PolicyId.IsEmpty() || Version <= 0 || RateDenominator <= 0 || WorldRateNumerator <= 0 ||
        RulerRateNumerator <= 0 || RulerRateNumerator >= WorldRateNumerator || OfflineRateNumerator <= 0 ||
        TurnLocalRateDivisor <= 1 || MaxDueActionsPerPump <= 0 || !CalendarId.IsValid() ||
        LocalTicksPerDay <= 0 || DaysPerMonth <= 0 || DaysPerMonth > MAX_int64 / LocalTicksPerDay)
    { OutError = TEXT("Authored clock rates/calendar/budget are invalid."); return false; }
    return true;
}

FOGEntityId FOGCanonicalClockRuntime::ClockEntityId()
{ return FOGEntityId(FGuid(0xC10C0400, 0xF0014000, 0x8A100000, 0x00000001)); }

bool FOGCanonicalClockRuntime::LoadOrCreate(const FOGCanonicalClockPolicy& InPolicy, int64 NowUtcMilliseconds, FString& OutError)
{
    OutError.Reset();
    bLoaded = false;
    if (!Store.IsOpen() || NowUtcMilliseconds < 0 || !InPolicy.Validate(OutError)) return false;
    bool bFound = false;
    FName Kind;
    FString Json;
    int64 Created = 0;
    if (!Store.TryReadEntity(ClockEntityId(), bFound, Kind, Json, Created, OutError)) return false;
    Policy = InPolicy;
    DueActions.Reset();
    State = FOGCanonicalClockState();
    State.RateDenominator = Policy.RateDenominator;
    if (bFound)
    {
        TSharedPtr<FJsonObject> Object;
        int64 Version = 0;
        const TSharedPtr<FJsonObject>* StoredPolicy = nullptr;
        if (Kind != FName(TEXT("runtime.canonical_clock")) || !ParseObject(Json, Object, OutError) ||
            !ReadInteger(Object, TEXT("record_version"), Version, OutError) || Version != 1 ||
            !Object->TryGetObjectField(TEXT("policy"), StoredPolicy) || !ReadPolicy(*StoredPolicy, Policy, OutError) ||
            !ReadInteger(Object, TEXT("world_tick"), State.CurrentWorldTick, OutError) ||
            !ReadInteger(Object, TEXT("fractional_numerator"), State.FractionalNumerator, OutError) ||
            !ReadInteger(Object, TEXT("last_utc_milliseconds"), LastUtcMilliseconds, OutError))
        { if (OutError.IsEmpty()) OutError = TEXT("Persisted canonical clock record is invalid."); return false; }
        // Imported worlds retain their exact authored rates/residue until an explicit migration.
        State.RateDenominator = Policy.RateDenominator;
        if (State.FractionalNumerator >= State.RateDenominator)
        { OutError = TEXT("Persisted canonical clock residue is invalid."); return false; }
        const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
        if (!Object->TryGetArrayField(TEXT("due_actions"), Actions))
        { OutError = TEXT("Persisted clock due registry is missing."); return false; }
        TSet<FOGEntityId> Seen;
        for (const auto& Value : *Actions)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            FDueAction Action;
            FString Key;
            int64 Loss = 0, MaxLoss = 0;
            if (!Value->TryGetObject(Entry) || !ReadId(*Entry, TEXT("schedule_id"), Action.ScheduleId, OutError) ||
                !ReadId(*Entry, TEXT("scope_id"), Action.ScopeId, OutError) ||
                !(*Entry)->TryGetStringField(TEXT("resolver_key"), Key) || Key.IsEmpty() ||
                !ReadInteger(*Entry, TEXT("requested_loss_bps"), Loss, OutError) ||
                !ReadInteger(*Entry, TEXT("maximum_loss_bps"), MaxLoss, OutError) || Loss > 10000 || MaxLoss > 10000 ||
                !(*Entry)->TryGetBoolField(TEXT("causally_locked"), Action.bCausallyLocked) ||
                !(*Entry)->TryGetBoolField(TEXT("protected_collapse"), Action.bProtectedCollapse) ||
                !(*Entry)->TryGetBoolField(TEXT("offline_pending"), Action.bOfflinePending) || Seen.Contains(Action.ScheduleId))
            { if (OutError.IsEmpty()) OutError = TEXT("Invalid canonical due registry entry."); return false; }
            Seen.Add(Action.ScheduleId);
            Action.ResolverKey = FName(*Key);
            Action.RequestedLossBps = static_cast<int32>(Loss);
            Action.MaximumLossBps = static_cast<int32>(MaxLoss);
            DueActions.Add(Action);
        }
    }
    else
    {
        LastUtcMilliseconds = NowUtcMilliseconds;
        if (!Save(State, LastUtcMilliseconds, OutError)) return false;
    }
    bLoaded = true;
    bHasOnlineAnchor = false;
    return true;
}

bool FOGCanonicalClockRuntime::Save(const FOGCanonicalClockState& Candidate, int64 UtcAnchor, FString& OutError)
{
    auto Object = MakeShared<FJsonObject>();
    Integer(Object, TEXT("record_version"), 1);
    Integer(Object, TEXT("world_tick"), Candidate.CurrentWorldTick);
    Integer(Object, TEXT("fractional_numerator"), Candidate.FractionalNumerator);
    Integer(Object, TEXT("last_utc_milliseconds"), UtcAnchor);
    Object->SetObjectField(TEXT("policy"), WritePolicy(Policy));
    TArray<TSharedPtr<FJsonValue>> Entries;
    for (const FDueAction& Action : DueActions)
    {
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("schedule_id"), Action.ScheduleId.ToString());
        Entry->SetStringField(TEXT("scope_id"), Action.ScopeId.ToString());
        Entry->SetStringField(TEXT("resolver_key"), Action.ResolverKey.ToString());
        Integer(Entry, TEXT("requested_loss_bps"), Action.RequestedLossBps);
        Integer(Entry, TEXT("maximum_loss_bps"), Action.MaximumLossBps);
        Entry->SetBoolField(TEXT("causally_locked"), Action.bCausallyLocked);
        Entry->SetBoolField(TEXT("protected_collapse"), Action.bProtectedCollapse);
        Entry->SetBoolField(TEXT("offline_pending"), Action.bOfflinePending);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Object->SetArrayField(TEXT("due_actions"), Entries);
    if (!Store.BeginTransaction(OutError)) return false;
    if (!Store.UpsertEntity(ClockEntityId(), FName(TEXT("runtime.canonical_clock")), 0, Serialize(Object), OutError) ||
        !Store.CommitTransaction(OutError))
    { FString Rollback; Store.RollbackTransaction(Rollback); return false; }
    State = Candidate;
    LastUtcMilliseconds = UtcAnchor;
    ++CommitRevision;
    return true;
}

void FOGCanonicalClockRuntime::ResetOnlineAnchor(double NowMonotonicSeconds)
{
    LastMonotonicSeconds = NowMonotonicSeconds;
    bHasOnlineAnchor = FMath::IsFinite(NowMonotonicSeconds) && NowMonotonicSeconds >= 0;
    OnlineFractionalMilliseconds = 0;
}

bool FOGCanonicalClockRuntime::AdvanceSource(int64 Milliseconds, int64 Numerator, int64 UtcAnchor, bool bOffline, FString& OutError)
{
    if (!bLoaded) { OutError = TEXT("Canonical clock is unavailable."); return false; }
    FOGCanonicalClockState Candidate;
    if (!FOGCanonicalClockAccumulator::Advance(State, Milliseconds, Numerator, Candidate, OutError)) return false;
    const TArray<FDueAction> PreviousActions = DueActions;
    if (bOffline)
    {
        for (auto& Action : DueActions)
        {
            bool bFound = false;
            FOGWorldDirectorScheduleRecord Schedule;
            if (!Store.TryReadWorldDirectorSchedule(Action.ScheduleId, bFound, Schedule, OutError))
            { DueActions = PreviousActions; return false; }
            if (bFound && Schedule.bHasScheduledStartWorldTick && Schedule.ScheduledStartWorldTick <= Candidate.CurrentWorldTick &&
                (Schedule.Status == FName(TEXT("scheduled")) || Schedule.Status == FName(TEXT("active")))) Action.bOfflinePending = true;
        }
    }
    if (!Save(Candidate, FMath::Max(UtcAnchor, LastUtcMilliseconds), OutError))
    { DueActions = PreviousActions; return false; }
    // Director owns its own transactions; persisted active schedules are retryable.
    return DrainDueActions(bOffline, OutError);
}

bool FOGCanonicalClockRuntime::PumpOnline(double NowMonotonicSeconds, int64 NowUtcMilliseconds, FString& OutError)
{
    OutError.Reset();
    if (!bLoaded || !FMath::IsFinite(NowMonotonicSeconds) || NowMonotonicSeconds < 0 || NowUtcMilliseconds < 0)
    { OutError = TEXT("Invalid canonical clock online sample."); return false; }
    if (!bHasOnlineAnchor) { ResetOnlineAnchor(NowMonotonicSeconds); return DrainDueActions(false, OutError); }
    if (NowMonotonicSeconds < LastMonotonicSeconds)
    { OutError = TEXT("Monotonic source moved backward."); return false; }
    const double Milliseconds = (NowMonotonicSeconds - LastMonotonicSeconds) * 1000.0 + OnlineFractionalMilliseconds;
    if (!FMath::IsFinite(Milliseconds) || Milliseconds >= static_cast<double>(MAX_int64))
    { OutError = TEXT("Online elapsed input exceeds integer range."); return false; }
    const int64 WholeMilliseconds = static_cast<int64>(Milliseconds);
    if (WholeMilliseconds == 0) return true;
    const int64 Numerator = bPaused ? 0 : (Mode == EOGCanonicalClockMode::Ruler ? Policy.RulerRateNumerator : Policy.WorldRateNumerator);
    const uint64 BeforeCommit = CommitRevision;
    const bool bResult = AdvanceSource(WholeMilliseconds, Numerator, NowUtcMilliseconds, false, OutError);
    // A resolver can fail after clock commit: elapsed must still be consumed once.
    if (bResult || CommitRevision != BeforeCommit)
    { LastMonotonicSeconds = NowMonotonicSeconds; OnlineFractionalMilliseconds = Milliseconds - WholeMilliseconds; }
    return bResult;
}

bool FOGCanonicalClockRuntime::CatchupOffline(int64 NowUtcMilliseconds, FString& OutError)
{
    OutError.Reset();
    if (!bLoaded || NowUtcMilliseconds < 0) { OutError = TEXT("Invalid offline clock sample."); return false; }
    // Backward device time never rewinds canonical history or manufactures elapsed.
    const int64 Elapsed = NowUtcMilliseconds > LastUtcMilliseconds ? NowUtcMilliseconds - LastUtcMilliseconds : 0;
    bHasOnlineAnchor = false;
    return AdvanceSource(Elapsed, bPaused ? 0 : Policy.OfflineRateNumerator, NowUtcMilliseconds, true, OutError);
}

bool FOGCanonicalClockRuntime::RecordBoundary(int64 NowUtcMilliseconds, FString& OutError)
{
    if (!bLoaded || NowUtcMilliseconds < 0) { OutError = TEXT("Cannot record unavailable canonical boundary."); return false; }
    if (!Save(State, FMath::Max(NowUtcMilliseconds, LastUtcMilliseconds), OutError)) return false;
    // Each registered simulation scope has an authoritative active-session anchor.
    TSet<FOGEntityId> Scopes;
    for (const auto& Action : DueActions) Scopes.Add(Action.ScopeId);
    for (const auto& Scope : Scopes)
        if (!Director.RecordActiveSessionBoundary(Scope, State.CurrentWorldTick, OutError)) return false;
    return true;
}

void FOGCanonicalClockRuntime::RegisterResolver(FName Key, FOGCanonicalDueActionResolver Resolver)
{ if (!Key.IsNone() && Resolver) Resolvers.Add(Key, MoveTemp(Resolver)); }

bool FOGCanonicalClockRuntime::RegisterDueAction(const FOGEntityId& ScheduleId, FName Key,
    const FOGEntityId& ScopeId, int32 Loss, int32 MaxLoss, bool bLocked, bool bProtected, FString& OutError)
{
    if (!bLoaded || !ScheduleId.IsValid() || Key.IsNone() || !ScopeId.IsValid() || Loss < 0 || Loss > 10000 || MaxLoss < 0 || MaxLoss > 10000)
    { OutError = TEXT("Invalid canonical due action registration."); return false; }
    for (const auto& Existing : DueActions)
        if (Existing.ScheduleId == ScheduleId)
        {
            if (Existing.ResolverKey == Key && Existing.ScopeId == ScopeId && Existing.RequestedLossBps == Loss &&
                Existing.MaximumLossBps == MaxLoss && Existing.bCausallyLocked == bLocked && Existing.bProtectedCollapse == bProtected) return true;
            OutError = TEXT("Due action registration conflicts with persisted contract."); return false;
        }
    bool bFound = false;
    FOGWorldDirectorScheduleRecord Schedule;
    if (!Store.TryReadWorldDirectorSchedule(ScheduleId, bFound, Schedule, OutError)) return false;
    if (!bFound || !Schedule.bHasScheduledStartWorldTick)
    { OutError = TEXT("Due action requires an existing Director schedule."); return false; }
    if (Schedule.Status == FName(TEXT("completed"))) return true;
    bool bBoundary = false;
    FOGOfflineSimulationStateRecord Boundary;
    if (!Store.TryReadOfflineSimulationState(ScopeId, bBoundary, Boundary, OutError)) return false;
    if (!bBoundary && !Director.RecordActiveSessionBoundary(ScopeId, State.CurrentWorldTick, OutError)) return false;
    FDueAction Action;
    Action.ScheduleId = ScheduleId; Action.ResolverKey = Key; Action.ScopeId = ScopeId;
    Action.RequestedLossBps = Loss; Action.MaximumLossBps = MaxLoss;
    Action.bCausallyLocked = bLocked; Action.bProtectedCollapse = bProtected;
    DueActions.Add(Action);
    if (!Save(State, LastUtcMilliseconds, OutError)) { DueActions.Pop(); return false; }
    return true;
}

bool FOGCanonicalClockRuntime::DrainDueActions(bool bOffline, FString& OutError)
{
    struct FReady { FDueAction Action; FOGWorldDirectorScheduleRecord Schedule; };
    TArray<FReady> Ready;
    TSet<FOGEntityId> Completed;
    for (const auto& Action : DueActions)
    {
        bool bFound = false;
        FOGWorldDirectorScheduleRecord Schedule;
        if (!Store.TryReadWorldDirectorSchedule(Action.ScheduleId, bFound, Schedule, OutError)) return false;
        if (!bFound) { OutError = TEXT("Persisted due action references a missing Director schedule."); return false; }
        if (Schedule.Status == FName(TEXT("completed"))) Completed.Add(Action.ScheduleId);
        if ((Schedule.Status == FName(TEXT("scheduled")) || Schedule.Status == FName(TEXT("active"))) &&
            Schedule.bHasScheduledStartWorldTick && Schedule.ScheduledStartWorldTick <= State.CurrentWorldTick)
        { FReady Entry; Entry.Action = Action; Entry.Schedule = Schedule; Ready.Add(Entry); }
    }
    if (!Completed.IsEmpty())
    {
        DueActions.RemoveAll([&Completed](const FDueAction& Action) { return Completed.Contains(Action.ScheduleId); });
        if (!Save(State, LastUtcMilliseconds, OutError)) return false;
    }
    Ready.Sort([](const FReady& A, const FReady& B)
    {
        if (A.Schedule.ScheduledStartWorldTick != B.Schedule.ScheduledStartWorldTick)
            return A.Schedule.ScheduledStartWorldTick < B.Schedule.ScheduledStartWorldTick;
        // Content/seed provide deterministic order; stable persisted schedule is final tie break.
        if (A.Schedule.ContentId != B.Schedule.ContentId) return A.Schedule.ContentId.ToString() < B.Schedule.ContentId.ToString();
        if (A.Schedule.ResolutionSeed != B.Schedule.ResolutionSeed) return A.Schedule.ResolutionSeed < B.Schedule.ResolutionSeed;
        return A.Schedule.ScheduleId.ToString() < B.Schedule.ScheduleId.ToString();
    });
    int32 Processed = 0;
    for (auto& Entry : Ready)
    {
        if (Processed++ >= Policy.MaxDueActionsPerPump) break;
        FOGCanonicalDueActionResolver* Resolver = Resolvers.Find(Entry.Action.ResolverKey);
        if (!Resolver) { OutError = FString::Printf(TEXT("Missing due action resolver %s."), *Entry.Action.ResolverKey.ToString()); return false; }
        const bool bGovernOffline = bOffline || Entry.Action.bOfflinePending;
        if (bGovernOffline)
        {
            FOGOfflineCatchupGovernorRequest Request;
            Request.ScopeEntityId = Entry.Action.ScopeId;
            Request.CatchupWorldTick = State.CurrentWorldTick;
            Request.RequestedNewIrreversibleLossBps = Entry.Action.RequestedLossBps;
            Request.MaxNewIrreversibleLossBps = Entry.Action.MaximumLossBps;
            Request.bCausallyLockedBeforeLogout = Entry.Action.bCausallyLocked;
            Request.bWouldCauseProtectedCatastrophicCollapse = Entry.Action.bProtectedCollapse;
            auto Decision = MakeShared<FJsonObject>();
            Decision->SetStringField(TEXT("schedule_id"), Entry.Action.ScheduleId.ToString());
            Decision->SetStringField(TEXT("resolver"), Entry.Action.ResolverKey.ToString());
            Request.DecisionStateJson = Serialize(Decision);
            FOGOfflineCatchupGovernorResult Result;
            if (!Director.EvaluateOfflineCatchupGovernor(Request, Result, OutError)) return false;
            if (!Result.bApproved) continue;
        }
        if (Entry.Schedule.Status == FName(TEXT("scheduled")))
        {
            if (!Director.ActivateDueContent(Entry.Action.ScheduleId, State.CurrentWorldTick, OutError)) return false;
            Entry.Schedule.Status = FName(TEXT("active"));
        }
        FString Outcome;
        if (!(*Resolver)(Store, Entry.Schedule, State.CurrentWorldTick, bGovernOffline, Outcome, OutError)) return false;
        if (!Director.CompleteSchedule(Entry.Action.ScheduleId, Entry.Schedule.ScheduledStartWorldTick, Outcome, OutError)) return false;
        DueActions.RemoveAll([&Entry](const FDueAction& Action) { return Action.ScheduleId == Entry.Action.ScheduleId; });
        if (!Save(State, LastUtcMilliseconds, OutError)) return false;
    }
    return true;
}

bool FOGCanonicalClockRuntime::SetLocalTurnBattle(const FOGEntityId& DomainId, bool bActive, FString& OutError)
{
    if (!bLoaded || !DomainId.IsValid()) { OutError = TEXT("Local turn battle needs ready canonical time/domain."); return false; }
    bool bFound = false;
    FOGTimeDomainRecord Domain;
    if (!Store.TryReadTimeDomain(DomainId, bFound, Domain, OutError)) return false;
    if (!bFound) { OutError = TEXT("Turn battle domain does not exist."); return false; }
    TSharedPtr<FJsonObject> Metadata;
    if (!ParseObject(Domain.StateJson, Metadata, OutError)) return false;
    const TSharedPtr<FJsonObject>* Prior = nullptr;
    const bool bFrozen = Metadata->TryGetObjectField(TEXT("canonical_clock_turn_restore"), Prior);
    if (bFrozen == bActive) return true;
    FOGWorldTimeService Time(Store);
    FOGLocalTimeProjection Local;
    if (!Time.ProjectCanonicalTick(DomainId, State.CurrentWorldTick, Local, OutError)) return false;
    int64 ParentTick = State.CurrentWorldTick;
    if (Domain.ParentTimeDomainId.IsValid())
    {
        FOGLocalTimeProjection Parent;
        if (!Time.ProjectCanonicalTick(Domain.ParentTimeDomainId, State.CurrentWorldTick, Parent, OutError)) return false;
        ParentTick = Parent.LocalTick;
    }
    if (bActive)
    {
        if (Domain.RateDenominator > MAX_int64 / Policy.TurnLocalRateDivisor)
        { OutError = TEXT("Authored local turn slowdown exceeds rational rate range."); return false; }
        auto Restore = MakeShared<FJsonObject>();
        Integer(Restore, TEXT("numerator"), Domain.RateNumerator);
        Integer(Restore, TEXT("denominator"), Domain.RateDenominator);
        Metadata->SetObjectField(TEXT("canonical_clock_turn_restore"), Restore);
        Domain.RateDenominator *= Policy.TurnLocalRateDivisor;
    }
    else
    {
        if (!ReadInteger(*Prior, TEXT("numerator"), Domain.RateNumerator, OutError) ||
            !ReadInteger(*Prior, TEXT("denominator"), Domain.RateDenominator, OutError)) return false;
        Metadata->RemoveField(TEXT("canonical_clock_turn_restore"));
    }
    Domain.ParentEpochTick = ParentTick;
    Domain.LocalEpochTick = Local.LocalTick;
    Domain.StateJson = Serialize(Metadata);
    return Time.SaveTimeDomain(Domain, State.CurrentWorldTick, OutError);
}
