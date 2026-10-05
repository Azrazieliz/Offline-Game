#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGSharedWorldStateService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeWorldStateTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGSharedWorldStateCrossModeTest,
    "OfflineGame.WorldState.WorldModeDiscoveryVisibleInRulerModeAfterRestart",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGSharedWorldStateCrossModeTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeWorldStateTestDirectory();

    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("shared_world.db"));

    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();

    const FOGEntityId OtherObserverId =
        FOGEntityId::NewId();

    const FOGEntityId RuinLocationId =
        FOGEntityId::NewId();

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Open database"),
            Store.Open(
                DatabasePath,
                Error));

        TestEqual(
            TEXT("Schema version is 13"),
            Store.GetSchemaVersion(Error),
            13);

        TestTrue(
            TEXT("Persist Ruler entity"),
            Store.UpsertEntity(
                RulerId,
                TEXT("ruler"),
                0,
                TEXT("{}"),
                Error));

        TestTrue(
            TEXT("Persist second observer entity"),
            Store.UpsertEntity(
                OtherObserverId,
                TEXT("faction"),
                0,
                TEXT("{}"),
                Error));

        FOGLocationRecord Ruin;
        Ruin.LocationId =
            RuinLocationId;
        Ruin.Kind =
            TEXT("ruin");
        Ruin.bPhysicallyAccessible =
            true;

        TestTrue(
            TEXT("Persist physical ruin"),
            Store.UpsertLocation(
                Ruin,
                10,
                Error));

        FOGSharedWorldStateService WorldMode(
            Store);

        FOGWorldPresenceRecord Presence;
        Presence.EntityId =
            RulerId;
        Presence.LocationId =
            RuinLocationId;
        Presence.LocalPosition =
            FVector3d(
                120.5,
                -33.25,
                900.0);
        Presence.MovementContext =
            TEXT("ground");
        Presence.UpdatedWorldTick =
            100;

        TestTrue(
            TEXT("World Mode writes authoritative physical presence"),
            WorldMode.UpdatePhysicalPresence(
                Presence,
                Error));

        TestTrue(
            TEXT("World Mode records located knowledge"),
            WorldMode.RecordLocationDiscovery(
                RulerId,
                RuinLocationId,
                EOGLocationKnowledgeLevel::Located,
                101,
                Error));

        TestTrue(
            TEXT("A weaker later report cannot downgrade knowledge"),
            WorldMode.RecordLocationDiscovery(
                RulerId,
                RuinLocationId,
                EOGLocationKnowledgeLevel::Rumored,
                102,
                Error));

        TestTrue(
            TEXT("Direct observation upgrades knowledge"),
            WorldMode.RecordLocationDiscovery(
                RulerId,
                RuinLocationId,
                EOGLocationKnowledgeLevel::Observed,
                103,
                Error));
    }

    {
        FOGSQLiteWorldStore Store;
        FString Error;

        TestTrue(
            TEXT("Reopen database"),
            Store.Open(
                DatabasePath,
                Error));

        FOGSharedWorldStateService RulerMode(
            Store);

        bool bPresenceFound = false;
        FOGWorldPresenceRecord Presence;

        TestTrue(
            TEXT("Ruler Mode reads shared physical presence"),
            RulerMode.TryReadPhysicalPresence(
                RulerId,
                bPresenceFound,
                Presence,
                Error));

        TestTrue(
            TEXT("Physical presence survives restart"),
            bPresenceFound);

        TestTrue(
            TEXT("Ruler Mode reads the same physical location"),
            Presence.LocationId ==
                RuinLocationId);

        TestEqual(
            TEXT("Local X persists"),
            Presence.LocalPosition.X,
            120.5);

        TestEqual(
            TEXT("Local Y persists"),
            Presence.LocalPosition.Y,
            -33.25);

        TestEqual(
            TEXT("Local Z persists"),
            Presence.LocalPosition.Z,
            900.0);

        TestEqual(
            TEXT("Movement context persists"),
            Presence.MovementContext,
            FName(TEXT("ground")));

        bool bKnown = false;
        EOGLocationKnowledgeLevel Knowledge =
            EOGLocationKnowledgeLevel::Rumored;

        TestTrue(
            TEXT("Ruler Mode reads World Mode discovery"),
            RulerMode.TryReadLocationKnowledge(
                RulerId,
                RuinLocationId,
                bKnown,
                Knowledge,
                Error));

        TestTrue(
            TEXT("Ruler knows the discovered ruin"),
            bKnown);

        TestEqual(
            TEXT("Highest acquired knowledge is retained"),
            Knowledge,
            EOGLocationKnowledgeLevel::Observed);

        bool bOtherKnows = true;
        EOGLocationKnowledgeLevel OtherKnowledge =
            EOGLocationKnowledgeLevel::Rumored;

        TestTrue(
            TEXT("Other observer knowledge lookup succeeds"),
            RulerMode.TryReadLocationKnowledge(
                OtherObserverId,
                RuinLocationId,
                bOtherKnows,
                OtherKnowledge,
                Error));

        TestFalse(
            TEXT("Physical truth does not imply omniscient knowledge"),
            bOtherKnows);

        bool bLocationFound = false;
        FOGLocationRecord StoredLocation;

        TestTrue(
            TEXT("Read canonical physical location"),
            Store.TryReadLocation(
                RuinLocationId,
                bLocationFound,
                StoredLocation,
                Error));

        TestTrue(
            TEXT("Ruin remains physically real even for observers who do not know it"),
            bLocationFound);

        TestEqual(
            TEXT("Location kind persists"),
            StoredLocation.Kind,
            FName(TEXT("ruin")));
    }

    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);

    return true;
}

#endif
