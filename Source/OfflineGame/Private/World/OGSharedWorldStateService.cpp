#include "World/OGSharedWorldStateService.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FName FOGSharedWorldStateService::LocationKnowledgeFactKey()
{
    static const FName Key(TEXT("world.location_knowledge"));
    return Key;
}

bool FOGSharedWorldStateService::RecordLocationDiscovery(
    const FOGEntityId& KnowledgeOwnerId,
    const FOGEntityId& LocationId,
    EOGLocationKnowledgeLevel Level,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!KnowledgeOwnerId.IsValid() ||
        !LocationId.IsValid())
    {
        OutError = TEXT("Location discovery requires valid owner and location IDs.");
        return false;
    }

    bool bExisting = false;
    FOGKnowledgeFactRecord Existing;
    if (!Store.TryReadKnowledgeFact(
            KnowledgeOwnerId,
            LocationKnowledgeFactKey(),
            LocationId,
            bExisting,
            Existing,
            OutError))
    {
        return false;
    }

    EOGLocationKnowledgeLevel EffectiveLevel = Level;

    if (bExisting)
    {
        TSharedPtr<FJsonObject> ExistingJson;
        const TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(
                Existing.ValueJson);

        if (FJsonSerializer::Deserialize(
                Reader,
                ExistingJson) &&
            ExistingJson.IsValid())
        {
            double ExistingLevelNumber = 0.0;
            if (ExistingJson->TryGetNumberField(
                    TEXT("level"),
                    ExistingLevelNumber))
            {
                const int32 ExistingLevel =
                    FMath::RoundToInt(
                        ExistingLevelNumber);

                EffectiveLevel =
                    static_cast<EOGLocationKnowledgeLevel>(
                        FMath::Max(
                            ExistingLevel,
                            static_cast<int32>(Level)));
            }
        }
    }

    TSharedRef<FJsonObject> Json =
        MakeShared<FJsonObject>();
    Json->SetNumberField(
        TEXT("level"),
        static_cast<int32>(EffectiveLevel));

    FString ValueJson;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(
            &ValueJson);
    FJsonSerializer::Serialize(
        Json,
        Writer);

    FOGKnowledgeFactRecord Fact;
    Fact.OwnerEntityId = KnowledgeOwnerId;
    Fact.FactKey =
        LocationKnowledgeFactKey();
    Fact.SubjectEntityId =
        LocationId;
    Fact.ValueJson =
        MoveTemp(ValueJson);
    Fact.LearnedWorldTick =
        bExisting
            ? Existing.LearnedWorldTick
            : WorldTick;
    Fact.UpdatedWorldTick =
        WorldTick;

    return Store.UpsertKnowledgeFact(
        Fact,
        OutError);
}

bool FOGSharedWorldStateService::TryReadLocationKnowledge(
    const FOGEntityId& KnowledgeOwnerId,
    const FOGEntityId& LocationId,
    bool& bOutKnown,
    EOGLocationKnowledgeLevel& OutLevel,
    FString& OutError) const
{
    bOutKnown = false;
    OutLevel =
        EOGLocationKnowledgeLevel::Rumored;
    OutError.Reset();

    FOGKnowledgeFactRecord Fact;
    if (!Store.TryReadKnowledgeFact(
            KnowledgeOwnerId,
            LocationKnowledgeFactKey(),
            LocationId,
            bOutKnown,
            Fact,
            OutError))
    {
        return false;
    }

    if (!bOutKnown)
    {
        return true;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(
            Fact.ValueJson);

    if (!FJsonSerializer::Deserialize(
            Reader,
            Json) ||
        !Json.IsValid())
    {
        OutError = TEXT("Location knowledge JSON is invalid.");
        return false;
    }

    double LevelNumber = 0.0;
    if (!Json->TryGetNumberField(
            TEXT("level"),
            LevelNumber))
    {
        OutError = TEXT("Location knowledge level is missing.");
        return false;
    }

    const int32 Level =
        FMath::RoundToInt(
            LevelNumber);

    if (Level <
            static_cast<int32>(
                EOGLocationKnowledgeLevel::Rumored) ||
        Level >
            static_cast<int32>(
                EOGLocationKnowledgeLevel::Explored))
    {
        OutError = TEXT("Location knowledge level is invalid.");
        return false;
    }

    OutLevel =
        static_cast<EOGLocationKnowledgeLevel>(
            Level);
    return true;
}

bool FOGSharedWorldStateService::UpdatePhysicalPresence(
    const FOGWorldPresenceRecord& Presence,
    FString& OutError)
{
    return Store.UpsertWorldPresence(
        Presence,
        OutError);
}

bool FOGSharedWorldStateService::TryReadPhysicalPresence(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGWorldPresenceRecord& OutPresence,
    FString& OutError) const
{
    return Store.TryReadWorldPresence(
        EntityId,
        bOutFound,
        OutPresence,
        OutError);
}
