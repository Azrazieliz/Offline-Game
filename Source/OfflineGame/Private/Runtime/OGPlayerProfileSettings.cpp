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
                    4.0)));
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
    OutSettings.VoiceVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("voice_volume"),
                    1.0)),
            0.0f,
            1.0f);
    OutSettings.SfxVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("sfx_volume"),
                    OutSettings.EffectsVolume)),
            0.0f,
            1.0f);
    OutSettings.AmbienceVolume =
        FMath::Clamp(
            static_cast<float>(
                NumberOrDefault(
                    Root,
                    TEXT("ambience_volume"),
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
    OutSettings.HapticsIntensity =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("haptics_intensity"),
                0.80)),
            0.0f,
            1.0f);
    OutSettings.CameraHorizontalSensitivity =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("camera_horizontal_sensitivity"),
                1.10)),
            0.35f,
            2.50f);
    OutSettings.CameraVerticalSensitivity =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("camera_vertical_sensitivity"),
                0.82)),
            0.30f,
            2.00f);
    OutSettings.CameraResponseExponent =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("camera_response_exponent"),
                1.55)),
            1.0f,
            2.5f);
    OutSettings.bInvertCameraX =
        BoolOrDefault(
            Root,
            TEXT("invert_camera_x"),
            false);
    OutSettings.bInvertCameraY =
        BoolOrDefault(
            Root,
            TEXT("invert_camera_y"),
            false);
    OutSettings.bSprintToggle =
        BoolOrDefault(
            Root,
            TEXT("sprint_toggle"),
            false);
    OutSettings.bLeftHandedControls =
        BoolOrDefault(
            Root,
            TEXT("left_handed_controls"),
            false);
    OutSettings.TouchControlScale =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("touch_control_scale"),
                1.0)),
            0.75f,
            1.40f);
    OutSettings.TouchControlOpacity =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("touch_control_opacity"),
                0.74)),
            0.30f,
            1.0f);
    OutSettings.MovementDeadzone =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("movement_deadzone"),
                0.08)),
            0.0f,
            0.35f);
    OutSettings.LookDeadzone =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("look_deadzone"),
                0.03)),
            0.0f,
            0.25f);
    OutSettings.MovementStickInset =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("movement_stick_inset"),
                0.105)),
            0.06f,
            0.34f);
    OutSettings.MovementStickBottom =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("movement_stick_bottom"),
                0.14)),
            0.08f,
            0.36f);
    OutSettings.ActionClusterHorizontalOffset =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("action_cluster_horizontal_offset"),
                0.0)),
            -0.08f,
            0.12f);
    OutSettings.ActionClusterVerticalOffset =
        FMath::Clamp(
            static_cast<float>(NumberOrDefault(
                Root,
                TEXT("action_cluster_vertical_offset"),
                0.0)),
            -0.12f,
            0.12f);
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
    OutSettings.bSubtitlesEnabled =
        BoolOrDefault(
            Root,
            TEXT("subtitles_enabled"),
            true);
    OutSettings.SubtitlePresentation =
        FName(*JsonOrDefault(
            Root,
            TEXT("subtitle_presentation"),
            TEXT("standard")));
    OutSettings.UiReadabilityProfile =
        FName(*JsonOrDefault(
            Root,
            TEXT("ui_readability_profile"),
            TEXT("standard")));
    OutSettings.ColorVisionProfile =
        FName(*JsonOrDefault(
            Root,
            TEXT("color_vision_profile"),
            TEXT("standard")));
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
    Root->SetNumberField(
        TEXT("voice_volume"),
        FMath::Clamp(Settings.VoiceVolume, 0.0f, 1.0f));
    Root->SetNumberField(
        TEXT("sfx_volume"),
        FMath::Clamp(Settings.SfxVolume, 0.0f, 1.0f));
    Root->SetNumberField(
        TEXT("ambience_volume"),
        FMath::Clamp(Settings.AmbienceVolume, 0.0f, 1.0f));
    Root->SetStringField(
        TEXT("dynamic_range"),
        Settings.DynamicRangeProfile.ToString());
    Root->SetBoolField(
        TEXT("haptics_enabled"),
        Settings.bHapticsEnabled);
    Root->SetNumberField(
        TEXT("haptics_intensity"),
        FMath::Clamp(Settings.HapticsIntensity, 0.0f, 1.0f));
    Root->SetNumberField(
        TEXT("camera_horizontal_sensitivity"),
        FMath::Clamp(Settings.CameraHorizontalSensitivity, 0.35f, 2.50f));
    Root->SetNumberField(
        TEXT("camera_vertical_sensitivity"),
        FMath::Clamp(Settings.CameraVerticalSensitivity, 0.30f, 2.00f));
    Root->SetNumberField(
        TEXT("camera_response_exponent"),
        FMath::Clamp(Settings.CameraResponseExponent, 1.0f, 2.5f));
    Root->SetBoolField(
        TEXT("invert_camera_x"),
        Settings.bInvertCameraX);
    Root->SetBoolField(
        TEXT("invert_camera_y"),
        Settings.bInvertCameraY);
    Root->SetBoolField(
        TEXT("sprint_toggle"),
        Settings.bSprintToggle);
    Root->SetBoolField(
        TEXT("left_handed_controls"),
        Settings.bLeftHandedControls);
    Root->SetNumberField(
        TEXT("touch_control_scale"),
        FMath::Clamp(Settings.TouchControlScale, 0.75f, 1.40f));
    Root->SetNumberField(
        TEXT("touch_control_opacity"),
        FMath::Clamp(Settings.TouchControlOpacity, 0.30f, 1.0f));
    Root->SetNumberField(
        TEXT("movement_deadzone"),
        FMath::Clamp(Settings.MovementDeadzone, 0.0f, 0.35f));
    Root->SetNumberField(
        TEXT("look_deadzone"),
        FMath::Clamp(Settings.LookDeadzone, 0.0f, 0.25f));
    Root->SetNumberField(
        TEXT("movement_stick_inset"),
        FMath::Clamp(Settings.MovementStickInset, 0.06f, 0.34f));
    Root->SetNumberField(
        TEXT("movement_stick_bottom"),
        FMath::Clamp(Settings.MovementStickBottom, 0.08f, 0.36f));
    Root->SetNumberField(
        TEXT("action_cluster_horizontal_offset"),
        FMath::Clamp(Settings.ActionClusterHorizontalOffset, -0.08f, 0.12f));
    Root->SetNumberField(
        TEXT("action_cluster_vertical_offset"),
        FMath::Clamp(Settings.ActionClusterVerticalOffset, -0.12f, 0.12f));
    Root->SetStringField(
        TEXT("control_profile_json"),
        Settings.ControlProfileJson);
    Root->SetBoolField(
        TEXT("reduced_motion"),
        Settings.bReducedMotion);
    Root->SetBoolField(
        TEXT("reduced_camera_shake"),
        Settings.bReducedCameraShake);
    Root->SetBoolField(
        TEXT("subtitles_enabled"),
        Settings.bSubtitlesEnabled);
    Root->SetStringField(
        TEXT("subtitle_presentation"),
        Settings.SubtitlePresentation.ToString());
    Root->SetStringField(
        TEXT("ui_readability_profile"),
        Settings.UiReadabilityProfile.ToString());
    Root->SetStringField(
        TEXT("color_vision_profile"),
        Settings.ColorVisionProfile.ToString());
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

FString FOGPlayerProfileSettingsService::DefaultProfilePath()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("OfflineGame"),
        TEXT("PlayerProfile.json"));
}

FName FOGPlayerProfileSettingsService::ResolveOrientation(
    const FOGPlayerProfileSettings& Settings,
    FName ModePreferredOrientation)
{
    if (Settings.OrientationLock.IsNone() ||
        Settings.OrientationLock ==
            FName(TEXT("automatic")))
    {
        return ModePreferredOrientation.IsNone()
            ? FName(TEXT("portrait"))
            : ModePreferredOrientation;
    }

    return Settings.OrientationLock;
}
