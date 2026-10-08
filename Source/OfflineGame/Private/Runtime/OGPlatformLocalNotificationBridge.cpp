#include "Runtime/OGPlatformLocalNotificationBridge.h"

#include "Kismet/BlueprintPlatformLibrary.h"
#include "Misc/DateTime.h"

bool FOGPlatformLocalNotificationBridge::ProjectNotification(
    const FOGReportRecord& Report,
    const FOGReportDeliveryRecord& Delivery,
    FString& OutPlatformNotificationId,
    FString& OutError)
{
    OutPlatformNotificationId.Reset();
    OutError.Reset();

    if (!Report.ReportId.IsValid())
    {
        OutError = TEXT("Cannot schedule a notification for an invalid Report.");
        return false;
    }

    const FString ActivationEvent = FString::Printf(
        TEXT("offlinegame-report:%s"),
        *Report.ReportId.ToString());

    const bool bPrivacyLimited =
        Delivery.PrivacyState == FName(TEXT("private")) ||
        Delivery.PrivacyState == FName(TEXT("sfw")) ||
        Delivery.PrivacyState == FName(TEXT("generic"));

    const FText Title = FText::FromString(TEXT("Offline Game"));
    const FText Body = FText::FromString(
        bPrivacyLimited
            ? TEXT("A new report is available.")
            : FString::Printf(
                  TEXT("New %s report available."),
                  *Report.Category.ToString()));

    FDateTime FireUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(1.0);
    if (!Delivery.ScheduledRealUtc.IsEmpty())
    {
        FDateTime Parsed;
        if (FDateTime::ParseIso8601(*Delivery.ScheduledRealUtc, Parsed))
        {
            FireUtc = Parsed;
        }
    }

    const FDateTime MinimumFire = FDateTime::UtcNow() + FTimespan::FromSeconds(1.0);
    if (FireUtc < MinimumFire)
    {
        FireUtc = MinimumFire;
    }

    const int32 NotificationId =
        UBlueprintPlatformLibrary::ScheduleLocalNotificationAtTime(
            FireUtc,
            false,
            Title,
            Body,
            FText::FromString(TEXT("Open")),
            ActivationEvent);

    if (NotificationId < 0)
    {
        OutError = TEXT("Platform local-notification service rejected the schedule request.");
        return false;
    }

    OutPlatformNotificationId = LexToString(NotificationId);
    return true;
}
