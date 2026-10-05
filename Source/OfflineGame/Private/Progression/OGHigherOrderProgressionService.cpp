#include "Progression/OGHigherOrderProgressionService.h"

#include "Events/OGWorldEvent.h"

bool FOGHigherOrderProgressionService::CommitValidatedTranscendence(
    const FOGEntityTranscendenceStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    bool bExisting = false;
    FOGEntityTranscendenceStateRecord Existing;
    if (!Store.TryReadEntityTranscendenceState(
            State.EntityId,
            bExisting,
            Existing,
            OutError))
    {
        return false;
    }

    if (bExisting)
    {
        OutError =
            TEXT("Transcendence is an irreversible one-time ordinary progression commitment.");
        return false;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertEntityTranscendenceState(
            State,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("progression.transcended"));
    Event.WorldTick =
        State.BreakthroughWorldTick;
    Event.PrimaryEntity =
        State.EntityId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"grade\":\"%s\"}"),
        *State.GradeId.ToString());

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

    return true;
}

bool FOGHigherOrderProgressionService::UnlockOrEvolveWorldFantasm(
    const FOGManifestationWorldFantasmStateRecord& State,
    FString& OutError)
{
    OutError.Reset();

    bool bManifestationFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            State.ManifestationId,
            bManifestationFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bManifestationFound)
    {
        OutError =
            TEXT("World Fantasm state references an unknown Manifestation.");
        return false;
    }

    bool bExisting = false;
    FOGManifestationWorldFantasmStateRecord Existing;
    if (!Store.TryReadManifestationWorldFantasmState(
            State.ManifestationId,
            bExisting,
            Existing,
            OutError))
    {
        return false;
    }

    if (!bExisting &&
        Manifestation.CurrentRarity !=
            FName(TEXT("MR")))
    {
        OutError =
            TEXT("World Fantasm first unlock requires Current Rarity MR.");
        return false;
    }

    // The validated grade comes from immutable Character Identity Origin Rarity
    // through the content registry. It is persisted here and cannot later be
    // silently replaced by an evolution.
    return Store.UpsertManifestationWorldFantasmState(
        State,
        OutError);
}

bool FOGHigherOrderProgressionService::UnlockOrEvolveProtagonistWorldManifestation(
    const FOGProtagonistWorldManifestationStateRecord& State,
    FString& OutError)
{
    // No collectible rarity or hidden pseudo-rarity is consulted. Provenance
    // describes the emergent personal-history synthesis.
    return Store.UpsertProtagonistWorldManifestationState(
        State,
        OutError);
}
