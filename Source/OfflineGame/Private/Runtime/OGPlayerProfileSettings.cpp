#include "Runtime/OGPlayerProfileSettings.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString JsonOrDefault(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    const TCHAR* DefaultValue)
{
    FString Value;
    return Root.IsValid() &&
           Root->TryGetStringField(Field, Value)
        ? Value
        : FString(DefaultValue);
}

bool BoolOrDefault(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    bool DefaultValue)
{
    bool Value = false;
    return Root.IsValid() &&
           Root->TryGetBoolField(Field, Value)
        ? Value
        : DefaultValue;
}

double NumberOrDefault(
    const TSharedPtr<FJsonObject>& Root,
    const TCHAR* Field,
    double DefaultValue)
{
    double Value = 0.0;
    return Root.IsValid() &&
           Root->TryGetNumberField(Field, Value)
        ? Value
        : DefaultValue;
}
}

bool FOGPlayerProfileSettingsService::Load(
    const FString& ProfilePath,
    FOGPlayerProfileSettings& OutSettings,
    FString& OutError)
{
    OutSettings =
        FOGPlayerProfileSettings();
    OutError.Reset();

    if (ProfilePath.IsEmpty())
    {
        OutError =
            TEXT("Player profile path is empty.");
        return false;
    }

    if (!IFileManager::Get().FileExists(
            *ProfilePath))
    {
        // Missing profile is a valid first-run state; defaults include automatic
        // orientation and do not touch canonical world causality.
        return true;
    }

    FString Json;
    if (!FFileHelper::LoadFileToString(
            Json,
            *ProfilePath))
    {
        OutError =
            TEXT("Failed to read player profile sidecar.");
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(
            Reader,
            Root) ||
        !Root.IsValid())
    {
        OutError =
            TEXT("Player profile sidecar is not valid JSON.");
        return false;
    }

    OutSettings.Version =
        FMath::Max(
            1,
            static_cast<int32>(
                NumberOrDefault(
                    Root,
                    TEXT("version"),
                    1.0)));
    OutSettings.bSfwPresentation =
        BoolOrDefault(
            Root,
            TEXT("sfw_presentation"),
            false);
    OutSettings.OrientationLock =
        FName(*JsonOrDefault(
            Root,
            TEXT("orientation_lock"),
            TEXT("automatic")));
    OutSettings.RosterDensity =
        FName(*JsonOrDefault(
            Root,
            TEXT("roster_density"),
            TEXT("dense")));

    OutSettings.MasterVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("master_volume"),
                    1.0)),
            0.0f,
            1.0f);
    OutSettings.MusicVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("music_volume"),
                    1.0)),
            0.0f,
            1.0f);
    OutSettings.EffectsVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("effects_volume"),
                    1.0)),
            0.0f,
            1.0f);
    OutSettings.DynamicRangeProfile =
        FName(*JsonOrDefault(
            Root,
            TEXT("dynamic_range"),
            TEXT("full")));

    OutSettings.bHapticsEnabled =
        BoolOrDefault(
            Root,
            TEXT("haptics_enabled"),
            true);
    OutSettings.ControlProfileJson =
        JsonOrDefault(
            Root,
            TEXT("control_profile_json"),
            TEXT("{}"));

    OutSettings.bReducedMotion =
        BoolOrDefault(
            Root,
            TEXT("reduced_motion"),
            false);
    OutSettings.bReducedCameraShake =
        BoolOrDefault(
            Root,
            TEXT("reduced_camera_shake"),
            false);
    OutSettings.DamageNumberPresentation =
        FName(*JsonOrDefault(
            Root,
            TEXT("damage_number_presentation"),
            TEXT("standard")));
    OutSettings.CinematicPreferencesJson =
        JsonOrDefault(
            Root,
            TEXT("cinematic_preferences_json"),
            TEXT("{}"));

    OutSettings.bAutoDownload =
        BoolOrDefault(
            Root,
            TEXT("auto_download"),
            true);
    OutSettings.NetworkPreferencesJson =
        JsonOrDefault(
            Root,
            TEXT("network_preferences_json"),
            TEXT("{\"large_downloads\":\"unmetered_only\"}"));
    OutSettings.AccessibilityJson =
        JsonOrDefault(
            Root,
            TEXT("accessibility_json"),
            TEXT("{}"));
    OutSettings.AutoCombatPresetJson =
        JsonOrDefault(
            Root,
            TEXT("auto_combat_preset_json"),
            TEXT("{}"));

    if (OutSettings.OrientationLock.IsNone())
    {
        OutSettings.OrientationLock =
            FName(TEXT("automatic"));
    }

    return true;
}

bool FOGPlayerProfileSettingsService::Save(
    const FString& ProfilePath,
    const FOGPlayerProfileSettings& Settings,
    FString& OutError)
{
    OutError.Reset();

    if (ProfilePath.IsEmpty() ||
        Settings.Version < 1)
    {
        OutError =
            TEXT("Player profile save request is invalid.");
        return false;
    }

    const FString Directory =
        FPaths::GetPath(ProfilePath);
    if (!Directory.IsEmpty() &&
        !IFileManager::Get().MakeDirectory(
            *Directory,
            true) &&
        !IFileManager::Get().DirectoryExists(
            *Directory))
    {
        OutError =
            TEXT("Failed to create player profile directory.");
        return false;
    }

    TSharedRef<FJsonObject> Root =
        MakeShared<FJsonObject>();
    Root->SetNumberField(
        TEXT("version"),
        Settings.Version);
    Root->SetBoolField(
        TEXT("sfw_presentation"),
        Settings.bSfwPresentation);
    Root->SetStringField(
        TEXT("orientation_lock"),
        Settings.OrientationLock.IsNone()
            ? TEXT("automatic")
            : Settings.OrientationLock.ToString());
    Root->SetStringField(
        TEXT("roster_density"),
        Settings.RosterDensity.IsNone()
            ? TEXT("comfortable")
            : Settings.RosterDensity.ToString());
    Root->SetNumberField(
        TEXT("master_volume"),
        FMath::Clamp(
            Settings.MasterVolume,
            0.0f,
            1.0f));
    Root->SetNumberField(
        TEXT("music_volume"),
        FMath::Clamp(
            Settings.MusicVolume,
            0.0f,
            1.0f));
    Root->SetNumberField(
        TEXT("effects_volume"),
        FMath::Clamp(
            Settings.EffectsVolume,
            0.0f,
            1.0f));
    Root->SetStringField(
        TEXT("dynamic_range"),
        Settings.DynamicRangeProfile.ToString());
    Root->SetBoolField(
        TEXT("haptics_enabled"),
        Settings.bHapticsEnabled);
    Root->SetStringField(
        TEXT("control_profile_json"),
        Settings.ControlProfileJson);
    Root->SetBoolField(
        TEXT("reduced_motion"),
        Settings.bReducedMotion);
    Root->SetBoolField(
        TEXT("reduced_camera_shake"),
        Settings.bReducedCameraShake);
    Root->SetStringField(
        TEXT("damage_number_presentation"),
        Settings.DamageNumberPresentation.ToString());
    Root->SetStringField(
        TEXT("cinematic_preferences_json"),
        Settings.CinematicPreferencesJson);
    Root->SetBoolField(
        TEXT("auto_download"),
        Settings.bAutoDownload);
    Root->SetStringField(
        TEXT("network_preferences_json"),
        Settings.NetworkPreferencesJson);
    Root->SetStringField(
        TEXT("accessibility_json"),
        Settings.AccessibilityJson);
    Root->SetStringField(
        TEXT("auto_combat_preset_json"),
        Settings.AutoCombatPresetJson);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(
            Root,
            Writer))
    {
        OutError =
            TEXT("Failed to serialize player profile sidecar.");
        return false;
    }

    if (!FFileHelper::SaveStringToFile(
            Json,
            *ProfilePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError =
            TEXT("Failed to write player profile sidecar.");
        return false;
    }

    return true;
}

FName FOGPlayerProfileSettingsService::ResolveOrientation(
    const FOGPlayerProfileSettings& Settings,
    FName CurrentDeviceOrientation)
{
    if (Settings.OrientationLock.IsNone() ||
        Settings.OrientationLock ==
            FName(TEXT("automatic")))
    {
        return CurrentDeviceOrientation.IsNone()
            ? FName(TEXT("portrait"))
            : CurrentDeviceOrientation;
    }

    return Settings.OrientationLock;
}
