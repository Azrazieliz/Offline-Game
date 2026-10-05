#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGDomainCoreService.h"
#include "World/OGProjectService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeDomainCoreTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PersistCoreTestRuler(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& RulerId,
    FString& Error)
{
    return Store.UpsertEntity(
        RulerId,
        TEXT("ruler"),
        0,
        TEXT("{}"),
        Error);
}

bool PersistCoreTestTerritory(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& RulerId,
    const FOGEntityId& LocationId,
    const FOGEntityId& TerritoryId,
    FString& Error)
{
    FOGLocationRecord Location;
    Location.LocationId =
        LocationId;
    Location.Kind =
        FName(TEXT("region"));

    if (!Store.UpsertLocation(
            Location,
            0,
            Error))
    {
        return false;
    }

    FOGTerritoryRecord Territory;
    Territory.TerritoryId =
        TerritoryId;
    Territory.RulerId =
        RulerId;
    Territory.RootLocationId =
        LocationId;
    Territory.bMainTerritory =
        false;
    Territory.Population =
        100;
    Territory.ControlState =
        FName(TEXT("controlled"));

    return Store.UpsertTerritory(
        Territory,
        0,
        Error);
}

FOGDomainCoreRecord MakeAwakenedCore(
    const FOGEntityId& CoreId,
    const FOGEntityId& TerritoryId,
    const FOGEntityId& RulerId,
    const FOGContentId& ConceptId,
    int32 Grade,
    int64 Durability)
{
    FOGDomainCoreRecord Core;
    Core.CoreId =
        CoreId;
    Core.TerritoryId =
        TerritoryId;
    Core.ControllerRulerId =
        RulerId;
    Core.Lifecycle =
        EOGDomainCoreLifecycle::Awakened;
    Core.CurrentDurability =
        FOGLargeNumber::FromInt64(
            Durability);
    Core.MaxDurability =
        FOGLargeNumber::FromInt64(
            Durability);

    FOGDomainCoreAspect Aspect;
    Aspect.AspectId =
        ConceptId;
    Aspect.Grade =
        Grade;
    Core.Aspects.Add(
        Aspect);
    return Core;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDomainCoreFusionLineageTest,
    "OfflineGame.DomainCore.Fusion.AsymmetricConceptsLineageAndHeartLoss",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDomainCoreFusionLineageTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeDomainCoreTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("fusion.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-9 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 9"),
        Store.GetSchemaVersion(
            Error),
        9);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId LocationA =
        FOGEntityId::NewId();
    const FOGEntityId LocationB =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryA =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryB =
        FOGEntityId::NewId();
    const FOGEntityId CoreAId =
        FOGEntityId::NewId();
    const FOGEntityId CoreBId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist fusion-test Ruler"),
        PersistCoreTestRuler(
            Store,
            RulerId,
            Error));
    TestTrue(
        TEXT("Persist absorber Territory"),
        PersistCoreTestTerritory(
            Store,
            RulerId,
            LocationA,
            TerritoryA,
            Error));
    TestTrue(
        TEXT("Persist absorbed Territory"),
        PersistCoreTestTerritory(
            Store,
            RulerId,
            LocationB,
            TerritoryB,
            Error));

    FOGDomainCoreRecord CoreA =
        MakeAwakenedCore(
            CoreAId,
            TerritoryA,
            RulerId,
            FOGContentId(
                TEXT("test:concept.time")),
            2,
            1000);
    FOGDomainCoreRecord CoreB =
        MakeAwakenedCore(
            CoreBId,
            TerritoryB,
            RulerId,
            FOGContentId(
                TEXT("test:concept.ice")),
            3,
            800);

    TestTrue(
        TEXT("Persist absorber Core"),
        Store.UpsertDomainCore(
            CoreA,
            10,
            Error));
    TestTrue(
        TEXT("Persist absorbed Core"),
        Store.UpsertDomainCore(
            CoreB,
            10,
            Error));

    FOGDomainCoreService Service(
        Store);
    TestTrue(
        TEXT("Activate absorber Domain heart"),
        Service.ActivateAwakenedCoreAsHeart(
            CoreAId,
            10,
            Error));
    TestTrue(
        TEXT("Activate absorbed Domain heart"),
        Service.ActivateAwakenedCoreAsHeart(
            CoreBId,
            10,
            Error));

    FOGDomainCoreFusionRequest Request;
    Request.AbsorberCoreId =
        CoreAId;
    Request.AbsorbedCoreId =
        CoreBId;
    Request.SynthesisRuleId =
        FOGContentId(
            TEXT("test:core_synthesis.time_ice"));
    Request.ResolutionSeed =
        0x5120;
    Request.OutcomeKind =
        FName(TEXT("stable_synthesis"));
    Request.InstabilityStateJson =
        TEXT("{\"stability\":\"stable\"}");

    FOGDomainCoreSynthesisConcept Synthesized;
    Synthesized.ConceptId =
        FOGContentId(
            TEXT("test:concept.time_frozen"));
    Synthesized.Grade = 1;
    Synthesized.StateJson =
        TEXT("{\"family\":\"time_freeze\"}");
    Request.SynthesizedConcepts.Add(
        Synthesized);

    FOGDomainCoreFusionRecord Fusion;
    TestTrue(
        TEXT("Fuse Core A absorbing Core B"),
        Service.FuseCores(
            Request,
            20,
            Fusion,
            Error));

    TestTrue(
        TEXT("Asymmetric fusion result remains absorber"),
        Fusion.ResultCoreId ==
            CoreAId);
    TestTrue(
        TEXT("Fusion persists absorber order"),
        Fusion.AbsorberCoreId ==
            CoreAId);
    TestTrue(
        TEXT("Fusion persists absorbed order"),
        Fusion.AbsorbedCoreId ==
            CoreBId);
    TestEqual(
        TEXT("Fusion seed is deterministic provenance"),
        Fusion.ResolutionSeed,
        static_cast<int64>(0x5120));

    bool bCoreFound = false;
    FOGDomainCoreRecord AbsorbedAfter;
    TestTrue(
        TEXT("Read absorbed Core"),
        Store.TryReadDomainCore(
            CoreBId,
            bCoreFound,
            AbsorbedAfter,
            Error));
    TestTrue(
        TEXT("Absorbed Core still exists historically"),
        bCoreFound);
    TestEqual(
        TEXT("Absorbed Core lifecycle is Absorbed, not Broken"),
        AbsorbedAfter.Lifecycle,
        EOGDomainCoreLifecycle::Absorbed);
    TestTrue(
        TEXT("Absorbed Core retains positive durability history"),
        AbsorbedAfter.CurrentDurability.GetSign() >
            0);
    TestFalse(
        TEXT("Absorbed Core has no active controller"),
        AbsorbedAfter.ControllerRulerId.IsValid());

    TArray<FOGDomainCoreConceptRecord> Concepts;
    TestTrue(
        TEXT("Read result Core Concepts"),
        Store.ListDomainCoreConcepts(
            CoreAId,
            Concepts,
            Error));
    TestEqual(
        TEXT("Result preserves both source Concepts plus synthesis"),
        Concepts.Num(),
        3);

    bool bHasTime = false;
    bool bHasIce = false;
    bool bHasSynthesis = false;
    for (const FOGDomainCoreConceptRecord& Concept :
         Concepts)
    {
        bHasTime |=
            Concept.ConceptId ==
                FOGContentId(
                    TEXT("test:concept.time"));
        bHasIce |=
            Concept.ConceptId ==
                FOGContentId(
                    TEXT("test:concept.ice"));

        if (Concept.ConceptId ==
            FOGContentId(
                TEXT("test:concept.time_frozen")))
        {
            bHasSynthesis = true;
            TestTrue(
                TEXT("Synthesized Concept records its rule"),
                Concept.SynthesisRuleId ==
                    Request.SynthesisRuleId);
        }
    }
    TestTrue(
        TEXT("Time Concept is preserved"),
        bHasTime);
    TestTrue(
        TEXT("Ice Concept is preserved"),
        bHasIce);
    TestTrue(
        TEXT("Bounded synthesized Concept is persisted"),
        bHasSynthesis);

    TArray<FOGDomainCoreLineageRecord> Lineage;
    TestTrue(
        TEXT("Read result Core lineage"),
        Store.ListDomainCoreLineage(
            CoreAId,
            Lineage,
            Error));
    TestEqual(
        TEXT("Fusion stores both direct lineage roles"),
        Lineage.Num(),
        2);

    bool bHasAbsorberLineage = false;
    bool bHasAbsorbedLineage = false;
    for (const FOGDomainCoreLineageRecord& Edge :
         Lineage)
    {
        bHasAbsorberLineage |=
            Edge.SourceCoreId ==
                CoreAId &&
            Edge.LineageRole ==
                FName(TEXT("absorber"));
        bHasAbsorbedLineage |=
            Edge.SourceCoreId ==
                CoreBId &&
            Edge.LineageRole ==
                FName(TEXT("absorbed"));
    }
    TestTrue(
        TEXT("Absorber lineage is explicit"),
        bHasAbsorberLineage);
    TestTrue(
        TEXT("Absorbed lineage is explicit"),
        bHasAbsorbedLineage);

    bool bStateFound = false;
    FOGTerritoryDomainStateRecord ResultDomain;
    TestTrue(
        TEXT("Read absorber Domain state"),
        Store.TryReadTerritoryDomainState(
            TerritoryA,
            bStateFound,
            ResultDomain,
            Error));
    TestTrue(
        TEXT("Absorber Domain remains functional"),
        bStateFound &&
        ResultDomain.ActiveCoreId ==
            CoreAId &&
        ResultDomain.DomainState ==
            FName(TEXT("functional")));

    FOGTerritoryDomainStateRecord LostDomain;
    bStateFound = false;
    TestTrue(
        TEXT("Read absorbed Territory Domain state"),
        Store.TryReadTerritoryDomainState(
            TerritoryB,
            bStateFound,
            LostDomain,
            Error));
    TestTrue(
        TEXT("Absorbed heart loss starts Domain ruin"),
        bStateFound &&
        !LostDomain.ActiveCoreId.IsValid() &&
        LostDomain.DomainState ==
            FName(TEXT("heart_lost_ruining")));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGDomainHeartProtectionAndReconstitutionTest,
    "OfflineGame.DomainCore.Heart.ProtectedCoreSurvivesPhysicalDamageAndExceptionalReconstitution",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGDomainHeartProtectionAndReconstitutionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeDomainCoreTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("heart.db"));
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
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    const FOGEntityId TerritoryId =
        FOGEntityId::NewId();
    const FOGEntityId CoreId =
        FOGEntityId::NewId();

    TestTrue(
        TEXT("Persist Ruler"),
        PersistCoreTestRuler(
            Store,
            RulerId,
            Error));
    TestTrue(
        TEXT("Persist Territory"),
        PersistCoreTestTerritory(
            Store,
            RulerId,
            LocationId,
            TerritoryId,
            Error));

    FOGDomainCoreRecord Core =
        MakeAwakenedCore(
            CoreId,
            TerritoryId,
            RulerId,
            FOGContentId(
                TEXT("test:concept.memory")),
            1,
            100);
    TestTrue(
        TEXT("Persist awakened Core"),
        Store.UpsertDomainCore(
            Core,
            10,
            Error));

    FOGDomainCoreService CoreService(
        Store);
    TestTrue(
        TEXT("Activate Core as Domain heart"),
        CoreService.ActivateAwakenedCoreAsHeart(
            CoreId,
            10,
            Error));

    // Physical devastation is a separate axis. This intentionally changes only
    // the physical Territory projection while leaving the protected Core intact.
    FOGTerritoryRecord Devastated;
    bool bTerritoryFound = false;
    TestTrue(
        TEXT("Read Territory"),
        Store.TryReadTerritory(
            TerritoryId,
            bTerritoryFound,
            Devastated,
            Error));
    Devastated.Population = 0;
    Devastated.ControlState =
        FName(TEXT("devastated"));
    TestTrue(
        TEXT("Persist physical devastation"),
        Store.UpsertTerritory(
            Devastated,
            0,
            Error));

    FOGTerritoryDomainStateRecord HeartState;
    TestTrue(
        TEXT("Refresh metaphysical Domain after physical devastation"),
        CoreService.RefreshDomainHeartConsequences(
            TerritoryId,
            20,
            true,
            HeartState,
            Error));
    TestEqual(
        TEXT("Protected intact Core keeps Domain functional"),
        HeartState.DomainState,
        FName(TEXT("functional")));
    TestTrue(
        TEXT("Protected Core remains active heart"),
        HeartState.ActiveCoreId ==
            CoreId);

    TestTrue(
        TEXT("Break active Core"),
        CoreService.ApplyCoreDurabilityDamage(
            CoreId,
            FOGLargeNumber::FromInt64(
                100),
            30,
            Error));

    TestTrue(
        TEXT("Resolve authored ruin threshold"),
        CoreService.RefreshDomainHeartConsequences(
            TerritoryId,
            40,
            true,
            HeartState,
            Error));
    TestEqual(
        TEXT("Lost heart can progress to ruined"),
        HeartState.DomainState,
        FName(TEXT("ruined")));

    FOGProjectService ProjectService(
        Store);
    FOGEntityId RecoveryProjectId;
    TestTrue(
        TEXT("Start separately-authored high-order recovery Project"),
        ProjectService.StartProject(
            RulerId,
            LocationId,
            FOGContentId(
                TEXT("test:project.domain_heart_reconstitution")),
            {},
            50,
            100,
            TEXT("{\"high_order\":true}"),
            RecoveryProjectId,
            Error));

    TestTrue(
        TEXT("Begin exceptional heart reconstitution"),
        CoreService.BeginHeartReconstitution(
            TerritoryId,
            RecoveryProjectId,
            50,
            Error));

    FOGProjectRecord Completed;
    TestTrue(
        TEXT("Complete high-order recovery Project"),
        ProjectService.RefreshProject(
            RecoveryProjectId,
            100,
            Completed,
            Error));
    TestEqual(
        TEXT("Recovery Project is complete"),
        Completed.Status,
        EOGProjectStatus::Completed);

    // The exceptional recovery content restores the same historical Broken
    // Core in this fixture. Ordinary repair never performs this mutation.
    Core.Lifecycle =
        EOGDomainCoreLifecycle::Awakened;
    Core.ControllerRulerId =
        RulerId;
    Core.CurrentDurability =
        FOGLargeNumber::FromInt64(
            100);
    TestTrue(
        TEXT("Persist high-order restored Core"),
        Store.UpsertDomainCore(
            Core,
            100,
            Error));

    TestTrue(
        TEXT("Complete heart reconstitution"),
        CoreService.CompleteHeartReconstitution(
            TerritoryId,
            CoreId,
            100,
            Error));

    bool bStateFound = false;
    FOGTerritoryDomainStateRecord Restored;
    TestTrue(
        TEXT("Read reconstituted Domain heart"),
        Store.TryReadTerritoryDomainState(
            TerritoryId,
            bStateFound,
            Restored,
            Error));
    TestTrue(
        TEXT("Reconstituted Domain is functional"),
        bStateFound &&
        Restored.ActiveCoreId ==
            CoreId &&
        Restored.DomainState ==
            FName(TEXT("functional")));
    TestFalse(
        TEXT("Reconstitution clears lost-heart tick"),
        Restored.bHasHeartLostWorldTick);
    TestFalse(
        TEXT("Reconstitution clears Project link"),
        Restored.ReconstitutionProjectId.IsValid());

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
