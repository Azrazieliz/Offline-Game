#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPerformanceTelemetry.h"

/** Writes a compact, shareable diagnostic snapshot without exposing save contents. */
class OFFLINEGAME_API FOGDiagnosticsBundle
{
public:
    static bool Write(
        IOGWorldStore& Store,
        const FString& OutputDirectory,
        FString& OutBundlePath,
        FString& OutError);

    static bool Write(
        IOGWorldStore& Store,
        const FString& OutputDirectory,
        const FOGPerformanceTelemetrySnapshot& Telemetry,
        FString& OutBundlePath,
        FString& OutError);

private:
    static bool WriteInternal(
        IOGWorldStore& Store,
        const FString& OutputDirectory,
        const FOGPerformanceTelemetrySnapshot* Telemetry,
        FString& OutBundlePath,
        FString& OutError);
};
