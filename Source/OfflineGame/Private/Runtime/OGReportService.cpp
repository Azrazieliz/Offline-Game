#include "Runtime/OGReportService.h"

bool FOGReportService::CreateReport(
    const FOGEntityId& OwnerEntityId,
    const FOGEntityId& SourceWorldEventId,
    FName Category,
    int32 Priority,
    int64 WorldTick,
    const FString& PayloadJson,
    FOGEntityId& OutReportId,
    FString& OutError)
{
    OutReportId =
        FOGEntityId();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        Category.IsNone() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Report creation request is invalid.");
        return false;
    }

    FOGReportRecord Report;
    Report.ReportId =
        FOGEntityId::NewId();
    Report.OwnerEntityId =
        OwnerEntityId;
    Report.SourceWorldEventId =
        SourceWorldEventId;
    Report.Category =
        Category;
    Report.Priority =
        Priority;
    Report.CreatedWorldTick =
        WorldTick;
    Report.PayloadJson =
        PayloadJson.IsEmpty()
            ? TEXT("{}")
            : PayloadJson;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    auto Rollback =
        [this, &OutError]()
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            if (!RollbackError.IsEmpty())
            {
                OutError += FString::Printf(
                    TEXT(" | Rollback error: %s"),
                    *RollbackError);
            }
            return false;
        };

    if (!Store.UpsertReport(
            Report,
            WorldTick,
            OutError))
    {
        return Rollback();
    }

    FOGReportDeliveryRecord InGame;
    InGame.ReportId =
        Report.ReportId;
    InGame.Channel =
        FName(TEXT("ingame"));
    InGame.State =
        FName(TEXT("available"));

    if (!Store.UpsertReportDelivery(
            InGame,
            OutError))
    {
        return Rollback();
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        return Rollback();
    }

    OutReportId =
        Report.ReportId;
    return true;
}

bool FOGReportService::AcknowledgeReport(
    const FOGEntityId& ReportId,
    int64 WorldTick,
    FString& OutError)
{
    bool bFound = false;
    FOGReportRecord Report;
    if (!Store.TryReadReport(
            ReportId,
            bFound,
            Report,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot acknowledge an unknown Report.");
        return false;
    }

    if (WorldTick <
        Report.CreatedWorldTick)
    {
        OutError =
            TEXT("Report acknowledgement cannot predate Report creation.");
        return false;
    }

    Report.bAcknowledged =
        true;
    Report.AcknowledgedWorldTick =
        WorldTick;

    return Store.UpsertReport(
        Report,
        Report.CreatedWorldTick,
        OutError);
}

bool FOGReportService::SetDelivery(
    const FOGReportDeliveryRecord& Delivery,
    FString& OutError)
{
    return Store.UpsertReportDelivery(
        Delivery,
        OutError);
}

bool FOGReportService::DeliverAndroidProjection(
    const FOGEntityId& ReportId,
    FName PrivacyState,
    const FString& ScheduledRealUtc,
    const FString& DeliveredRealUtc,
    IOGAndroidNotificationBridge& Bridge,
    FString& OutError)
{
    bool bFound = false;
    FOGReportRecord Report;
    if (!Store.TryReadReport(
            ReportId,
            bFound,
            Report,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot project notification for an unknown Report.");
        return false;
    }

    FOGReportDeliveryRecord Delivery;
    Delivery.ReportId =
        ReportId;
    Delivery.Channel =
        FName(TEXT("android_notification"));
    Delivery.State =
        FName(TEXT("scheduled"));
    Delivery.ScheduledRealUtc =
        ScheduledRealUtc;
    Delivery.PrivacyState =
        PrivacyState.IsNone()
            ? FName(TEXT("default"))
            : PrivacyState;

    if (!Store.UpsertReportDelivery(
            Delivery,
            OutError))
    {
        return false;
    }

    FString PlatformId;
    if (!Bridge.ProjectNotification(
            Report,
            Delivery,
            PlatformId,
            OutError))
    {
        Delivery.State =
            FName(TEXT("failed"));
        FString PersistError;
        Store.UpsertReportDelivery(
            Delivery,
            PersistError);
        return false;
    }

    Delivery.State =
        FName(TEXT("delivered"));
    Delivery.DeliveredRealUtc =
        DeliveredRealUtc;
    Delivery.PlatformNotificationId =
        PlatformId;

    return Store.UpsertReportDelivery(
        Delivery,
        OutError);
}
