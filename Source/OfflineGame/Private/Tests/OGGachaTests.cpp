#include "Gacha/OGGachaService.h"
#include "Gacha/OGRulerGachaAccessService.h"
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
    EAutomationTestFlags::ApplicationContextMask |
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
        TestEqual(TEXT("Schema version is 8"), Store.GetSchemaVersion(Error), 8);
        TestTrue(TEXT("Persist Ruler"), Store.UpsertEntity(
            RulerId, TEXT("ruler"), 0, TEXT("{}"), Error));
        TestTrue(TEXT("Seed pull currency"), Store.SetResourceBalance(
            RulerId, Banner.CurrencyId, 1000, Error));

        FOGRulerGachaAccessService AccessService(Store);
        FOGRulerGachaAccessRecord Access;
        TestTrue(TEXT("Start continuous Territory qualification"),
            AccessService.RefreshGachaQualification(
                RulerId,
                0,
                true,
                false,
                false,
                Access,
                Error));
        TestFalse(TEXT("Qualification alone does not unlock gacha"),
            Access.bPermanentlyUnlocked);
        TestTrue(TEXT("Unlock after authoritative calendar confirms more than one month"),
            AccessService.RefreshGachaQualification(
                RulerId,
                1,
                true,
                false,
                true,
                Access,
                Error));
        TestTrue(TEXT("Gacha unlock is permanent"),
            Access.bPermanentlyUnlocked);

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

#endif
