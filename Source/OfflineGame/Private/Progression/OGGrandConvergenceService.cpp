#include "Progression/OGGrandConvergenceService.h"

#include "Events/OGWorldEvent.h"

bool FOGGrandConvergenceService::PerformConvergence(
    FOGCharacterManifestationRecord ResultManifestation,
    const TArray<FOGEntityId>& SourceManifestationIds,
    const TArray<FOGContentId>& SourceLineageIds,
    const FOGContentId& RuleId,
    int64 WorldTick,
    const FString& StateJson,
    FOGEntityId& OutConvergenceId,
    FString& OutError)
{
    return PerformConvergence(
        MoveTemp(ResultManifestation),
        SourceManifestationIds,
        SourceLineageIds,
        RuleId,
        WorldTick,
        StateJson,
        false,
        OutConvergenceId,
        OutError);
}

bool FOGGrandConvergenceService::PerformConvergence(
    FOGCharacterManifestationRecord ResultManifestation,
    const TArray<FOGEntityId>& SourceManifestationIds,
    const TArray<FOGContentId>& SourceLineageIds,
    const FOGContentId& RuleId,
    int64 WorldTick,
    const FString& StateJson,
    bool bExplicitlyConfirmProtectedSources,
    FOGEntityId& OutConvergenceId,
    FString& OutError)
{
    OutConvergenceId =
        FOGEntityId();
    OutError.Reset();

    if (!ResultManifestation.ManifestationId.IsValid() ||
        !ResultManifestation.OwningRulerId.IsValid() ||
        !ResultManifestation.IdentityId.IsValid() ||
        !ResultManifestation.ActiveVersionId.IsValid() ||
        !RuleId.IsValid() ||
        WorldTick < 0 ||
        SourceManifestationIds.IsEmpty() ||
        (!SourceLineageIds.IsEmpty() &&
         SourceLineageIds.Num() !=
             SourceManifestationIds.Num()))
    {
        OutError =
            TEXT("Grand Convergence request is invalid.");
        return false;
    }

    TSet<FOGEntityId> UniqueSources;
    TArray<FOGCharacterManifestationRecord> Sources;
    Sources.Reserve(
        SourceManifestationIds.Num());

    for (const FOGEntityId& SourceId :
         SourceManifestationIds)
    {
        if (!SourceId.IsValid() ||
            SourceId ==
                ResultManifestation.ManifestationId ||
            UniqueSources.Contains(SourceId))
        {
            OutError =
                TEXT("Grand Convergence contains an invalid or duplicate source Manifestation.");
            return false;
        }

        UniqueSources.Add(SourceId);

        bool bFound = false;
        FOGCharacterManifestationRecord Source;
        if (!Store.TryReadCharacterManifestation(
                SourceId,
                bFound,
                Source,
                OutError))
        {
            return false;
        }

        if (!bFound)
        {
            OutError =
                TEXT("Grand Convergence source Manifestation does not exist.");
            return false;
        }

        if (Source.OwningRulerId !=
                ResultManifestation.OwningRulerId ||
            Source.IdentityId !=
                ResultManifestation.IdentityId)
        {
            OutError =
                TEXT("Grand Convergence sources must share one owner and Character Identity.");
            return false;
        }

        if (Source.LifecycleState ==
            FName(TEXT("converged")))
        {
            OutError =
                TEXT("A previously converged source cannot be consumed again as a separate Manifestation.");
            return false;
        }

        bool bManagementFound = false;
        FOGManifestationManagementMetadataRecord Management;
        if (!Store.TryReadManifestationManagementMetadata(
                SourceId,
                bManagementFound,
                Management,
                OutError))
        {
            return false;
        }

        if (bManagementFound &&
            (Management.bProtected ||
             Management.bLocked) &&
            !bExplicitlyConfirmProtectedSources)
        {
            OutError =
                TEXT("Grand Convergence source is Protected/Locked and requires explicit confirmation.");
            return false;
        }

        bool bReinforcementFound = false;
        FOGManifestationReinforcementRecord Reinforcement;
        if (!Store.TryReadManifestationReinforcement(
                SourceId,
                bReinforcementFound,
                Reinforcement,
                OutError))
        {
            return false;
        }

        if (!bReinforcementFound ||
            !Reinforcement.bMaxReinforced)
        {
            OutError =
                TEXT("Grand Convergence requires every selected source to be max reinforced.");
            return false;
        }

        Sources.Add(MoveTemp(Source));
    }

    bool bResultFound = false;
    FOGCharacterManifestationRecord ExistingResult;
    if (!Store.TryReadCharacterManifestation(
            ResultManifestation.ManifestationId,
            bResultFound,
            ExistingResult,
            OutError))
    {
        return false;
    }

    if (bResultFound)
    {
        if (ExistingResult.OwningRulerId !=
                ResultManifestation.OwningRulerId ||
            ExistingResult.IdentityId !=
                ResultManifestation.IdentityId ||
            ExistingResult.LifecycleState ==
                FName(TEXT("converged")))
        {
            OutError =
                TEXT("Existing Grand result is incompatible with this Convergence.");
            return false;
        }
    }

    ResultManifestation.LifecycleState =
        FName(TEXT("grand"));

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertCharacterManifestation(
            ResultManifestation,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!bResultFound)
    {
        FOGManifestationReinforcementRecord ResultReinforcement;
        ResultReinforcement.ManifestationId =
            ResultManifestation.ManifestationId;
        ResultReinforcement.ReinforcementState =
            FName(TEXT("grand_result_unassessed"));
        ResultReinforcement.bMaxReinforced =
            false;
        ResultReinforcement.UpdatedWorldTick =
            WorldTick;
        ResultReinforcement.StateJson =
            TEXT("{\"source\":\"grand_convergence\"}");

        if (!Store.UpsertManifestationReinforcement(
                ResultReinforcement,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGCharacterConvergenceRecord Convergence;
    Convergence.ConvergenceId =
        FOGEntityId::NewId();
    Convergence.IdentityId =
        ResultManifestation.IdentityId;
    Convergence.ResultManifestationId =
        ResultManifestation.ManifestationId;
    Convergence.RuleId =
        RuleId;
    Convergence.WorldTick =
        WorldTick;
    Convergence.StateJson =
        StateJson.IsEmpty()
            ? TEXT("{}")
            : StateJson;

    if (!Store.UpsertCharacterConvergence(
            Convergence,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    for (int32 Index = 0;
         Index < Sources.Num();
         ++Index)
    {
        const FOGCharacterManifestationRecord& Source =
            Sources[Index];

        FOGCharacterConvergenceSourceRecord SourceRecord;
        SourceRecord.ConvergenceId =
            Convergence.ConvergenceId;
        SourceRecord.SourceManifestationId =
            Source.ManifestationId;
        SourceRecord.Ordinal =
            Index;

        if (SourceLineageIds.IsValidIndex(Index))
        {
            SourceRecord.LineageId =
                SourceLineageIds[Index];
        }

        if (!Store.UpsertCharacterConvergenceSource(
                SourceRecord,
                OutError) ||
            !Store.SetManifestationLifecycle(
                Source.ManifestationId,
                FName(TEXT("converged")),
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("character.grand_convergence"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        ResultManifestation.ManifestationId;
    for (const FOGEntityId& SourceId :
         SourceManifestationIds)
    {
        Event.RelatedEntities.Add(
            SourceId);
    }
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"identity\":\"%s\",\"rule\":\"%s\",\"source_count\":%d}"),
        *ResultManifestation.IdentityId.ToString(),
        *RuleId.ToString(),
        SourceManifestationIds.Num());

    if (!Store.AppendWorldEvent(
            Event,
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

    OutConvergenceId =
        Convergence.ConvergenceId;
    return true;
}
