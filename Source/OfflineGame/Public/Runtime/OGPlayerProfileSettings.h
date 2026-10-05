#pragma once

#include "CoreMinimal.h"

/**
 * Versioned non-authoritative player/device profile. It is deliberately stored
 * outside the canonical world database and may affect presentation/input only.
 */
struct OFFLINEGAME_API FOGPlayerProfileSettings
{
    int32 Version = 1;

    bool bSfwPresentation = false;
    FName OrientationLock = FName(TEXT("automatic"));
    FName RosterDensity = FName(TEXT("comfortable"));

    float MasterVolume = 1.0f;
    float MusicVolume = 1.0f;
    float EffectsVolume = 1.0f;
    FName DynamicRangeProfile = FName(TEXT("full"));

    bool bHapticsEnabled = true;
    FString ControlProfileJson = TEXT("{}");

    bool bReducedMotion = false;
    bool bReducedCameraShake = false;
    FName DamageNumberPresentation = FName(TEXT("standard"));
    FString CinematicPreferencesJson = TEXT("{}");

    bool bAutoDownload = false;
    FString NetworkPreferencesJson = TEXT("{}");
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

    /**
     * Automatic follows the current device orientation; an explicit player lock
     * wins until changed by the player.
     */
    static FName ResolveOrientation(
        const FOGPlayerProfileSettings& Settings,
        FName CurrentDeviceOrientation);
};
