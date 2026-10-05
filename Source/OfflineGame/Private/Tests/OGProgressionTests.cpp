#include "Persistence/OGSQLiteWorldStore.h"
#include "Progression/OGCharacterProgressionService.h"
#include "Progression/OGClassRecognitionService.h"
#include "Progression/OGFactorService.h"
#include "Progression/OGGrandConvergenceService.h"
#include "Progression/OGHigherOrderProgressionService.h"
#include "Progression/OGRankService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeProgressionTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(
            EGuidFormats::Digits));
}

bool PersistProgressionEntity(
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

bool PersistManifestation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ManifestationId,
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    const FOGContentId& VersionId,
    int32 AcquisitionOrdinal,
    FName CurrentRarity,
    FString& Error)
{
    FOGCharacterManifestationRecord Manifestation;
    Manifestation.ManifestationId =
        ManifestationId;
    Manifestation.OwningRulerId =
        RulerId;
    Manifestation.IdentityId =
        IdentityId;
    Manifestation.ActiveVersionId =
        VersionId;
    Manifestation.Level = 1;
    Manifestation.CurrentRarity =
        CurrentRarity;
    Manifestation.AcquisitionWorldTick =
        10 + AcquisitionOrdinal;
    Manifestation.AcquisitionOrdinal =
        AcquisitionOrdinal;
    Manifestation.LifecycleState =
        FName(TEXT("active"));

    return Store.UpsertCharacterManifestation(
        Manifestation,
        Manifestation.AcquisitionWorldTick,
        Error);
}

bool SetMaxReinforced(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ManifestationId,
    int64 WorldTick,
    FString& Error)
{
    FOGManifestationReinforcementRecord State;
    State.ManifestationId =
        ManifestationId;
    State.ReinforcementState =
        FName(TEXT("max_reinforced"));
    State.bMaxReinforced = true;
    State.UpdatedWorldTick =
        WorldTick;
    State.StateJson =
        TEXT("{\"validated\":true}");

    return Store.UpsertManifestationReinforcement(
        State,
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGRankAndFactorPersistenceTest,
    "OfflineGame.Progression.RankAndFactors.DataDrivenAndAllFactorsRemainCausal",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGRankAndFactorPersistenceTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeProgressionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("rank_factor.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-12 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema version is 12"),
        Store.GetSchemaVersion(
            Error),
        12);

    const FOGEntityId OwnerId =
        FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist progression owner"),
        PersistProgressionEntity(
            Store,
            OwnerId,
            FName(TEXT("protagonist")),
            Error));

    FOGRankService RankService(
        Store);

    FOGEntityRankStateRecord Rank;
    Rank.EntityId =
        OwnerId;
    Rank.AttainedRankId =
        FOGContentId(
            TEXT("core:rank.saint"));
    Rank.AttainedLevel = 72;
    Rank.EffectiveRankId =
        FOGContentId(
            TEXT("core:rank.sage"));
    Rank.EffectiveLevel = 88;
    Rank.PeakRankId =
        FOGContentId(
            TEXT("core:rank.saint"));
    Rank.PeakLevel = 72;
    Rank.UpdatedWorldTick = 100;

    TestTrue(
        TEXT("Persist data-driven attained/effective Rank"),
        RankService.SetRankState(
            Rank,
            Error));

    bool bRankFound = false;
    FOGResolvedRankProjection ResolvedRank;
    TestTrue(
        TEXT("Resolve current effective Rank"),
        RankService.ResolveEffectiveRank(
            OwnerId,
            bRankFound,
            ResolvedRank,
            Error));
    TestTrue(
        TEXT("Rank state exists"),
        bRankFound);
    TestTrue(
        TEXT("Effective override is used"),
        ResolvedRank.bUsingEffectiveOverride);
    TestTrue(
        TEXT("Rank identity remains a content ID"),
        ResolvedRank.RankId ==
            FOGContentId(
                TEXT("core:rank.sage")));
    TestEqual(
        TEXT("Effective Level is preserved"),
        ResolvedRank.Level,
        88);

    FOGFactorService FactorService(
        Store);

    FOGFactorInstanceRecord Dragon;
    Dragon.FactorInstanceId =
        FOGEntityId::NewId();
    Dragon.OwnerEntityId =
        OwnerId;
    Dragon.FactorId =
        FOGContentId(
            TEXT("test:factor.dragon"));
    Dragon.AcquiredWorldTick = 110;
    Dragon.PurityBps = 8000;
    Dragon.MaturityBps = 3500;
    Dragon.CompletenessBps = 10000;
    Dragon.ExpressionWeightBps = 9000;

    TestTrue(
        TEXT("Acquire first Factor"),
        FactorService.AcquireFactor(
            Dragon,
            {},
            Error));

    FOGFactorInstanceRecord Spatial;
    Spatial.FactorInstanceId =
        FOGEntityId::NewId();
    Spatial.OwnerEntityId =
        OwnerId;
    Spatial.FactorId =
        FOGContentId(
            TEXT("test:factor.spatial"));
    Spatial.AcquiredWorldTick = 120;
    Spatial.PurityBps = 6000;
    Spatial.MaturityBps = 2000;
    Spatial.CompletenessBps = 7500;
    Spatial.ExpressionWeightBps = 1000;

    TestTrue(
        TEXT("Acquire second Factor"),
        FactorService.AcquireFactor(
            Spatial,
            {},
            Error));

    FOGFactorInstanceRecord Hybrid;
    Hybrid.FactorInstanceId =
        FOGEntityId::NewId();
    Hybrid.OwnerEntityId =
        OwnerId;
    Hybrid.FactorId =
        FOGContentId(
            TEXT("test:factor.draconic_spatial"));
    Hybrid.AcquiredWorldTick = 130;
    Hybrid.PurityBps = 7000;
    Hybrid.MaturityBps = 1000;
    Hybrid.CompletenessBps = 9000;
    Hybrid.ExpressionWeightBps = 5000;

    FOGFactorLineageRecord ParentA;
    ParentA.ParentFactorInstanceId =
        Dragon.FactorInstanceId;
    ParentA.RelationKind =
        FName(TEXT("synthesis_parent"));
    ParentA.Ordinal = 0;

    FOGFactorLineageRecord ParentB;
    ParentB.ParentFactorInstanceId =
        Spatial.FactorInstanceId;
    ParentB.RelationKind =
        FName(TEXT("synthesis_parent"));
    ParentB.Ordinal = 1;

    TestTrue(
        TEXT("Acquire synthesized Factor with parent provenance"),
        FactorService.AcquireFactor(
            Hybrid,
            {ParentA, ParentB},
            Error));

    TestTrue(
        TEXT("Expression emphasis may be reduced to zero"),
        FactorService.SetExpressionWeight(
            Spatial.FactorInstanceId,
            0,
            140,
            Error));

    TArray<FOGFactorInstanceRecord> OwnedFactors;
    TestTrue(
        TEXT("List all owned Factors"),
        FactorService.ListOwnedFactors(
            OwnerId,
            OwnedFactors,
            Error));
    TestEqual(
        TEXT("Zero expression does not unequip or delete a Factor"),
        OwnedFactors.Num(),
        3);

    bool bSpatialStillPresent = false;
    for (const FOGFactorInstanceRecord& Factor :
         OwnedFactors)
    {
        if (Factor.FactorInstanceId ==
            Spatial.FactorInstanceId)
        {
            bSpatialStillPresent = true;
            TestEqual(
                TEXT("Spatial Factor keeps zero emphasis only"),
                Factor.ExpressionWeightBps,
                0);
        }
    }
    TestTrue(
        TEXT("Spatial Factor remains causal/persistent"),
        bSpatialStillPresent);

    TArray<FOGFactorLineageRecord> Lineage;
    TestTrue(
        TEXT("Read synthesized Factor lineage"),
        Store.ListFactorLineageForChild(
            Hybrid.FactorInstanceId,
            Lineage,
            Error));
    TestEqual(
        TEXT("Both parent Factors remain in lineage"),
        Lineage.Num(),
        2);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGClassCrownGrandSeatTest,
    "OfflineGame.Progression.Classes.CrownIsNonUniqueGrandSeatIsUnique",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGClassCrownGrandSeatTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeProgressionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("classes.db"));
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

    const FOGEntityId FirstBearer =
        FOGEntityId::NewId();
    const FOGEntityId SecondBearer =
        FOGEntityId::NewId();
    const FOGContentId SwordClass(
        TEXT("test:class.swordmaster"));

    TestTrue(
        TEXT("Persist first candidate"),
        PersistProgressionEntity(
            Store,
            FirstBearer,
            FName(TEXT("character")),
            Error));
    TestTrue(
        TEXT("Persist second candidate"),
        PersistProgressionEntity(
            Store,
            SecondBearer,
            FName(TEXT("character")),
            Error));

    FOGClassRecognitionService Service(
        Store);

    FOGEntityClassRecord FirstCrown;
    FirstCrown.OwnerEntityId =
        FirstBearer;
    FirstCrown.ClassId =
        SwordClass;
    FirstCrown.AttainedTier =
        FName(TEXT("crown"));
    FirstCrown.CurrentExpressionState =
        FName(TEXT("expressible"));
    FirstCrown.RecognizedWorldTick = 10;
    FirstCrown.UpdatedWorldTick = 10;

    FOGEntityClassRecord SecondCrown =
        FirstCrown;
    SecondCrown.OwnerEntityId =
        SecondBearer;
    SecondCrown.RecognizedWorldTick = 20;
    SecondCrown.UpdatedWorldTick = 20;

    TestTrue(
        TEXT("First Crown bearer is recognized"),
        Service.RecognizeClass(
            FirstCrown,
            Error));
    TestTrue(
        TEXT("Second Crown bearer of same Class is also recognized"),
        Service.RecognizeClass(
            SecondCrown,
            Error));

    TArray<FOGEntityClassRecord> FirstClasses;
    TArray<FOGEntityClassRecord> SecondClasses;
    TestTrue(
        TEXT("Read first Crown Classes"),
        Store.ListEntityClasses(
            FirstBearer,
            FirstClasses,
            Error));
    TestTrue(
        TEXT("Read second Crown Classes"),
        Store.ListEntityClasses(
            SecondBearer,
            SecondClasses,
            Error));
    TestEqual(
        TEXT("First candidate retains Crown"),
        FirstClasses.Num(),
        1);
    TestEqual(
        TEXT("Second candidate retains Crown"),
        SecondClasses.Num(),
        1);

    TestTrue(
        TEXT("Appoint first Grand bearer"),
        Service.AppointGrandSeat(
            SwordClass,
            FirstBearer,
            30,
            TEXT("{\"mandate\":\"defensive_order\"}"),
            Error));

    TestFalse(
        TEXT("Second bearer cannot occupy the same active Grand seat"),
        Service.AppointGrandSeat(
            SwordClass,
            SecondBearer,
            31,
            TEXT("{}"),
            Error));

    TestTrue(
        TEXT("Vacate Grand seat explicitly"),
        Service.VacateGrandSeat(
            SwordClass,
            40,
            FName(TEXT("succession")),
            Error));

    TestTrue(
        TEXT("Second Crown may then occupy Grand seat"),
        Service.AppointGrandSeat(
            SwordClass,
            SecondBearer,
            50,
            TEXT("{}"),
            Error));

    bool bSeatFound = false;
    FOGGrandClassSeatRecord Seat;
    TestTrue(
        TEXT("Read unique Grand seat"),
        Store.TryReadGrandClassSeat(
            SwordClass,
            bSeatFound,
            Seat,
            Error));
    TestTrue(
        TEXT("Grand seat exists"),
        bSeatFound);
    TestTrue(
        TEXT("Second bearer holds the unique seat"),
        Seat.BearerEntityId ==
            SecondBearer);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCharacterRoutesAndHigherOrderProgressionTest,
    "OfflineGame.Progression.Character.RoutesFormsTranscendenceAndWorldExpressions",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGCharacterRoutesAndHigherOrderProgressionTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeProgressionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("routes_higher_order.db"));
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
    const FOGEntityId ManifestationId =
        FOGEntityId::NewId();
    const FOGEntityId ProtagonistId =
        FOGEntityId::NewId();
    const FOGContentId IdentityId(
        TEXT("test:character.alpha"));

    TestTrue(
        TEXT("Persist Ruler"),
        PersistProgressionEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist protagonist"),
        PersistProgressionEntity(
            Store,
            ProtagonistId,
            FName(TEXT("protagonist")),
            Error));
    TestTrue(
        TEXT("Persist MR Manifestation"),
        PersistManifestation(
            Store,
            ManifestationId,
            RulerId,
            IdentityId,
            FOGContentId(
                TEXT("test:character.alpha.base")),
            0,
            FName(TEXT("MR")),
            Error));

    FOGCharacterProgressionService CharacterService(
        Store);

    FOGEntitySkillRecord Skill;
    Skill.OwnerEntityId =
        ManifestationId;
    Skill.SkillId =
        FOGContentId(
            TEXT("test:skill.void_cut"));
    Skill.LearnedWorldTick = 20;

    FOGSkillProvenanceRecord SkillSource;
    SkillSource.OwnerEntityId =
        ManifestationId;
    SkillSource.SkillId =
        Skill.SkillId;
    SkillSource.SourceKind =
        FName(TEXT("route"));
    SkillSource.SourceContentOrEntityId =
        TEXT("test:route.awakening/node.void_cut");
    SkillSource.SourceWorldTick = 20;

    TestTrue(
        TEXT("Learn skill with explicit provenance"),
        CharacterService.LearnSkill(
            Skill,
            {SkillSource},
            Error));

    FOGManifestationRouteNodeRecord Evolution;
    Evolution.ManifestationId =
        ManifestationId;
    Evolution.RouteId =
        FOGContentId(
            TEXT("test:route.evolution"));
    Evolution.NodeId =
        FOGContentId(
            TEXT("test:route.evolution/node.1"));
    Evolution.State =
        FName(TEXT("completed"));
    Evolution.bHasEnteredWorldTick = true;
    Evolution.EnteredWorldTick = 21;
    Evolution.bHasCompletedWorldTick = true;
    Evolution.CompletedWorldTick = 30;

    FOGManifestationRouteNodeRecord Corruption =
        Evolution;
    Corruption.RouteId =
        FOGContentId(
            TEXT("test:route.corruption"));
    Corruption.NodeId =
        FOGContentId(
            TEXT("test:route.corruption/node.1"));
    Corruption.EnteredWorldTick = 31;
    Corruption.CompletedWorldTick = 40;

    TestTrue(
        TEXT("Evolution route can be completed"),
        CharacterService.SetRouteNode(
            Evolution,
            Error));
    TestTrue(
        TEXT("Separate Corruption route can coexist"),
        CharacterService.SetRouteNode(
            Corruption,
            Error));

    TArray<FOGManifestationRouteNodeRecord> Nodes;
    TestTrue(
        TEXT("Read open-ended route nodes"),
        Store.ListManifestationRouteNodes(
            ManifestationId,
            Nodes,
            Error));
    TestEqual(
        TEXT("Route families coexist without enum exclusivity"),
        Nodes.Num(),
        2);

    FOGManifestationFormRecord Form;
    Form.ManifestationId =
        ManifestationId;
    Form.FormId =
        FOGContentId(
            TEXT("test:form.void_awakened"));
    Form.State =
        FName(TEXT("unlocked"));
    Form.UnlockedWorldTick = 41;

    TestTrue(
        TEXT("Unlock Manifestation form"),
        CharacterService.UnlockOrUpdateForm(
            Form,
            Error));

    FOGHigherOrderProgressionService HigherOrder(
        Store);

    FOGManifestationWorldFantasmStateRecord Fantasm;
    Fantasm.ManifestationId =
        ManifestationId;
    Fantasm.GradeId =
        FOGContentId(
            TEXT("core:world_fantasm.world_alteration"));
    Fantasm.UnlockedWorldTick = 50;
    Fantasm.EvolutionStateJson =
        TEXT("{\"scale\":\"initial\"}");
    Fantasm.UpdatedWorldTick = 50;

    TestTrue(
        TEXT("Current-Rarity MR Manifestation may unlock validated World Fantasm"),
        HigherOrder.UnlockOrEvolveWorldFantasm(
            Fantasm,
            Error));

    Fantasm.EvolutionStateJson =
        TEXT("{\"scale\":\"developed\"}");
    Fantasm.UpdatedWorldTick = 60;
    TestTrue(
        TEXT("Fantasm expression can evolve without changing base grade"),
        HigherOrder.UnlockOrEvolveWorldFantasm(
            Fantasm,
            Error));

    FOGEntityTranscendenceStateRecord Transcendence;
    Transcendence.EntityId =
        ManifestationId;
    Transcendence.GradeId =
        FOGContentId(
            TEXT("core:transcendence.unbound"));
    Transcendence.BreakthroughWorldTick = 70;
    Transcendence.QualificationSnapshotJson =
        TEXT("{\"qualified\":true}");
    Transcendence.ProofProvenanceJson =
        TEXT("{\"proofs\":[\"test:proof.alpha\"]}");

    TestTrue(
        TEXT("Commit validated Transcendence once"),
        HigherOrder.CommitValidatedTranscendence(
            Transcendence,
            Error));

    FOGEntityTranscendenceStateRecord DifferentGrade =
        Transcendence;
    DifferentGrade.GradeId =
        FOGContentId(
            TEXT("core:transcendence.exalted"));
    DifferentGrade.BreakthroughWorldTick = 80;

    TestFalse(
        TEXT("Ordinary progression cannot redo Transcendence at another grade"),
        HigherOrder.CommitValidatedTranscendence(
            DifferentGrade,
            Error));

    FOGProtagonistWorldManifestationStateRecord PersonalWorld;
    PersonalWorld.OwnerEntityId =
        ProtagonistId;
    PersonalWorld.UnlockedWorldTick = 90;
    PersonalWorld.ExpressionProfileId =
        FOGContentId(
            TEXT("test:world_manifestation.emergent_alpha"));
    PersonalWorld.ProvenanceStateJson =
        TEXT("{\"emergent_from_history\":true}");
    PersonalWorld.EvolutionStateJson =
        TEXT("{\"revision\":1}");
    PersonalWorld.UpdatedWorldTick = 90;

    TestTrue(
        TEXT("Protagonist personal World Manifestation uses no collectible rarity"),
        HigherOrder.UnlockOrEvolveProtagonistWorldManifestation(
            PersonalWorld,
            Error));

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGGrandConvergencePersistenceTest,
    "OfflineGame.Progression.GrandConvergence.PreservesSourcesAndCreatesGrandManifestation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGGrandConvergencePersistenceTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeProgressionTestDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("grand_convergence.db"));
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
    const FOGEntityId SourceA =
        FOGEntityId::NewId();
    const FOGEntityId SourceB =
        FOGEntityId::NewId();
    const FOGEntityId GrandResultId =
        FOGEntityId::NewId();
    const FOGContentId IdentityId(
        TEXT("test:character.convergence"));

    TestTrue(
        TEXT("Persist Ruler"),
        PersistProgressionEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist source A"),
        PersistManifestation(
            Store,
            SourceA,
            RulerId,
            IdentityId,
            FOGContentId(
                TEXT("test:character.convergence.awakened")),
            0,
            FName(TEXT("MR")),
            Error));
    TestTrue(
        TEXT("Persist source B"),
        PersistManifestation(
            Store,
            SourceB,
            RulerId,
            IdentityId,
            FOGContentId(
                TEXT("test:character.convergence.corrupted")),
            1,
            FName(TEXT("MR")),
            Error));

    TestTrue(
        TEXT("Source A is max reinforced"),
        SetMaxReinforced(
            Store,
            SourceA,
            100,
            Error));
    TestTrue(
        TEXT("Source B is max reinforced"),
        SetMaxReinforced(
            Store,
            SourceB,
            100,
            Error));

    FOGCharacterManifestationRecord Result;
    Result.ManifestationId =
        GrandResultId;
    Result.OwningRulerId =
        RulerId;
    Result.IdentityId =
        IdentityId;
    Result.ActiveVersionId =
        FOGContentId(
            TEXT("test:character.convergence.grand"));
    Result.Level = 1;
    Result.CurrentRarity =
        FName(TEXT("MR"));
    Result.AcquisitionWorldTick = 120;
    Result.AcquisitionOrdinal = 2;

    FOGGrandConvergenceService Service(
        Store);
    FOGEntityId ConvergenceId;

    TestTrue(
        TEXT("Perform content-resolved Grand Convergence"),
        Service.PerformConvergence(
            Result,
            {SourceA, SourceB},
            {
                FOGContentId(TEXT("test:lineage.awakening")),
                FOGContentId(TEXT("test:lineage.corruption"))
            },
            FOGContentId(
                TEXT("test:convergence_rule.alpha")),
            120,
            TEXT("{\"synthesis\":\"content_resolved\"}"),
            ConvergenceId,
            Error));
    TestTrue(
        TEXT("Convergence gets a persistent entity ID"),
        ConvergenceId.IsValid());

    bool bFound = false;
    FOGCharacterManifestationRecord SourceAfterA;
    FOGCharacterManifestationRecord SourceAfterB;
    FOGCharacterManifestationRecord GrandAfter;

    TestTrue(
        TEXT("Read source A after Convergence"),
        Store.TryReadCharacterManifestation(
            SourceA,
            bFound,
            SourceAfterA,
            Error));
    TestTrue(
        TEXT("Source A remains historical"),
        bFound);
    TestEqual(
        TEXT("Source A lifecycle becomes converged"),
        SourceAfterA.LifecycleState,
        FName(TEXT("converged")));

    bFound = false;
    TestTrue(
        TEXT("Read source B after Convergence"),
        Store.TryReadCharacterManifestation(
            SourceB,
            bFound,
            SourceAfterB,
            Error));
    TestTrue(
        TEXT("Source B remains historical"),
        bFound);
    TestEqual(
        TEXT("Source B lifecycle becomes converged"),
        SourceAfterB.LifecycleState,
        FName(TEXT("converged")));

    bFound = false;
    TestTrue(
        TEXT("Read Grand Manifestation"),
        Store.TryReadCharacterManifestation(
            GrandResultId,
            bFound,
            GrandAfter,
            Error));
    TestTrue(
        TEXT("Grand Manifestation exists"),
        bFound);
    TestEqual(
        TEXT("Grand Manifestation has Grand lifecycle"),
        GrandAfter.LifecycleState,
        FName(TEXT("grand")));
    TestTrue(
        TEXT("Grand Manifestation keeps canonical Character Identity"),
        GrandAfter.IdentityId ==
            IdentityId);

    TArray<FOGCharacterConvergenceSourceRecord> Sources;
    TestTrue(
        TEXT("Read Convergence source lineage"),
        Store.ListCharacterConvergenceSources(
            ConvergenceId,
            Sources,
            Error));
    TestEqual(
        TEXT("Both divergent source histories remain attached"),
        Sources.Num(),
        2);

    Store.Close();

    FOGSQLiteWorldStore Reopened;
    TestTrue(
        TEXT("Reopen database"),
        Reopened.Open(
            DatabasePath,
            Error));

    TArray<FOGCharacterConvergenceRecord> Convergences;
    TestTrue(
        TEXT("List persisted identity Convergences after restart"),
        Reopened.ListCharacterConvergencesByIdentity(
            IdentityId,
            Convergences,
            Error));
    TestEqual(
        TEXT("Grand Convergence survives restart"),
        Convergences.Num(),
        1);

    Reopened.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
