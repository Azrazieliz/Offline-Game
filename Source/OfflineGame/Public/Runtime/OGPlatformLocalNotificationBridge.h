#pragma once

#include "CoreMinimal.h"
#include "Runtime/OGReportService.h"

/**
 * Engine-native Android/local-notification projection for persisted Reports.
 * Notifications are presentation only and never become authoritative world state.
 */
class OFFLINEGAME_API FOGPlatformLocalNotificationBridge final
    : public IOGAndroidNotificationBridge
{
public:
    virtual bool ProjectNotification(
        const FOGReportRecord& Report,
        const FOGReportDeliveryRecord& Delivery,
        FString& OutPlatformNotificationId,
        FString& OutError) override;
};
