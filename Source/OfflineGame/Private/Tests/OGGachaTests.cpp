#include "Gacha/OGGachaService.h"
#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeGachaTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

FOGGachaBannerDefinition MakeSingleFeaturedTopBanner()
{
    FOGGachaBannerDefinition Banner;
    Banner.BannerId = FOGContentId(TEXT("test:banner.slice"));
    Banner.PityCategory = TEXT("standard");
    Banner.CurrencyId = FOGContentId(TEXT("test:currency.pull"));
    Banner.PullCost = 100;
    Banner.TopRarity = TEXT("UR");
    Banner.SoftPityStart = 0;
    Banner.HardPity = 1;

    FOGGachaPoolEntry Entry;
    Entry.IdentityId = FOGContentId(TEXT("test:character.alpha"));
    Entry.VersionId = FOGContentId(TEXT("test:character.alpha.base"));
    Entry.Rarity = TEXT("UR");
    Entry.Weight = 100;
    Entry.bFeatured = true;
    Banner.Entries.Add(Entry);
    return Banner;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGachaPersistenceAndDuplicateTest,
    "OfflineGame.Gacha.PersistentPityCurrencyAndDuplicateAcquisition",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGachaPersistenceAndDuplicateTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeGachaTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("gacha.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId RulerId = FOGEntityId::NewId();
    const FOGGachaBannerDefinition Banner = MakeSingleFeaturedTopBanner();
    FOGEntityId FirstManifestationId;
    FOGEntityId SecondManifestationId;

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));
        TestEqual(TEXT("Schema version is 14"), Store.GetSchemaVersion(Error), 14);
        TestTrue(TEXT("Persist Ruler"), Store.UpsertEntity(
            RulerId, TEXT("ruler"), 0, TEXT("{}"), Error));
        TestTrue(TEXT("Seed pull currency"), Store.SetResourceBalance(
            RulerId, Banner.CurrencyId, 1000, Error));

        FOGRulerGachaAccessRecord Access;
        Access.RulerId = RulerId;
        Access.bPermanentlyUnlocked = true;
        Access.bHasUnlockedWorldTick = true;
        Access.UnlockedWorldTick = 0;
        Access.UpdatedWorldTick = 0;
        TestTrue(TEXT("Seed already-earned permanent gacha access"),
            Store.UpsertRulerGachaAccess(
                Access,
                Error));

        FOGGachaService Service(Store);
        FOGGachaPullResult First;
        TestTrue(TEXT("First pull succeeds"), Service.Pull(
            Banner, RulerId, 10, 101, First, Error));
        TestFalse(TEXT("First pull is not duplicate"), First.bDuplicateIdentity);
        TestTrue(TEXT("First pull is featured"), First.bFeatured);
        FirstManifestationId = First.ManifestationId;
        TestEqual(TEXT("First pull increments persistent total"),
            First.UpdatedState.TotalPulls, static_cast<int64>(1));

        FOGGachaPullResult Second;
        TestTrue(TEXT("Second pull succeeds"), Service.Pull(
            Banner, RulerId, 20, 202, Second, Error));
        TestTrue(TEXT("Second pull reports Identity already owned"), Second.bDuplicateIdentity);
        SecondManifestationId = Second.ManifestationId;
        TestTrue(TEXT("Repeat acquisition creates a distinct Manifestation"),
            SecondManifestationId != FirstManifestationId);

        TArray<FOGCharacterManifestationRecord> OwnedCopies;
        TestTrue(TEXT("List both same-Identity Manifestations"),
            Store.ListCharacterManifestationsByOwnerAndIdentity(
                RulerId,
                Banner.Entries[0].IdentityId,
                OwnedCopies,
                Error));
        TestEqual(TEXT("Two independent Manifestations are owned"),
            OwnedCopies.Num(), 2);
        if (OwnedCopies.Num() == 2)
        {
            TestEqual(TEXT("First acquisition ordinal"),
                OwnedCopies[0].AcquisitionOrdinal, 0);
            TestEqual(TEXT("Second acquisition ordinal"),
                OwnedCopies[1].AcquisitionOrdinal, 1);
            TestTrue(TEXT("Each Manifestation retains its own origin event"),
                OwnedCopies[0].OriginPullEventId.IsValid() &&
                OwnedCopies[1].OriginPullEventId.IsValid() &&
                OwnedCopies[0].OriginPullEventId != OwnedCopies[1].OriginPullEventId);
        }

        bool bKnown = false;
        int64 Balance = 0;
        TestTrue(TEXT("Read pull currency"), Store.TryReadResourceBalance(
            RulerId, Banner.CurrencyId, bKnown, Balance, Error));
        TestTrue(TEXT("Currency remains known"), bKnown);
        TestEqual(TEXT("Two costs deducted"), Balance, static_cast<int64>(800));
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Reopen database"), Store.Open(DatabasePath, Error));

        bool bFound = false;
        TArray<FOGCharacterManifestationRecord> OwnedCopies;
        TestTrue(TEXT("List Manifestations after restart"),
            Store.ListCharacterManifestationsByOwnerAndIdentity(
                RulerId,
                Banner.Entries[0].IdentityId,
                OwnedCopies,
                Error));
        TestEqual(TEXT("Both independent Manifestations survive restart"),
            OwnedCopies.Num(), 2);
        if (OwnedCopies.Num() == 2)
        {
            TestTrue(TEXT("First Manifestation ID survives restart"),
                OwnedCopies[0].ManifestationId == FirstManifestationId);
            TestTrue(TEXT("Second Manifestation ID survives restart"),
                OwnedCopies[1].ManifestationId == SecondManifestationId);
        }

        FOGGachaStateRecord State;
        TestTrue(TEXT("Read pity after restart"), Store.TryReadGachaState(
            RulerId, Banner.PityCategory, bFound, State, Error));
        TestTrue(TEXT("Pity survives restart"), bFound);
        TestEqual(TEXT("Total pulls survive restart"),
            State.TotalPulls, static_cast<int64>(2));
        TestEqual(TEXT("Top rarity resets pity"), State.PullsSinceTopRarity, 0);
    }

    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGachaTicketFirstPaymentTest,
    "OfflineGame.Gacha.CompatibleTicketIsConsumedBeforeCurrency",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGachaTicketFirstPaymentTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeGachaTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("gacha_ticket_first.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open database"),
        Store.Open(
            DatabasePath,
            Error));

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    FOGGachaBannerDefinition Banner =
        MakeSingleFeaturedTopBanner();
    const FOGContentId TicketId(
        TEXT("test:ticket.standard"));
    Banner.CompatibleTicketIds.Add(
        TicketId);

    TestTrue(
        TEXT("Persist Ruler"),
        Store.UpsertEntity(
            RulerId,
            FName(TEXT("ruler")),
            0,
            TEXT("{}"),
            Error));

    FOGRulerGachaAccessRecord Access;
    Access.RulerId =
        RulerId;
    Access.bPermanentlyUnlocked =
        true;
    Access.bHasUnlockedWorldTick =
        true;
    TestTrue(
        TEXT("Persist earned permanent gacha access"),
        Store.UpsertRulerGachaAccess(
            Access,
            Error));

    TestTrue(
        TEXT("Seed one compatible ordinary ticket"),
        Store.SetResourceBalance(
            RulerId,
            TicketId,
            1,
            Error));
    TestTrue(
        TEXT("Seed pull currency"),
        Store.SetResourceBalance(
            RulerId,
            Banner.CurrencyId,
            1000,
            Error));

    FOGGachaService Service(
        Store);
    FOGGachaPullResult TicketPull;
    TestTrue(
        TEXT("Pull succeeds with compatible ticket"),
        Service.Pull(
            Banner,
            RulerId,
            10,
            111,
            TicketPull,
            Error));
    TestTrue(
        TEXT("Result records ticket payment"),
        TicketPull.bUsedTicket);
    TestTrue(
        TEXT("Result records exact ticket resource"),
        TicketPull.PaymentResourceId ==
            TicketId);

    bool bKnown = false;
    int64 Balance = -1;
    TestTrue(
        TEXT("Read ticket after pull"),
        Store.TryReadResourceBalance(
            RulerId,
            TicketId,
            bKnown,
            Balance,
            Error));
    TestTrue(
        TEXT("Compatible ticket was consumed first"),
        bKnown &&
        Balance == 0);

    bKnown = false;
    Balance = -1;
    TestTrue(
        TEXT("Read currency after ticket-funded pull"),
        Store.TryReadResourceBalance(
            RulerId,
            Banner.CurrencyId,
            bKnown,
            Balance,
            Error));
    TestTrue(
        TEXT("Currency is untouched while a ticket was available"),
        bKnown &&
        Balance == 1000);

    FOGGachaPullResult CurrencyPull;
    TestTrue(
        TEXT("Second pull falls back to currency after ticket depletion"),
        Service.Pull(
            Banner,
            RulerId,
            20,
            222,
            CurrencyPull,
            Error));
    TestFalse(
        TEXT("Fallback result is not marked ticket-funded"),
        CurrencyPull.bUsedTicket);
    TestTrue(
        TEXT("Fallback records pull currency resource"),
        CurrencyPull.PaymentResourceId ==
            Banner.CurrencyId);

    bKnown = false;
    Balance = -1;
    TestTrue(
        TEXT("Read currency after fallback pull"),
        Store.TryReadResourceBalance(
            RulerId,
            Banner.CurrencyId,
            bKnown,
            Balance,
            Error));
    TestTrue(
        TEXT("Fallback consumes configured currency cost"),
        bKnown &&
        Balance == 900);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
