#include "Progression/OGClassRecognitionService.h"

#include "Events/OGWorldEvent.h"

bool FOGClassRecognitionService::RecognizeClass(
    const FOGEntityClassRecord& ClassState,
    FString& OutError)
{
    // Recognition inputs are resolved from actual mastery/capability elsewhere;
    // this service never grants techniques merely because a Class label exists.
    return Store.UpsertEntityClass(
        ClassState,
        OutError);
}

bool FOGClassRecognitionService::AppointGrandSeat(
    const FOGContentId& ClassId,
    const FOGEntityId& BearerEntityId,
    int64 WorldTick,
    const FString& MandateStateJson,
    FString& OutError)
{
    OutError.Reset();

    if (!ClassId.IsValid() ||
        !BearerEntityId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Grand appointment request is invalid.");
        return false;
    }

    bool bClassFound = false;
    FOGEntityClassRecord ClassState;
    if (!Store.TryReadEntityClass(
            BearerEntityId,
            ClassId,
            bClassFound,
            ClassState,
            OutError))
    {
        return false;
    }

    if (!bClassFound ||
        ClassState.AttainedTier !=
            FName(TEXT("crown")))
    {
        OutError =
            TEXT("Grand appointment requires attained Crown mastery of the canonical Class.");
        return false;
    }

    bool bExistingSeat = false;
    FOGGrandClassSeatRecord ExistingSeat;
    if (!Store.TryReadGrandClassSeat(
            ClassId,
            bExistingSeat,
            ExistingSeat,
            OutError))
    {
        return false;
    }

    if (bExistingSeat &&
        ExistingSeat.BearerEntityId !=
            BearerEntityId)
    {
        OutError =
            TEXT("Canonical Class already has a Grand bearer; explicit vacancy is required.");
        return false;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    FOGGrandClassSeatRecord Seat;
    Seat.ClassId = ClassId;
    Seat.BearerEntityId =
        BearerEntityId;
    Seat.AppointedWorldTick =
        bExistingSeat
            ? ExistingSeat.AppointedWorldTick
            : WorldTick;
    Seat.SeatState =
        FName(TEXT("active"));
    Seat.MandateStateJson =
        MandateStateJson.IsEmpty()
            ? TEXT("{}")
            : MandateStateJson;

    if (!Store.UpsertGrandClassSeat(
            Seat,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!bExistingSeat)
    {
        FOGWorldEvent Event;
        Event.EventId =
            FOGEntityId::NewId();
        Event.EventType =
            FName(TEXT("class.grand_appointed"));
        Event.WorldTick =
            WorldTick;
        Event.PrimaryEntity =
            BearerEntityId;
        Event.bChronicleEligible =
            true;
        Event.PayloadJson = FString::Printf(
            TEXT("{\"class\":\"%s\"}"),
            *ClassId.ToString());

        if (!Store.AppendWorldEvent(
                Event,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGClassRecognitionService::VacateGrandSeat(
    const FOGContentId& ClassId,
    int64 WorldTick,
    FName Reason,
    FString& OutError)
{
    OutError.Reset();

    if (!ClassId.IsValid() ||
        WorldTick < 0 ||
        Reason.IsNone())
    {
        OutError =
            TEXT("Grand-seat vacancy request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGGrandClassSeatRecord Seat;
    if (!Store.TryReadGrandClassSeat(
            ClassId,
            bFound,
            Seat,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot vacate a Grand seat that is already empty.");
        return false;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.DeleteGrandClassSeat(
            ClassId,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("class.grand_vacated"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Seat.BearerEntityId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"class\":\"%s\",\"reason\":\"%s\"}"),
        *ClassId.ToString(),
        *Reason.ToString());

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}
