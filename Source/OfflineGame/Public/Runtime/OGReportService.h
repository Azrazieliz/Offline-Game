#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGPackageReportManagementRecords.h"

class OFFLINEGAME_API IOGAndroidNotificationBridge
{
public:
    virtual ~IOGAndroidNotificationBridge() = default;

    /**
     * Projects an already-persisted Report into the Android notification
     * surface. The bridge never mutates canonical world events.
     */
    virtual bool ProjectNotification(
        const FOGReportRecord& Report,
        const FOGReportDeliveryRecord& Delivery,
        FString& OutPlatformNotificationId,
        FString& OutError) = 0;
};

class OFFLINEGAME_API FOGReportService
{
public:
    explicit FOGReportService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool CreateReport(
        const FOGEntityId& OwnerEntityId,
        const FOGEntityId& SourceWorldEventId,
        FName Category,
        int32 Priority,
        int64 WorldTick,
        const FString& PayloadJson,
        FOGEntityId& OutReportId,
        FString& OutError);

    bool AcknowledgeReport(
        const FOGEntityId& ReportId,
        int64 WorldTick,
        FString& OutError);

    bool SetDelivery(
        const FOGReportDeliveryRecord& Delivery,
        FString& OutError);

    bool DeliverAndroidProjection(
        const FOGEntityId& ReportId,
        FName PrivacyState,
        const FString& ScheduledRealUtc,
        const FString& DeliveredRealUtc,
        IOGAndroidNotificationBridge& Bridge,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
