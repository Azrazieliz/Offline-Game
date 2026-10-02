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
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGachaPersistenceAndDuplicateTest::RunTest(const FString& Parameters)
{
    const FString Directory = MakeGachaTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("gacha.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FOGEntityId RulerId = FOGEntityId::NewId();
    const FOGGachaBannerDefinition Banner = MakeSingleFeaturedTopBanner();
    FOGEntityId ManifestationId;

    {
        FOGSQLiteWorldStore Store;
        FString Error;
        TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));
        TestEqual(TEXT("Schema version is 6"), Store.GetSchemaVersion(Error), 6);
        TestTrue(TEXT("Persist Ruler"), Store.UpsertEntity(
            RulerId, TEXT("ruler"), 0, TEXT("{}"), Error));
        TestTrue(TEXT("Seed pull currency"), Store.SetResourceBalance(
            RulerId, Banner.CurrencyId, 1000, Error));

        FOGGachaService Service(Store);
        FOGGachaPullResult First;
        TestTrue(TEXT("First pull succeeds"), Service.Pull(
            Banner, RulerId, 10, 101, First, Error));
        TestFalse(TEXT("First pull is not duplicate"), First.bDuplicateIdentity);
        TestTrue(TEXT("First pull is featured"), First.bFeatured);
        ManifestationId = First.ManifestationId;

        FOGGachaPullResult Second;
        TestTrue(TEXT("Second pull succeeds"), Service.Pull(
            Banner, RulerId, 20, 202, Second, Error));
        TestTrue(TEXT("Second pull is duplicate"), Second.bDuplicateIdentity);
        TestTrue(TEXT("Duplicate reuses Manifestation"),
            Second.ManifestationId == ManifestationId);
        TestEqual(TEXT("Duplicate count increments"),
            Second.DuplicateAcquisitionCount, 1);

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
        FOGCharacterManifestationRecord Manifestation;
        TestTrue(TEXT("Read Manifestation after restart"),
            Store.TryReadCharacterManifestation(
                ManifestationId, bFound, Manifestation, Error));
        TestTrue(TEXT("Manifestation survives restart"), bFound);
        TestEqual(TEXT("Duplicate count survives restart"),
            Manifestation.DuplicateAcquisitionCount, 1);

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
