#pragma once

#include "CoreMinimal.h"
#include "Gacha/OGGachaDefinitions.h"
#include "Persistence/OGWorldStore.h"

/**
 * Deterministic, persistent acquisition service.
 *
 * Banner content is immutable definition data. Pity/guarantee state and owned
 * Manifestations live in the authoritative world store.
 */
class OFFLINEGAME_API FOGGachaService
{
public:
    explicit FOGGachaService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool Pull(
        const FOGGachaBannerDefinition& Banner,
        const FOGEntityId& RulerId,
        int64 WorldTick,
        int64 Seed,
        FOGGachaPullResult& OutResult,
        FString& OutError);

    static bool ValidateBanner(
        const FOGGachaBannerDefinition& Banner,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
