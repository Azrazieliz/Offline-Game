#pragma once

#include "CoreMinimal.h"

/**
 * Versioned non-authoritative player/device profile. It is deliberately stored
 * outside the canonical world database and may affect presentation/input only.
 */
struct OFFLINEGAME_API FOGPlayerProfileSettings
{
    int32 Version = 4;

    bool bSfwPresentation = false;
    FName OrientationLock = FName(TEXT("automatic"));
    FName RosterDensity = FName(TEXT("dense"));

    float MasterVolume = 1.0f;
    float MusicVolume = 1.0f;
    /** Legacy v1/v2 aggregate effects volume; retained for forward migration. */
    float EffectsVolume = 1.0f;
    float VoiceVolume = 1.0f;
    float SfxVolume = 1.0f;
    float AmbienceVolume = 1.0f;
    FName DynamicRangeProfile = FName(TEXT("full"));

    bool bHapticsEnabled = true;
    float HapticsIntensity = 0.80f;
    float CameraHorizontalSensitivity = 1.10f;
    float CameraVerticalSensitivity = 0.82f;
    float CameraResponseExponent = 1.55f;
    bool bInvertCameraX = false;
    bool bInvertCameraY = false;
    bool bSprintToggle = false;
    bool bLeftHandedControls = false;
    float TouchControlScale = 1.0f;
    float TouchControlOpacity = 0.74f;
    float MovementDeadzone = 0.08f;
    float LookDeadzone = 0.03f;
    float MovementStickInset = 0.105f;
    float MovementStickBottom = 0.14f;
    float ActionClusterHorizontalOffset = 0.0f;
    float ActionClusterVerticalOffset = 0.0f;
    FString ControlProfileJson = TEXT("{}");

    bool bReducedMotion = false;
    bool bReducedCameraShake = false;
    bool bSubtitlesEnabled = true;
    FName SubtitlePresentation = FName(TEXT("standard"));
    FName UiReadabilityProfile = FName(TEXT("standard"));
    FName ColorVisionProfile = FName(TEXT("standard"));
    FName DamageNumberPresentation = FName(TEXT("standard"));
    FString CinematicPreferencesJson = TEXT("{}");

    bool bAutoDownload = true;
    FString NetworkPreferencesJson = TEXT("{\"large_downloads\":\"unmetered_only\"}");
    FString AccessibilityJson = TEXT("{}");

    /** Non-authoritative rule/team preference. Committed actions store resolved commands. */
    FString AutoCombatPresetJson = TEXT("{}");
};

class OFFLINEGAME_API FOGPlayerProfileSettingsService
{
public:
    static bool Load(
        const FString& ProfilePath,
        FOGPlayerProfileSettings& OutSettings,
        FString& OutError);

    static bool Save(
        const FString& ProfilePath,
        const FOGPlayerProfileSettings& Settings,
        FString& OutError);

    /** Stable non-authoritative profile location for runtime presentation settings. */
    static FString DefaultProfilePath();

    /**
     * Automatic follows the presentation mode's preferred orientation; an
     * explicit player lock wins until changed by the player.
     */
    static FName ResolveOrientation(
        const FOGPlayerProfileSettings& Settings,
        FName ModePreferredOrientation);
};
