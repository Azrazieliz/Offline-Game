#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Class recognition is downstream of actual mastery supplied by validated
 * capability/content logic. Multiple Classes and multiple Crown bearers may
 * coexist. Grand remains a unique metaphysical seat per canonical Class.
 */
class OFFLINEGAME_API FOGClassRecognitionService
{
public:
    explicit FOGClassRecognitionService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool RecognizeClass(
        const FOGEntityClassRecord& ClassState,
        FString& OutError);

    bool AppointGrandSeat(
        const FOGContentId& ClassId,
        const FOGEntityId& BearerEntityId,
        int64 WorldTick,
        const FString& MandateStateJson,
        FString& OutError);

    bool VacateGrandSeat(
        const FOGContentId& ClassId,
        int64 WorldTick,
        FName Reason,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
