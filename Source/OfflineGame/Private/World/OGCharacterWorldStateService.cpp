#include "World/OGCharacterWorldStateService.h"

bool FOGCharacterWorldStateService::RecordKnowledge(
    const FOGKnowledgeFactRecord& Fact,
    FString& OutError)
{
    return Store.UpsertKnowledgeFact(
        Fact,
        OutError);
}

bool FOGCharacterWorldStateService::RecordLanguage(
    const FOGEntityLanguageRecord& Language,
    FString& OutError)
{
    return Store.UpsertEntityLanguage(
        Language,
        OutError);
}

bool FOGCharacterWorldStateService::RecordSemanticMemory(
    const FOGSemanticMemoryRecord& Memory,
    int64 WorldTick,
    FString& OutError)
{
    return Store.UpsertSemanticMemory(
        Memory,
        WorldTick,
        OutError);
}

bool FOGCharacterWorldStateService::PromoteNpc(
    const FOGEntityId& ExistingEntityId,
    const FOGContentId& SimulationTierId,
    int64 WorldTick,
    const FOGEntityId& ReasonEventId,
    const FString& PresentationPackageStateJson,
    FString& OutError)
{
    OutError.Reset();

    if (!ExistingEntityId.IsValid() ||
        !SimulationTierId.IsValid() ||
        WorldTick < 0)
    {
        OutError = TEXT("NPC promotion request is invalid.");
        return false;
    }

    bool bFound = false;
    FName ExistingKind = NAME_None;
    FString ExistingState;
    int64 Revision = 0;
    if (!Store.TryReadEntity(
            ExistingEntityId,
            bFound,
            ExistingKind,
            ExistingState,
            Revision,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("NPC promotion requires an existing world entity.");
        return false;
    }

    FOGNpcPromotionStateRecord Promotion;
    Promotion.EntityId = ExistingEntityId;
    Promotion.SimulationTierId = SimulationTierId;
    Promotion.PromotedWorldTick = WorldTick;
    Promotion.ReasonEventId = ReasonEventId;
    Promotion.PresentationPackageStateJson =
        PresentationPackageStateJson.IsEmpty()
            ? TEXT("{}")
            : PresentationPackageStateJson;

    // Deliberately writes promotion metadata only: entity ID/history remain the
    // same canonical object.
    return Store.UpsertNpcPromotionState(
        Promotion,
        OutError);
}

bool FOGCharacterWorldStateService::SetAdultRuntimeContext(
    const FOGCharacterAdultRuntimeStateRecord& State,
    FString& OutError)
{
    // Canonical maturity is content Identity lore and is intentionally absent
    // from this mutable runtime-state contract.
    return Store.UpsertCharacterAdultRuntimeState(
        State,
        OutError);
}

bool FOGCharacterWorldStateService::CreateHeroicRecord(
    const FOGEntityId& SourceWorldEntityId,
    const FOGContentId& IdentityId,
    const FOGEntityId& DeathEventId,
    const FOGContentId& PatternId,
    int64 WorldTick,
    FOGEntityId& OutRecordId,
    FString& OutError)
{
    OutRecordId = FOGEntityId();
    OutError.Reset();

    if (!SourceWorldEntityId.IsValid() ||
        !IdentityId.IsValid() ||
        !DeathEventId.IsValid() ||
        (!PatternId.IsEmpty() && !PatternId.IsValid()) ||
        WorldTick < 0)
    {
        OutError = TEXT("Heroic Record creation request is invalid.");
        return false;
    }

    FOGHeroicRecord Record;
    Record.RecordId = FOGEntityId::NewId();
    Record.SourceWorldEntityId = SourceWorldEntityId;
    Record.IdentityId = IdentityId;
    Record.DeathEventId = DeathEventId;
    Record.CreatedWorldTick = WorldTick;
    Record.PatternId = PatternId;
    Record.GachaAccessState = FName(TEXT("locked"));

    if (!Store.UpsertHeroicRecord(
            Record,
            WorldTick,
            OutError))
    {
        return false;
    }

    OutRecordId = Record.RecordId;
    return true;
}
