#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGDispatchService.h"
#include "World/OGFactionWarService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeStrategyTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool PersistEntity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& EntityId,
    FName Kind,
    FString& Error)
{
    return Store.UpsertEntity(
        EntityId,
        Kind,
        0,
        TEXT("{}"),
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDispatchPersistenceTest,
    "OfflineGame.WorldState.Dispatch.ResultOrientedPersistence",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDispatchPersistenceTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("dispatch.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId ScoutId =
        FOGEntityId::NewId();
    const FOGEntityId SpyId =
        FOGEntityId::NewId();
    const FOGEntityId TargetId =
        FOGEntityId::NewId();

    FOGEntityId DispatchId;

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Open database"),
            Store.Open(
                DatabasePath,
                Error));

        TestEqual(
            TEXT("Schema version is 12"),
            Store.GetSchemaVersion(Error),
            12);

        TestTrue(
            TEXT("Persist owner"),
            PersistEntity(
                Store,
                RulerId,
                TEXT("ruler"),
                Error));

        TestTrue(
            TEXT("Persist scout"),
            PersistEntity(
                Store,
                ScoutId,
                TEXT("character_manifestation"),
                Error));

        TestTrue(
            TEXT("Persist spy"),
            PersistEntity(
                Store,
                SpyId,
                TEXT("character_manifestation"),
                Error));

        TestTrue(
            TEXT("Persist target"),
            PersistEntity(
                Store,
                TargetId,
                TEXT("faction"),
                Error));

        FOGDispatchService Service(Store);

        TestTrue(
            TEXT("Start espionage dispatch"),
            Service.StartDispatch(
                RulerId,
                TargetId,
                EOGDispatchType::Espionage,
                {ScoutId, SpyId},
                100,
                300,
                2500,
                123456,
                DispatchId,
                Error));

        FOGDispatchRecord Early;
        TestFalse(
            TEXT("Dispatch cannot resolve before due tick"),
            Service.ResolveDispatch(
                DispatchId,
                299,
                true,
                TEXT("{\"intel\":\"gate\"}"),
                Early,
                Error));

        FOGDispatchRecord Resolved;
        TestTrue(
            TEXT("Dispatch resolves at due tick"),
            Service.ResolveDispatch(
                DispatchId,
                300,
                true,
                TEXT("{\"intel\":\"gate\"}"),
                Resolved,
                Error));

        TestEqual(
            TEXT("Dispatch result status is stored"),
            Resolved.Status,
            EOGDispatchStatus::Succeeded);

        TestEqual(
            TEXT("Dispatch keeps participant count"),
            Resolved.ParticipantEntityIds.Num(),
            2);
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Reopen database"),
            Store.Open(
                DatabasePath,
                Error));

        bool bFound = false;
        FOGDispatchRecord Stored;

        TestTrue(
            TEXT("Read dispatch after restart"),
            Store.TryReadDispatch(
                DispatchId,
                bFound,
                Stored,
                Error));

        TestTrue(
            TEXT("Dispatch survives restart"),
            bFound);

        TestEqual(
            TEXT("Dispatch type persists"),
            Stored.Type,
            EOGDispatchType::Espionage);

        TestEqual(
            TEXT("Dispatch participants persist"),
            Stored.ParticipantEntityIds.Num(),
            2);

        TestTrue(
            TEXT("Participant order is stable"),
            Stored.ParticipantEntityIds[0] == ScoutId &&
            Stored.ParticipantEntityIds[1] == SpyId);

        TestEqual(
            TEXT("Resolution seed persists"),
            Stored.ResolutionSeed,
            static_cast<int64>(123456));

        TestEqual(
            TEXT("Result payload persists"),
            Stored.ResultJson,
            FString(TEXT("{\"intel\":\"gate\"}")));
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGFactionArmyWarStateTest,
    "OfflineGame.WorldState.FactionWar.MinimalPersistentState",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGFactionArmyWarStateTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("war.db"));

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

    const FOGEntityId FactionAId =
        FOGEntityId::NewId();
    const FOGEntityId FactionBId =
        FOGEntityId::NewId();
    const FOGEntityId CommanderId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId ArmyId =
        FOGEntityId::NewId();

    FOGFactionRecord FactionA;
    FactionA.FactionId = FactionAId;
    FactionA.Kind = TEXT("kingdom");
    FactionA.Population = 500000;

    FOGFactionRecord FactionB;
    FactionB.FactionId = FactionBId;
    FactionB.Kind = TEXT("domain");
    FactionB.Population = 800000;

    TestTrue(
        TEXT("Persist faction A"),
        Store.UpsertFaction(
            FactionA,
            0,
            Error));

    TestTrue(
        TEXT("Persist faction B"),
        Store.UpsertFaction(
            FactionB,
            0,
            Error));

    TestTrue(
        TEXT("Persist named commander"),
        PersistEntity(
            Store,
            CommanderId,
            TEXT("character_manifestation"),
            Error));

    FOGLocationRecord Location;
    Location.LocationId = LocationId;
    Location.Kind = TEXT("frontier");

    TestTrue(
        TEXT("Persist army location"),
        Store.UpsertLocation(
            Location,
            0,
            Error));

    FOGArmyRecord Army;
    Army.ArmyId = ArmyId;
    Army.FactionId = FactionAId;
    Army.LocationId = LocationId;
    Army.Headcount = 100000;
    Army.EffectivePower =
        FOGLargeNumber::FromInt64(2500000);
    Army.CommanderEntityIds.Add(
        CommanderId);

    TestTrue(
        TEXT("Persist aggregated army"),
        Store.UpsertArmy(
            Army,
            10,
            Error));

    bool bArmyFound = false;
    FOGArmyRecord StoredArmy;

    TestTrue(
        TEXT("Read aggregated army"),
        Store.TryReadArmy(
            ArmyId,
            bArmyFound,
            StoredArmy,
            Error));

    TestTrue(
        TEXT("Army exists"),
        bArmyFound);

    TestEqual(
        TEXT("Ordinary force remains aggregate headcount"),
        StoredArmy.Headcount,
        static_cast<int64>(100000));

    TestEqual(
        TEXT("Named commander remains individualized"),
        StoredArmy.CommanderEntityIds.Num(),
        1);

    FOGFactionWarService Service(Store);

    TestTrue(
        TEXT("Form alliance"),
        Service.FormAlliance(
            FactionAId,
            FactionBId,
            20,
            Error));

    FOGEntityId CanonA = FactionAId;
    FOGEntityId CanonB = FactionBId;

    if (CanonA.ToString().Compare(
            CanonB.ToString(),
            ESearchCase::CaseSensitive) > 0)
    {
        Swap(CanonA, CanonB);
    }

    bool bAllianceFound = false;
    FOGFactionLinkRecord Alliance;

    TestTrue(
        TEXT("Read alliance"),
        Store.TryReadFactionLink(
            CanonA,
            CanonB,
            EOGFactionLinkType::Alliance,
            bAllianceFound,
            Alliance,
            Error));

    TestTrue(
        TEXT("Alliance exists"),
        bAllianceFound && Alliance.bActive);

    TestTrue(
        TEXT("Provide support is an explicit directional state"),
        Service.SetSupport(
            FactionAId,
            FactionBId,
            true,
            25,
            Error));

    bool bSupportFound = false;
    FOGFactionLinkRecord Support;

    TestTrue(
        TEXT("Read support link"),
        Store.TryReadFactionLink(
            FactionAId,
            FactionBId,
            EOGFactionLinkType::Support,
            bSupportFound,
            Support,
            Error));

    TestTrue(
        TEXT("Support state is active"),
        bSupportFound && Support.bActive);

    FOGEntityId WarId;

    TestTrue(
        TEXT("War can be declared without Casus Belli/treaty prerequisite"),
        Service.DeclareWar(
            FactionAId,
            FactionBId,
            TEXT("capture_core"),
            FOGEntityId(),
            30,
            WarId,
            Error));

    bAllianceFound = false;
    Alliance = FOGFactionLinkRecord();

    TestTrue(
        TEXT("Read alliance after declaration"),
        Store.TryReadFactionLink(
            CanonA,
            CanonB,
            EOGFactionLinkType::Alliance,
            bAllianceFound,
            Alliance,
            Error));

    TestTrue(
        TEXT("Existing alliance record remains explicit"),
        bAllianceFound);

    TestFalse(
        TEXT("Declaring war breaks active alliance"),
        Alliance.bActive);

    bool bWarFound = false;
    FOGWarRecord War;

    TestTrue(
        TEXT("Read active war"),
        Store.TryReadWar(
            WarId,
            bWarFound,
            War,
            Error));

    TestTrue(
        TEXT("War exists"),
        bWarFound);

    TestEqual(
        TEXT("War is active"),
        War.Status,
        EOGWarStatus::Active);

    TestEqual(
        TEXT("War has two participants"),
        War.Participants.Num(),
        2);

    TestTrue(
        TEXT("Resolve only after actual world/strategic outcome"),
        Service.ResolveWar(
            WarId,
            EOGWarStatus::Resolved,
            100,
            TEXT("{\"outcome\":\"objective_completed\"}"),
            Error));

    bWarFound = false;
    War = FOGWarRecord();

    TestTrue(
        TEXT("Read resolved war"),
        Store.TryReadWar(
            WarId,
            bWarFound,
            War,
            Error));

    TestTrue(
        TEXT("Resolved war persists"),
        bWarFound);

    TestEqual(
        TEXT("War resolution state persists"),
        War.Status,
        EOGWarStatus::Resolved);

    TestEqual(
        TEXT("War end tick persists"),
        War.EndWorldTick,
        static_cast<int64>(100));

    TestEqual(
        TEXT("Resolution is concise state, not treaty document"),
        War.ResolutionJson,
        FString(TEXT("{\"outcome\":\"objective_completed\"}")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGWarDeclarationDoesNotInventAllianceTest,
    "OfflineGame.WorldState.FactionWar.DeclareWarDoesNotInventAllianceHistory",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGWarDeclarationDoesNotInventAllianceTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeStrategyTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("no_fake_alliance.db"));

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

    const FOGEntityId FactionAId =
        FOGEntityId::NewId();
    const FOGEntityId FactionBId =
        FOGEntityId::NewId();

    FOGFactionRecord A;
    A.FactionId = FactionAId;
    A.Kind = TEXT("a");

    FOGFactionRecord B;
    B.FactionId = FactionBId;
    B.Kind = TEXT("b");

    TestTrue(
        TEXT("Persist faction A"),
        Store.UpsertFaction(A, 0, Error));

    TestTrue(
        TEXT("Persist faction B"),
        Store.UpsertFaction(B, 0, Error));

    FOGFactionWarService Service(Store);
    FOGEntityId WarId;

    TestTrue(
        TEXT("Declare war between never-allied factions"),
        Service.DeclareWar(
            FactionAId,
            FactionBId,
            TEXT("conquest"),
            FOGEntityId(),
            10,
            WarId,
            Error));

    FOGEntityId CanonA = FactionAId;
    FOGEntityId CanonB = FactionBId;
    if (CanonA.ToString().Compare(
            CanonB.ToString(),
            ESearchCase::CaseSensitive) > 0)
    {
        Swap(CanonA, CanonB);
    }

    bool bAllianceFound = true;
    FOGFactionLinkRecord Alliance;

    TestTrue(
        TEXT("Alliance lookup succeeds"),
        Store.TryReadFactionLink(
            CanonA,
            CanonB,
            EOGFactionLinkType::Alliance,
            bAllianceFound,
            Alliance,
            Error));

    TestFalse(
        TEXT("War declaration does not fabricate inactive alliance history"),
        bAllianceFound);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

#endif
