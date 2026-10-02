#include "World/OGTerritoryProjectService.h"

#include "Events/OGWorldEvent.h"

bool FOGTerritoryProjectService::ApplyCoreDurabilityDamage(
    const FOGEntityId& CoreId,
    const FOGLargeNumber& Damage,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (Damage.GetSign() < 0)
    {
        OutError = TEXT("Domain Core damage cannot be negative.");
        return false;
    }

    bool bFound = false;
    FOGDomainCoreRecord Core;

    if (!Store.TryReadDomainCore(
            CoreId,
            bFound,
            Core,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Domain Core does not exist.");
        return false;
    }

    if (Core.Lifecycle == EOGDomainCoreLifecycle::Broken ||
        Core.Lifecycle == EOGDomainCoreLifecycle::Absorbed)
    {
        OutError = TEXT("Domain Core is already broken or absorbed.");
        return false;
    }

    const FOGLargeNumber NegativeDamage(
        -Damage.Significand,
        Damage.Exponent10);

    Core.CurrentDurability =
        FOGLargeNumber::Add(
            Core.CurrentDurability,
            NegativeDamage);

    bool bJustBroken = false;

    if (FOGLargeNumber::Compare(
            Core.CurrentDurability,
            FOGLargeNumber()) <= 0)
    {
        Core.CurrentDurability = FOGLargeNumber();
        Core.Lifecycle = EOGDomainCoreLifecycle::Broken;
        Core.ControllerRulerId = FOGEntityId();
        bJustBroken = true;
    }

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    if (!Store.UpsertDomainCore(
            Core,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    if (bJustBroken)
    {
        FOGWorldEvent Event;
        Event.EventId = FOGEntityId::NewId();
        Event.EventType = TEXT("domain_core.broken");
        Event.WorldTick = WorldTick;
        Event.PrimaryEntity = Core.CoreId;
        Event.RelatedEntities.Add(Core.TerritoryId);
        Event.bChronicleEligible = true;

        if (!Store.AppendWorldEvent(
                Event,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            return false;
        }
    }

    if (!Store.CommitTransaction(OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGTerritoryProjectService::CaptureIntactCore(
    const FOGEntityId& CoreId,
    const FOGEntityId& NewControllerRulerId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!NewControllerRulerId.IsValid())
    {
        OutError = TEXT("Core capture requires a valid new controller.");
        return false;
    }

    bool bFound = false;
    FOGDomainCoreRecord Core;

    if (!Store.TryReadDomainCore(
            CoreId,
            bFound,
            Core,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Domain Core does not exist.");
        return false;
    }

    if (Core.Lifecycle == EOGDomainCoreLifecycle::Broken ||
        Core.Lifecycle == EOGDomainCoreLifecycle::Absorbed ||
        Core.CurrentDurability.GetSign() <= 0)
    {
        OutError = TEXT("Broken/destroyed Domain Cores cannot be captured.");
        return false;
    }

    Core.ControllerRulerId =
        NewControllerRulerId;

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    if (!Store.UpsertDomainCore(
            Core,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType = TEXT("domain_core.captured");
    Event.WorldTick = WorldTick;
    Event.PrimaryEntity = Core.CoreId;
    Event.RelatedEntities.Add(Core.TerritoryId);
    Event.RelatedEntities.Add(NewControllerRulerId);
    Event.bChronicleEligible = true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    return true;
}

bool FOGTerritoryProjectService::StartProject(
    const FOGEntityId& OwnerEntityId,
    const FOGEntityId& LocationId,
    const FOGContentId& ProjectTypeId,
    const TArray<FOGProjectResourceCost>& Costs,
    int64 StartWorldTick,
    int64 ResolveWorldTick,
    const FString& PayloadJson,
    FOGEntityId& OutProjectId,
    FString& OutError)
{
    OutProjectId = FOGEntityId();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        !LocationId.IsValid() ||
        !ProjectTypeId.IsValid() ||
        ResolveWorldTick < StartWorldTick)
    {
        OutError = TEXT("Project definition is invalid.");
        return false;
    }

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    for (const FOGProjectResourceCost& Cost : Costs)
    {
        if (!Cost.ResourceId.IsValid() ||
            Cost.Amount < 0)
        {
            OutError = TEXT("Project contains an invalid resource cost.");
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            return false;
        }

        bool bKnown = false;
        int64 Balance = 0;

        if (!Store.TryReadResourceBalance(
                OwnerEntityId,
                Cost.ResourceId,
                bKnown,
                Balance,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            return false;
        }

        if (!bKnown || Balance < Cost.Amount)
        {
            OutError = FString::Printf(
                TEXT("Insufficient resource: %s"),
                *Cost.ResourceId.ToString());
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            return false;
        }

        if (!Store.SetResourceBalance(
                OwnerEntityId,
                Cost.ResourceId,
                Balance - Cost.Amount,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(RollbackError);
            return false;
        }
    }

    FOGProjectRecord Project;
    Project.ProjectId = FOGEntityId::NewId();
    Project.OwnerEntityId = OwnerEntityId;
    Project.LocationId = LocationId;
    Project.ProjectTypeId = ProjectTypeId;
    Project.Status = EOGProjectStatus::Active;
    Project.StartWorldTick = StartWorldTick;
    Project.ResolveWorldTick = ResolveWorldTick;
    Project.ProgressBps =
        ResolveWorldTick == StartWorldTick
            ? 10000
            : 0;
    Project.PayloadJson =
        PayloadJson.IsEmpty()
            ? TEXT("{}")
            : PayloadJson;

    if (Project.ProgressBps == 10000)
    {
        Project.Status = EOGProjectStatus::Completed;
    }

    if (!Store.UpsertProject(
            Project,
            StartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    OutProjectId = Project.ProjectId;
    return true;
}

bool FOGTerritoryProjectService::RefreshProject(
    const FOGEntityId& ProjectId,
    int64 CurrentWorldTick,
    FOGProjectRecord& OutProject,
    FString& OutError)
{
    OutProject = FOGProjectRecord();
    OutError.Reset();

    bool bFound = false;

    if (!Store.TryReadProject(
            ProjectId,
            bFound,
            OutProject,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Project does not exist.");
        return false;
    }

    if (OutProject.Status != EOGProjectStatus::Active)
    {
        return true;
    }

    const int64 Duration =
        OutProject.ResolveWorldTick -
        OutProject.StartWorldTick;

    if (Duration <= 0 ||
        CurrentWorldTick >= OutProject.ResolveWorldTick)
    {
        OutProject.ProgressBps = 10000;
        OutProject.Status = EOGProjectStatus::Completed;
    }
    else if (CurrentWorldTick <= OutProject.StartWorldTick)
    {
        OutProject.ProgressBps = 0;
    }
    else
    {
        const int64 Elapsed =
            CurrentWorldTick -
            OutProject.StartWorldTick;

        const double Fraction =
            static_cast<double>(Elapsed) /
            static_cast<double>(Duration);

        OutProject.ProgressBps =
            FMath::Clamp(
                FMath::FloorToInt(
                    Fraction * 10000.0),
                0,
                10000);
    }

    return Store.UpsertProject(
        OutProject,
        OutProject.StartWorldTick,
        OutError);
}
