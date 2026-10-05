#include "World/OGProjectService.h"

bool FOGProjectService::StartProject(
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
        OutError =
            TEXT("Project definition is invalid.");
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
            OutError =
                TEXT("Project contains an invalid resource cost.");
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
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
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (!bKnown ||
            Balance < Cost.Amount)
        {
            OutError = FString::Printf(
                TEXT("Insufficient resource: %s"),
                *Cost.ResourceId.ToString());
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (!Store.SetResourceBalance(
                OwnerEntityId,
                Cost.ResourceId,
                Balance - Cost.Amount,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGProjectRecord Project;
    Project.ProjectId =
        FOGEntityId::NewId();
    Project.OwnerEntityId =
        OwnerEntityId;
    Project.LocationId =
        LocationId;
    Project.ProjectTypeId =
        ProjectTypeId;
    Project.Status =
        EOGProjectStatus::Active;
    Project.StartWorldTick =
        StartWorldTick;
    Project.ResolveWorldTick =
        ResolveWorldTick;
    Project.ProgressBps =
        ResolveWorldTick ==
            StartWorldTick
            ? 10000
            : 0;
    Project.PayloadJson =
        PayloadJson.IsEmpty()
            ? TEXT("{}")
            : PayloadJson;

    if (Project.ProgressBps == 10000)
    {
        Project.Status =
            EOGProjectStatus::Completed;
    }

    if (!Store.UpsertProject(
            Project,
            StartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    OutProjectId =
        Project.ProjectId;
    return true;
}

bool FOGProjectService::RefreshProject(
    const FOGEntityId& ProjectId,
    int64 CurrentWorldTick,
    FOGProjectRecord& OutProject,
    FString& OutError)
{
    OutProject =
        FOGProjectRecord();
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
        OutError =
            TEXT("Project does not exist.");
        return false;
    }

    if (OutProject.Status !=
        EOGProjectStatus::Active)
    {
        return true;
    }

    const int64 Duration =
        OutProject.ResolveWorldTick -
        OutProject.StartWorldTick;

    if (Duration <= 0 ||
        CurrentWorldTick >=
            OutProject.ResolveWorldTick)
    {
        OutProject.ProgressBps =
            10000;
        OutProject.Status =
            EOGProjectStatus::Completed;
    }
    else if (CurrentWorldTick <=
             OutProject.StartWorldTick)
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
