#include "Diagnostics/OGDiagnosticsBundle.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FOGDiagnosticsBundle::Write(
    IOGWorldStore& Store,
    const FString& OutputDirectory,
    FString& OutBundlePath,
    FString& OutError)
{
    OutBundlePath.Reset();
    OutError.Reset();

    if (!Store.IsOpen())
    {
        OutError = TEXT("Cannot generate diagnostics while the world store is closed.");
        return false;
    }

    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*OutputDirectory, true) &&
        !Files.DirectoryExists(*OutputDirectory))
    {
        OutError = FString::Printf(
            TEXT("Failed to create diagnostics directory: %s"),
            *OutputDirectory);
        return false;
    }

    FString SchemaError;
    const int32 SchemaVersion =
        Store.GetSchemaVersion(SchemaError);

    FString IntegrityReport;
    FString IntegrityError;
    const bool bIntegrityOk =
        Store.RunIntegrityCheck(
            IntegrityReport,
            IntegrityError);

    const FString DatabasePath =
        Store.GetDatabasePath();
    const int64 DatabaseBytes =
        Files.FileSize(*DatabasePath);

    TSharedRef<FJsonObject> Root =
        MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("generated_utc"),
        FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(
        TEXT("engine_version"),
        FEngineVersion::Current().ToString());
    Root->SetStringField(
        TEXT("os_version"),
        FPlatformMisc::GetOSVersion());
    Root->SetNumberField(
        TEXT("schema_version"),
        SchemaVersion);
    Root->SetStringField(
        TEXT("schema_error"),
        SchemaError);
    Root->SetBoolField(
        TEXT("integrity_ok"),
        bIntegrityOk);
    Root->SetStringField(
        TEXT("integrity_report"),
        IntegrityReport);
    Root->SetStringField(
        TEXT("integrity_error"),
        IntegrityError);
    Root->SetStringField(
        TEXT("database_file"),
        FPaths::GetCleanFilename(DatabasePath));
    Root->SetNumberField(
        TEXT("database_bytes"),
        static_cast<double>(DatabaseBytes));

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Json);

    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("Failed to serialize diagnostics JSON.");
        return false;
    }

    const FString Timestamp =
        FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"));
    OutBundlePath = FPaths::Combine(
        OutputDirectory,
        FString::Printf(
            TEXT("diagnostics_%s.json"),
            *Timestamp));

    if (!FFileHelper::SaveStringToFile(Json, *OutBundlePath))
    {
        OutError = FString::Printf(
            TEXT("Failed to write diagnostics bundle: %s"),
            *OutBundlePath);
        OutBundlePath.Reset();
        return false;
    }

    return true;
}
