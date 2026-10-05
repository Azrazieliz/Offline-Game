#include "Persistence/OGSQLiteWorldStore.h"
#include "UI/OGUiViewModelService.h"
#include "World/OGFactionWarService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeUiCompletenessDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool PersistUiCompletenessEntity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& EntityId,
    FName Kind,
    FString& OutError)
{
    return Store.UpsertEntity(
        EntityId,
        Kind,
        0,
        TEXT("{}"),
        OutError);
}

bool PersistUiCompletenessManifestation(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& ManifestationId,
    const FOGEntityId& RulerId,
    const FOGContentId& IdentityId,
    const FOGContentId& VersionId,
    FName Rarity,
    int32 Ordinal,
    FString& OutError)
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
    Manifestation.Level =
        10 + Ordinal;
    Manifestation.CurrentRarity =
        Rarity;
    Manifestation.AcquisitionWorldTick =
        10 + Ordinal;
    Manifestation.AcquisitionOrdinal =
        Ordinal;
    Manifestation.LifecycleState =
        FName(TEXT("active"));
    Manifestation.BuildLabel =
        FString::Printf(
            TEXT("Build %d"),
            Ordinal + 1);

    return Store.UpsertCharacterManifestation(
        Manifestation,
        Manifestation.AcquisitionWorldTick,
        OutError);
}

FOGCombatUnitState MakeUiTurnUnit(
    const FOGEntityId& EntityId,
    int64 NextActionValue)
{
    FOGCombatUnitState Unit;
    Unit.UnitEntityId =
        EntityId;
    Unit.IdentityId =
        FOGContentId(
            FString::Printf(
                TEXT("test:identity.%s"),
                *EntityId.ToString().Left(8)));
    Unit.Presence =
        EOGCombatPresence::Active;
    Unit.CurrentHp =
        FOGLargeNumber::FromInt64(100);
    Unit.Stats.MaxHp =
        Unit.CurrentHp;
    Unit.NextActionValue =
        NextActionValue;
    return Unit;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGUiCharacterCompletenessTest,
    "OfflineGame.PreUnreal.UI.CharacterRosterDetailWardrobeAdultEquipment",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGUiCharacterCompletenessTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiCompletenessDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("character_ui_completeness.db"));
    IFileManager::Get().MakeDirectory(
        *Directory,
        true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(
        TEXT("Open schema-14 database"),
        Store.Open(
            DatabasePath,
            Error));
    TestEqual(
        TEXT("Schema remains 14 before Unreal"),
        Store.GetSchemaVersion(Error),
        14);

    const FOGEntityId RulerId =
        FOGEntityId::NewId();
    const FOGEntityId AlphaA =
        FOGEntityId::NewId();
    const FOGEntityId AlphaB =
        FOGEntityId::NewId();
    const FOGEntityId Beta =
        FOGEntityId::NewId();
    const FOGContentId AlphaIdentity(
        TEXT("test:identity.alpha"));
    const FOGContentId BetaIdentity(
        TEXT("test:identity.beta"));
    const FOGContentId SwordClass(
        TEXT("test:class.sword"));

    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiCompletenessEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist alpha copy A"),
        PersistUiCompletenessManifestation(
            Store,
            AlphaA,
            RulerId,
            AlphaIdentity,
            FOGContentId(TEXT("test:version.alpha_a")),
            FName(TEXT("sr")),
            0,
            Error));
    TestTrue(
        TEXT("Persist alpha copy B"),
        PersistUiCompletenessManifestation(
            Store,
            AlphaB,
            RulerId,
            AlphaIdentity,
            FOGContentId(TEXT("test:version.alpha_b")),
            FName(TEXT("ur")),
            1,
            Error));
    TestTrue(
        TEXT("Persist beta copy"),
        PersistUiCompletenessManifestation(
            Store,
            Beta,
            RulerId,
            BetaIdentity,
            FOGContentId(TEXT("test:version.beta")),
            FName(TEXT("r")),
            0,
            Error));

    FOGEntityClassRecord Class;
    Class.OwnerEntityId =
        AlphaB;
    Class.ClassId =
        SwordClass;
    Class.AttainedTier =
        FName(TEXT("normal"));
    Class.RecognizedWorldTick =
        20;
    Class.UpdatedWorldTick =
        20;
    TestTrue(
        TEXT("Persist selected Manifestation class"),
        Store.UpsertEntityClass(
            Class,
            Error));

    FOGEntityRankStateRecord Rank;
    Rank.EntityId =
        AlphaB;
    Rank.AttainedRankId =
        FOGContentId(TEXT("test:rank.saint"));
    Rank.AttainedLevel = 2;
    Rank.PeakRankId =
        Rank.AttainedRankId;
    Rank.PeakLevel = 2;
    Rank.UpdatedWorldTick = 20;
    TestTrue(
        TEXT("Persist selected Manifestation Rank"),
        Store.UpsertEntityRankState(
            Rank,
            Error));

    FOGManifestationWorldFantasmStateRecord Fantasm;
    Fantasm.ManifestationId =
        AlphaB;
    Fantasm.GradeId =
        FOGContentId(TEXT("test:fantasm.lr"));
    Fantasm.UnlockedWorldTick =
        20;
    Fantasm.UpdatedWorldTick =
        20;
    TestTrue(
        TEXT("Persist selected World Fantasm"),
        Store.UpsertManifestationWorldFantasmState(
            Fantasm,
            Error));

    FOGEntitySkillRecord Skill;
    Skill.OwnerEntityId =
        AlphaB;
    Skill.SkillId =
        FOGContentId(TEXT("test:skill.moon_cut"));
    Skill.LearnedWorldTick =
        20;
    Skill.CurrentState =
        FName(TEXT("integrated"));
    TestTrue(
        TEXT("Persist learned skill"),
        Store.UpsertEntitySkill(
            Skill,
            Error));

    FOGSkillProvenanceRecord Provenance;
    Provenance.OwnerEntityId =
        AlphaB;
    Provenance.SkillId =
        Skill.SkillId;
    Provenance.SourceKind =
        FName(TEXT("class"));
    Provenance.SourceContentOrEntityId =
        SwordClass.ToString();
    Provenance.SourceWorldTick =
        20;
    TestTrue(
        TEXT("Persist skill provenance"),
        Store.UpsertSkillProvenance(
            Provenance,
            Error));

    FOGManifestationRouteNodeRecord Route;
    Route.ManifestationId =
        AlphaB;
    Route.RouteId =
        FOGContentId(TEXT("test:route.awakening"));
    Route.NodeId =
        FOGContentId(TEXT("test:route_node.first"));
    Route.State =
        FName(TEXT("completed"));
    Route.bHasEnteredWorldTick =
        true;
    Route.EnteredWorldTick =
        20;
    Route.bHasCompletedWorldTick =
        true;
    Route.CompletedWorldTick =
        21;
    TestTrue(
        TEXT("Persist known progression node"),
        Store.UpsertManifestationRouteNode(
            Route,
            Error));

    FOGManifestationFormRecord Form;
    Form.ManifestationId =
        AlphaB;
    Form.FormId =
        FOGContentId(TEXT("test:form.awakened"));
    Form.State =
        FName(TEXT("unlocked"));
    Form.UnlockedWorldTick =
        21;
    TestTrue(
        TEXT("Persist form"),
        Store.UpsertManifestationForm(
            Form,
            Error));

    const FOGEntityId ItemId =
        FOGEntityId::NewId();
    FOGItemInstanceRecord Item;
    Item.ItemId =
        ItemId;
    Item.DefinitionId =
        FOGContentId(TEXT("test:item.named_blade"));
    Item.OwnerEntityId =
        AlphaB;
    Item.CurrentRankId =
        FOGContentId(TEXT("test:item_rank.ur"));
    Item.QualityId =
        FOGContentId(TEXT("test:item_quality.ancient"));
    Item.EvolutionStateJson =
        TEXT("{\"stage\":\"awakened\"}");
    Item.HistoryStateJson =
        TEXT("{\"named\":true,\"history\":\"founder_blade\"}");
    TestTrue(
        TEXT("Persist equipment item"),
        Store.UpsertItemInstance(
            Item,
            22,
            Error));

    FOGEquipmentBindingRecord Binding;
    Binding.WearerEntityId =
        AlphaB;
    Binding.SlotId =
        FOGContentId(TEXT("test:body_slot.main_hand"));
    Binding.ItemId =
        ItemId;
    TestTrue(
        TEXT("Persist body-schema equipment binding"),
        Store.UpsertEquipmentBinding(
            Binding,
            Error));

    FOGItemOwnerAffinityRecord Affinity;
    Affinity.ItemId =
        ItemId;
    Affinity.OwnerEntityId =
        AlphaB;
    Affinity.AffinityValue =
        8400;
    Affinity.MilestoneId =
        FOGContentId(TEXT("test:affinity.bonded"));
    Affinity.UpdatedWorldTick =
        22;
    TestTrue(
        TEXT("Persist item affinity milestone"),
        Store.UpsertItemOwnerAffinity(
            Affinity,
            Error));

    FOGEquipmentProficiencyRecord Proficiency;
    Proficiency.OwnerEntityId =
        AlphaB;
    Proficiency.ProficiencyId =
        FOGContentId(TEXT("test:proficiency.sword"));
    Proficiency.ProficiencyValue =
        9200;
    Proficiency.GradeId =
        FOGContentId(TEXT("test:proficiency_grade.master"));
    Proficiency.UpdatedWorldTick =
        22;
    TestTrue(
        TEXT("Persist separate equipment proficiency"),
        Store.UpsertEquipmentProficiency(
            Proficiency,
            Error));

    FOGManifestationPresentationStateRecord Presentation;
    Presentation.ManifestationId =
        AlphaB;
    Presentation.SelectedSkinId =
        FOGContentId(TEXT("test:skin.ceremonial"));
    Presentation.OutfitStateJson =
        TEXT("{\"layers\":[\"robe\",\"belt\"]}");
    Presentation.UpdatedWorldTick =
        22;
    TestTrue(
        TEXT("Persist presentation state"),
        Store.UpsertManifestationPresentationState(
            Presentation,
            Error));

    FOGOwnedPresentationUnlockRecord Unlock;
    Unlock.OwnerEntityId =
        RulerId;
    Unlock.PresentationId =
        Presentation.SelectedSkinId;
    Unlock.AcquiredWorldTick =
        22;
    Unlock.State =
        FName(TEXT("owned"));
    TestTrue(
        TEXT("Persist owned skin"),
        Store.UpsertOwnedPresentationUnlock(
            Unlock,
            Error));

    FOGManifestationContextSelectionRecord LastUsed;
    LastUsed.OwnerEntityId =
        RulerId;
    LastUsed.ContextId =
        FOGUiViewModelService::CharacterLastUsedContextId(
            AlphaIdentity);
    LastUsed.ManifestationId =
        AlphaB;
    LastUsed.UpdatedWorldTick =
        23;
    TestTrue(
        TEXT("Persist last-used alpha copy"),
        Store.UpsertManifestationContextSelection(
            LastUsed,
            Error));

    FOGUiViewModelService Ui(
        Store);

    FOGCharacterIdentityDefinition AlphaDefinition;
    AlphaDefinition.IdentityId =
        AlphaIdentity;
    AlphaDefinition.DisplayNameKey =
        TEXT("test.name.alpha");
    AlphaDefinition.CanonicalMaturity =
        EOGCanonicalMaturity::Adult;

    FOGCharacterIdentityDefinition BetaDefinition;
    BetaDefinition.IdentityId =
        BetaIdentity;
    BetaDefinition.DisplayNameKey =
        TEXT("test.name.beta");
    BetaDefinition.CanonicalMaturity =
        EOGCanonicalMaturity::Unknown;

    FOGRosterQuery Query;
    Query.SearchText =
        TEXT("alpha");
    Query.ClassIds =
    {
        SwordClass
    };
    Query.Rarities =
    {
        FName(TEXT("ur"))
    };
    Query.Columns =
        3;
    Query.SortDimension =
        EOGRosterSortDimension::Rarity;
    Query.RarityOrder =
    {
        FName(TEXT("r")),
        FName(TEXT("sr")),
        FName(TEXT("ur"))
    };

    TArray<FOGRosterIdentityViewModel> Roster;
    TestTrue(
        TEXT("Build searchable/filterable roster"),
        Ui.BuildRosterWithQuery(
            RulerId,
            {
                AlphaDefinition,
                BetaDefinition
            },
            Query,
            Roster,
            Error));
    TestEqual(
        TEXT("Roster filters to one matching Identity"),
        Roster.Num(),
        1);
    if (Roster.Num() == 1)
    {
        TestTrue(
            TEXT("Identity card uses display-name search metadata"),
            Roster[0].DisplayNameKey ==
                AlphaDefinition.DisplayNameKey);
        TestTrue(
            TEXT("Last-used Manifestation remains selected"),
            Roster[0].SelectedManifestationId ==
                AlphaB);
    }

    FOGManifestationDetailViewModel Detail;
    TestTrue(
        TEXT("Build complete Manifestation detail"),
        Ui.BuildManifestationDetail(
            RulerId,
            AlphaB,
            TEXT("moon"),
            12,
            [](const FOGEntityId&,
               FString& OutStats,
               FString& OutResolverError)
            {
                OutResolverError.Reset();
                OutStats =
                    TEXT("{\"hp\":\"12K\",\"attack\":\"3K\"}");
                return true;
            },
            Detail,
            Error));
    TestEqual(
        TEXT("Skill search exposes relevant subset"),
        Detail.Skills.Num(),
        1);
    TestEqual(
        TEXT("Total learned skill count remains separate from visible subset"),
        Detail.TotalLearnedSkillCount,
        1);
    TestEqual(
        TEXT("Known route node is projected"),
        Detail.RouteNodes.Num(),
        1);
    TestTrue(
        TEXT("Unknown route nodes are not fabricated"),
        Detail.bUnknownRouteNodesOmitted);
    TestEqual(
        TEXT("Body-schema equipment is projected"),
        Detail.Equipment.Num(),
        1);
    if (Detail.Equipment.Num() == 1)
    {
        TestTrue(
            TEXT("Item history remains available"),
            Detail.Equipment[0].ItemHistoryStateJson.Contains(
                TEXT("founder_blade")));
        TestTrue(
            TEXT("Affinity defaults to authored milestone"),
            Detail.Equipment[0].AffinityMilestoneId ==
                Affinity.MilestoneId);
    }
    TestEqual(
        TEXT("Proficiency remains separate from affinity"),
        Detail.Proficiencies.Num(),
        1);
    TestFalse(
        TEXT("Internal formulas remain hidden"),
        Detail.bInternalFormulaBreakdownVisible);

    FOGManifestationComparisonViewModel Comparison;
    TestTrue(
        TEXT("Compare two same-Identity Manifestations side by side"),
        Ui.BuildManifestationComparison(
            RulerId,
            AlphaA,
            AlphaB,
            12,
            [](const FOGEntityId& ManifestationId,
               FString& OutStats,
               FString& OutResolverError)
            {
                OutResolverError.Reset();
                OutStats =
                    FString::Printf(
                        TEXT("{\"manifestation\":\"%s\"}"),
                        *ManifestationId.ToString());
                return true;
            },
            Comparison,
            Error));
    TestTrue(
        TEXT("Comparison stays within one Character Identity"),
        Comparison.IdentityId ==
            AlphaIdentity);
    TestTrue(
        TEXT("Comparison remains side-by-side"),
        Comparison.bSideBySide);

    FOGWardrobeViewModel Wardrobe;
    TestTrue(
        TEXT("Build wardrobe with body/form compatibility resolver"),
        Ui.BuildWardrobe(
            RulerId,
            AlphaB,
            [&Presentation](
                const FOGEntityId&,
                const FOGContentId& PresentationId,
                bool& bCompatible,
                FString& OutResolverError)
            {
                OutResolverError.Reset();
                bCompatible =
                    PresentationId ==
                        Presentation.SelectedSkinId;
                return true;
            },
            Wardrobe,
            Error));
    TestEqual(
        TEXT("Owned presentation is available"),
        Wardrobe.OwnedPresentations.Num(),
        1);
    TestTrue(
        TEXT("Selected Skin remains cosmetic presentation state"),
        Wardrobe.SelectedSkinId ==
            Presentation.SelectedSkinId);
    TestEqual(
        TEXT("Actual equipped item remains visible with outfit"),
        Wardrobe.VisibleEquipment.Num(),
        1);
    TestTrue(
        TEXT("Wardrobe remains utility surface"),
        Wardrobe.bWardrobeIsUtilityNotBottomTab);

    FOGCharacterAdultRuntimeStateRecord AdultRuntime;
    AdultRuntime.CharacterEntityId =
        AlphaB;
    AdultRuntime.CurrentProfileVariantId =
        FOGContentId(TEXT("test:adult_profile.default"));
    AdultRuntime.MutableContextStateJson =
        TEXT("{\"libido_value\":73,\"preference\":\"private\"}");
    AdultRuntime.UpdatedWorldTick =
        24;
    TestTrue(
        TEXT("Persist mutable Adult runtime expression"),
        Store.UpsertCharacterAdultRuntimeState(
            AdultRuntime,
            Error));

    FOGWorldEvent AdultHistory;
    AdultHistory.EventId =
        FOGEntityId::NewId();
    AdultHistory.EventType =
        FName(TEXT("character.major_relationship_event"));
    AdultHistory.WorldTick =
        25;
    AdultHistory.PrimaryEntity =
        AlphaB;
    AdultHistory.bChronicleEligible =
        true;
    AdultHistory.PayloadJson =
        TEXT("{\"importance\":\"major\",\"presentation\":\"archive\"}");
    TestTrue(
        TEXT("Persist meaningful adult archive event"),
        Store.AppendWorldEvent(
            AdultHistory,
            Error));

    FOGAdultUtilityViewModel Adult;
    TestTrue(
        TEXT("Build Adult utility for canonical Adult Identity"),
        Ui.BuildAdultUtility(
            AlphaDefinition,
            AlphaB,
            false,
            {
                FOGContentId(TEXT("test:adult_interaction.contextual"))
            },
            Adult,
            Error));
    TestTrue(
        TEXT("Adult utility is always accessible for canonical Adult Identity"),
        Adult.bVisible);
    TestFalse(
        TEXT("Adult utility does not introduce relationship access gate"),
        Adult.bRelationshipGatePresent);
    TestTrue(
        TEXT("Fast Privacy/SFW toggle is available"),
        Adult.bFastPrivacySfwToggleAvailable);
    TestEqual(
        TEXT("Adult utility exposes Profile/Interaction/Systemic/Archive sections"),
        Adult.Sections.Num(),
        4);
    TestTrue(
        TEXT("Mutable adult profile data remains inspectable"),
        Adult.MutableContextStateJson.Contains(
            TEXT("libido_value")));
    TestEqual(
        TEXT("Meaningful history is available for replay/archive"),
        Adult.ArchiveEvents.Num(),
        1);

    FOGAdultUtilityViewModel NonAdult;
    BetaDefinition.CanonicalMaturity =
        EOGCanonicalMaturity::NonAdult;
    TestTrue(
        TEXT("Build nonadult utility projection safely"),
        Ui.BuildAdultUtility(
            BetaDefinition,
            Beta,
            false,
            {},
            NonAdult,
            Error));
    TestFalse(
        TEXT("NonAdult Identity never receives Adult utility"),
        NonAdult.bVisible);

    FOGEquipmentComparisonViewModel EquipmentComparison;
    TestTrue(
        TEXT("Build equipment comparison from resolved deltas"),
        FOGUiViewModelService::BuildEquipmentComparison(
            ItemId,
            FOGEntityId::NewId(),
            TEXT("{\"attack_delta\":\"+800\"}"),
            {
                FOGContentId(TEXT("test:skill.gained"))
            },
            {
                FOGContentId(TEXT("test:skill.lost"))
            },
            {
                TEXT("requires awakened form")
            },
            TEXT("test.affinity.implication"),
            TEXT("test.proficiency.implication"),
            EquipmentComparison,
            Error));
    TestFalse(
        TEXT("Equipment comparison never exposes implementation formula decomposition"),
        EquipmentComparison.bInternalFormulaBreakdownVisible);

    const FOGCraftingViewModel Crafting =
        FOGUiViewModelService::BuildCraftingShell();
    TestEqual(
        TEXT("Crafting exposes Known and Experiment modes"),
        Crafting.Modes.Num(),
        2);
    TestEqual(
        TEXT("Experiment exposes authored invention dimensions"),
        Crafting.ExperimentFields.Num(),
        5);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGUiRecordsGachaTerritoryCombatCompletenessTest,
    "OfflineGame.PreUnreal.UI.GachaRecordsTerritoryWorldHudTurnBattle",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGUiRecordsGachaTerritoryCombatCompletenessTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiCompletenessDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("records_combat_ui.db"));
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
    const FOGEntityId SubjectId =
        FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiCompletenessEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist intelligence subject"),
        PersistUiCompletenessEntity(
            Store,
            SubjectId,
            FName(TEXT("boss")),
            Error));

    FOGGachaBannerDefinition Banner;
    Banner.BannerId =
        FOGContentId(TEXT("test:banner.limited"));
    Banner.PityCategory =
        FName(TEXT("limited_character"));
    Banner.CurrencyId =
        FOGContentId(TEXT("test:currency.pull"));
    Banner.PullCost =
        100;
    Banner.CompatibleTicketIds =
    {
        FOGContentId(TEXT("test:ticket.limited"))
    };
    Banner.TopRarity =
        FName(TEXT("ur"));
    Banner.SoftPityStart =
        50;
    Banner.SoftPityBonusPerPullBps =
        400;
    Banner.HardPity =
        70;
    Banner.bFeaturedGuaranteeAfterMiss =
        true;

    FOGGachaPoolEntry Featured;
    Featured.IdentityId =
        FOGContentId(TEXT("test:identity.featured"));
    Featured.VersionId =
        FOGContentId(TEXT("test:version.featured"));
    Featured.Rarity =
        FName(TEXT("ur"));
    Featured.Weight =
        10;
    Featured.bFeatured =
        true;
    Banner.Entries.Add(
        Featured);

    FOGGachaPoolEntry Standard;
    Standard.IdentityId =
        FOGContentId(TEXT("test:identity.standard"));
    Standard.VersionId =
        FOGContentId(TEXT("test:version.standard"));
    Standard.Rarity =
        FName(TEXT("sr"));
    Standard.Weight =
        90;
    Banner.Entries.Add(
        Standard);

    FOGUiViewModelService Ui(
        Store);
    FOGGachaDetailsViewModel Details;
    TestTrue(
        TEXT("Build complete gacha Details projection"),
        Ui.BuildGachaDetails(
            Banner,
            {
                Featured.IdentityId
            },
            TEXT("{\"designation\":\"authored_selector\"}"),
            Details,
            Error));
    TestEqual(
        TEXT("Carry category is visible"),
        Details.CarryCategory,
        Banner.PityCategory);
    TestEqual(
        TEXT("Exact declared base probabilities are listed"),
        Details.DeclaredProbabilities.Num(),
        2);
    TestEqual(
        TEXT("Compatible tickets are explained"),
        Details.CompatibleTicketIds.Num(),
        1);
    TestEqual(
        TEXT("Authored target designation is exposed only for eligible Identity"),
        Details.AuthoredDesignationOptions.Num(),
        1);

    FOGWorldEvent PullEvent;
    PullEvent.EventId =
        FOGEntityId::NewId();
    PullEvent.EventType =
        FName(TEXT("gacha_pull"));
    PullEvent.WorldTick =
        30;
    PullEvent.PrimaryEntity =
        RulerId;
    PullEvent.PayloadJson =
        TEXT("{\"banner\":\"test:banner.limited\",\"identity\":\"test:identity.featured\",\"version\":\"test:version.featured\",\"rarity\":\"ur\",\"featured\":true,\"duplicate\":false,\"payment_resource\":\"test:ticket.limited\",\"used_ticket\":true}");
    TestTrue(
        TEXT("Persist gacha history event"),
        Store.AppendWorldEvent(
            PullEvent,
            Error));

    FOGGachaHistoryFilter HistoryFilter;
    HistoryFilter.BannerId =
        Banner.BannerId;
    HistoryFilter.IdentityId =
        Featured.IdentityId;
    HistoryFilter.Rarity =
        FName(TEXT("ur"));
    HistoryFilter.bUseWorldTickRange =
        true;
    HistoryFilter.MinWorldTick =
        20;
    HistoryFilter.MaxWorldTick =
        40;

    TArray<FOGGachaHistoryEntryViewModel> History;
    TestTrue(
        TEXT("Filter pull history by banner Identity rarity and date/tick range"),
        Ui.BuildGachaHistoryFiltered(
            RulerId,
            HistoryFilter,
            50,
            History,
            Error));
    TestEqual(
        TEXT("Matching pull history is returned"),
        History.Num(),
        1);

    FOGWorldEvent ChronicleEvent;
    ChronicleEvent.EventId =
        FOGEntityId::NewId();
    ChronicleEvent.EventType =
        FName(TEXT("boss.defeated"));
    ChronicleEvent.WorldTick =
        40;
    ChronicleEvent.PrimaryEntity =
        SubjectId;
    ChronicleEvent.RelatedEntities.Add(
        RulerId);
    ChronicleEvent.bChronicleEligible =
        true;
    ChronicleEvent.PayloadJson =
        TEXT("{\"importance\":\"major\"}");
    TestTrue(
        TEXT("Persist meaningful Chronicle event"),
        Store.AppendWorldEvent(
            ChronicleEvent,
            Error));

    FOGChronicleFilter ChronicleFilter;
    ChronicleFilter.EntityId =
        SubjectId;
    ChronicleFilter.EventTypes =
    {
        FName(TEXT("boss.defeated"))
    };
    ChronicleFilter.ImportanceLevels =
    {
        FName(TEXT("major"))
    };
    ChronicleFilter.Limit =
        20;

    TArray<FOGChronicleEntryViewModel> Chronicle;
    TestTrue(
        TEXT("Build filterable timeline Chronicle"),
        Ui.BuildChronicleFiltered(
            RulerId,
            ChronicleFilter,
            Chronicle,
            Error));
    TestEqual(
        TEXT("Chronicle filters event type/entity/importance"),
        Chronicle.Num(),
        1);

    FOGKnowledgeFactRecord Intel;
    Intel.OwnerEntityId =
        RulerId;
    Intel.FactKey =
        FName(TEXT("boss_phase"));
    Intel.SubjectEntityId =
        SubjectId;
    Intel.ValueJson =
        TEXT("{\"phase\":2}");
    Intel.LearnedWorldTick =
        41;
    Intel.UpdatedWorldTick =
        41;
    Intel.BeliefState =
        FName(TEXT("rumor"));
    Intel.ConfidenceBps =
        3000;
    Intel.SourceEventId =
        ChronicleEvent.EventId;
    TestTrue(
        TEXT("Persist knowledge-limited Intelligence"),
        Store.UpsertKnowledgeFact(
            Intel,
            Error));

    FOGIntelligenceFilter IntelFilter;
    IntelFilter.SubjectEntityId =
        SubjectId;
    IntelFilter.States =
    {
        EOGUiKnowledgeState::Rumor
    };

    TArray<FOGIntelligenceEntryViewModel> Intelligence;
    TestTrue(
        TEXT("Build filtered Intelligence"),
        Ui.BuildIntelligenceFiltered(
            RulerId,
            IntelFilter,
            Intelligence,
            Error));
    TestEqual(
        TEXT("Rumor is distinguished"),
        Intelligence.Num(),
        1);
    if (Intelligence.Num() == 1)
    {
        TestEqual(
            TEXT("Rumor state remains explicit"),
            Intelligence[0].Knowledge.KnowledgeState,
            EOGUiKnowledgeState::Rumor);
        TestTrue(
            TEXT("Provenance remains available"),
            Intelligence[0].Knowledge.SourceEventId ==
                ChronicleEvent.EventId);
    }

    FOGCodexEntryViewModel CharacterCodex;
    CharacterCodex.EntryId =
        Featured.IdentityId;
    CharacterCodex.Category =
        FName(TEXT("character_identities"));
    CharacterCodex.DisplayNameKey =
        TEXT("test.name.featured");
    CharacterCodex.KnowledgeState =
        EOGUiKnowledgeState::Confirmed;

    FOGCodexEntryViewModel ItemCodex;
    ItemCodex.EntryId =
        FOGContentId(TEXT("test:item.codex"));
    ItemCodex.Category =
        FName(TEXT("items"));
    ItemCodex.DisplayNameKey =
        TEXT("test.name.item");
    ItemCodex.KnowledgeState =
        EOGUiKnowledgeState::Estimated;

    const FOGCodexViewModel Codex =
        FOGUiViewModelService::BuildCodex(
            {
                CharacterCodex,
                ItemCodex
            },
            TEXT("featured"),
            FName(TEXT("character_identities")));
    TestEqual(
        TEXT("Codex search/category filtering works"),
        Codex.VisibleEntries.Num(),
        1);
    TestEqual(
        TEXT("Codex exposes frozen searchable categories"),
        Codex.Categories.Num(),
        8);

    FOGTerritoryViewModel Territory;
    Territory.AnalyticalOverlays =
    {
        FName(TEXT("control")),
        FName(TEXT("threat")),
        FName(TEXT("resources"))
    };
    TestTrue(
        TEXT("Activate one Territory overlay"),
        FOGUiViewModelService::SetActiveTerritoryOverlay(
            Territory,
            FName(TEXT("control")),
            Error));
    TestTrue(
        TEXT("Switching overlay replaces previous overlay"),
        FOGUiViewModelService::SetActiveTerritoryOverlay(
            Territory,
            FName(TEXT("threat")),
            Error));
    TestEqual(
        TEXT("Exactly one major overlay is active"),
        Territory.ActiveOverlay,
        FName(TEXT("threat")));

    FOGKnowledgeFactRecord ConfirmedHp;
    ConfirmedHp.OwnerEntityId =
        RulerId;
    ConfirmedHp.FactKey =
        FName(TEXT("target_hp"));
    ConfirmedHp.SubjectEntityId =
        SubjectId;
    ConfirmedHp.BeliefState =
        FName(TEXT("confirmed"));
    ConfirmedHp.ConfidenceBps =
        10000;

    FOGHudStatusEffectInput Status;
    Status.EffectId =
        FOGContentId(TEXT("test:effect.bleed"));
    Status.Stacks =
        3;
    Status.RemainingTurns =
        2;
    Status.bPreciseKnowledge =
        true;
    Status.PreciseEffectTextKey =
        TEXT("test.effect.bleed.detail");

    FOGWorldTargetViewModel Target;
    TestTrue(
        TEXT("Build principal target panel"),
        FOGUiViewModelService::BuildWorldTarget(
            SubjectId,
            FOGLargeNumber::FromInt64(6200000000LL),
            &ConfirmedHp,
            &Intel,
            nullptr,
            {
                Status
            },
            Target,
            Error));
    TestTrue(
        TEXT("Confirmed HP uses compact suffix presentation"),
        Target.HpDisplay.Contains(TEXT("B")));
    TestEqual(
        TEXT("Status stack/turn presentation is compact"),
        Target.StatusEffects.Num(),
        1);
    TestTrue(
        TEXT("Tap opens precise known status information"),
        Target.StatusEffects[0].bTapForDetails &&
        Target.StatusEffects[0].bPreciseDetailsVisible);

    const FOGEntityId UnitA =
        FOGEntityId::NewId();
    const FOGEntityId UnitB =
        FOGEntityId::NewId();

    FOGTurnBattleState Battle;
    Battle.BattleId =
        FOGEntityId::NewId();
    Battle.Status =
        EOGTurnBattleStatus::Running;
    Battle.Units =
    {
        MakeUiTurnUnit(UnitA, 200),
        MakeUiTurnUnit(UnitB, 100)
    };

    FOGTurnTimelineEntryViewModel Interrupt;
    Interrupt.EntityId =
        UnitA;
    Interrupt.ActionValue =
        75;
    Interrupt.Marker =
        FName(TEXT("interrupt_insert"));

    FOGTurnBattleRecapEntryViewModel Recap;
    Recap.EntityId =
        UnitA;
    Recap.DirectDamage =
        FOGLargeNumber::FromInt64(1000);
    Recap.DotDamage =
        FOGLargeNumber::FromInt64(200);
    Recap.TotalHealing =
        FOGLargeNumber::FromInt64(300);

    FOGTurnBattlePresentationViewModel TurnUi;
    TestTrue(
        TEXT("Build frozen turn-battle presentation"),
        FOGUiViewModelService::BuildTurnBattlePresentation(
            Battle,
            3.0f,
            false,
            {
                Interrupt
            },
            {
                Recap
            },
            TurnUi,
            Error));
    TestEqual(
        TEXT("Turn battle supports exactly 1x/2x/3x speed choices"),
        TurnUi.SpeedOptions.Num(),
        3);
    TestEqual(
        TEXT("3x speed is supported"),
        TurnUi.SelectedSpeed,
        3.0f);
    TestFalse(
        TEXT("Ultimate/cinematic preference is independent from speed"),
        TurnUi.bUltimateCinematicsEnabled);
    TestTrue(
        TEXT("Timeline includes actor and interrupt entries"),
        TurnUi.Timeline.Num() >= 3);
    TestTrue(
        TEXT("Advanced conditional auto editor is available"),
        TurnUi.bAdvancedConditionalRuleEditorAvailable);
    TestEqual(
        TEXT("Normal recap remains per-character and compact"),
        TurnUi.Recap.Num(),
        1);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGUiRecoveryPackageStrategyCompletenessTest,
    "OfflineGame.PreUnreal.UI.RecoveryPackageProjectDispatchWarRisk",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGUiRecoveryPackageStrategyCompletenessTest::RunTest(
    const FString& Parameters)
{
    const FString Directory =
        MakeUiCompletenessDirectory();
    const FString DatabasePath =
        FPaths::Combine(
            Directory,
            TEXT("strategy_ui_completeness.db"));
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
    const FOGEntityId SpecialistId =
        FOGEntityId::NewId();
    const FOGEntityId LocationId =
        FOGEntityId::NewId();
    TestTrue(
        TEXT("Persist Ruler"),
        PersistUiCompletenessEntity(
            Store,
            RulerId,
            FName(TEXT("ruler")),
            Error));
    TestTrue(
        TEXT("Persist specialist"),
        PersistUiCompletenessEntity(
            Store,
            SpecialistId,
            FName(TEXT("character")),
            Error));

    FOGLocationRecord Location;
    Location.LocationId =
        LocationId;
    Location.Kind =
        FName(TEXT("region"));
    TestTrue(
        TEXT("Persist strategy location"),
        Store.UpsertLocation(
            Location,
            0,
            Error));

    FOGContentPackageRecord Package;
    Package.PackageId =
        FOGContentId(TEXT("test:package.world_region"));
    Package.Version =
        3;
    Package.ContentHash =
        TEXT("hash-v3");
    Package.bInstalled =
        true;
    Package.bValidated =
        true;
    Package.Category =
        FName(TEXT("world_region"));
    Package.InstallUri =
        TEXT("file:///storage/test/world_region.pak");
    Package.StorageClass =
        FName(TEXT("external"));
    Package.DownloadState =
        FName(TEXT("installed"));
    TestTrue(
        TEXT("Persist package"),
        Store.UpsertContentPackageRecord(
            Package,
            Error));

    FOGUiViewModelService Ui(
        Store);
    FOGPackageStorageViewModel Storage;
    TestTrue(
        TEXT("Build package/storage manager projection"),
        Ui.BuildPackageStorage(
            [](const FOGContentPackageRecord&,
               bool& bKnown,
               int64& SizeBytes,
               FString& OutResolverError)
            {
                OutResolverError.Reset();
                bKnown =
                    true;
                SizeBytes =
                    734003200;
                return true;
            },
            [](const FOGContentPackageRecord& PackageRecord,
               bool& bMove,
               bool& bArchive,
               FString& OutResolverError)
            {
                OutResolverError.Reset();
                bMove =
                    PackageRecord.StorageClass ==
                        FName(TEXT("external"));
                bArchive =
                    true;
                return true;
            },
            Storage,
            Error));
    TestEqual(
        TEXT("Installed package is listed"),
        Storage.Packages.Num(),
        1);
    if (Storage.Packages.Num() == 1)
    {
        TestTrue(
            TEXT("Package size is exposed"),
            Storage.Packages[0].bSizeKnown &&
            Storage.Packages[0].SizeBytes ==
                734003200);
        TestTrue(
            TEXT("Move is exposed only when storage resolver allows it"),
            Storage.Packages[0].Actions.Contains(
                FName(TEXT("move"))));
        TestTrue(
            TEXT("Archive is exposed when storage resolver allows it"),
            Storage.Packages[0].Actions.Contains(
                FName(TEXT("archive"))));
    }
    TestTrue(
        TEXT("Package manager allows automatic metadata checks"),
        Storage.bAutomaticMetadataChecksAllowed);
    TestTrue(
        TEXT("Large downloads default to unmetered/Wi-Fi policy"),
        Storage.bLargeDownloadsDefaultUnmeteredOnly);

    FOGBackupCatalogEntry Automatic;
    Automatic.BackupId =
        TEXT("WorldSnapshot_20261005_120000");
    Automatic.BackupPathOrUri =
        TEXT("/backup/auto.db");
    Automatic.SchemaVersion =
        14;
    Automatic.WorldIdentity =
        TEXT("test:world");
    Automatic.CreatedUtc =
        TEXT("2026-10-05T12:00:00Z");
    Automatic.ContentHash =
        TEXT("hash-auto");
    Automatic.SourceBuildVersion =
        TEXT("test-build");
    Automatic.ValidationState =
        FName(TEXT("validated"));

    FOGBackupCatalogEntry Manual =
        Automatic;
    Manual.BackupId =
        TEXT("manual-before-boss");
    Manual.BackupPathOrUri =
        TEXT("/backup/manual.db");

    const FOGBackupManagerViewModel Backups =
        FOGUiViewModelService::BuildBackupManager(
            {
                Automatic,
                Manual
            });
    TestEqual(
        TEXT("Automatic and manual snapshots are visible"),
        Backups.Backups.Num(),
        2);
    TestEqual(
        TEXT("Automatic snapshot kind is recognized"),
        Backups.Backups[0].SnapshotKind,
        FName(TEXT("automatic")));
    TestEqual(
        TEXT("Manual snapshot kind is recognized"),
        Backups.Backups[1].SnapshotKind,
        FName(TEXT("manual")));
    TestTrue(
        TEXT("Backup manager exposes import"),
        Backups.GlobalActions.Contains(
            FName(TEXT("import_backup"))));

    FOGProjectRecord Project;
    Project.ProjectId =
        FOGEntityId::NewId();
    Project.OwnerEntityId =
        RulerId;
    Project.LocationId =
        LocationId;
    Project.ProjectTypeId =
        FOGContentId(TEXT("test:project.gate"));
    Project.Status =
        EOGProjectStatus::Active;
    Project.StartWorldTick =
        10;
    Project.ResolveWorldTick =
        30;
    Project.ProgressBps =
        5000;
    TestTrue(
        TEXT("Persist Project"),
        Store.UpsertProject(
            Project,
            10,
            Error));

    FOGProjectPhaseRecord Phase;
    Phase.ProjectId =
        Project.ProjectId;
    Phase.PhaseId =
        FOGContentId(TEXT("test:phase.analysis"));
    Phase.Sequence =
        0;
    Phase.Status =
        FName(TEXT("active"));
    Phase.StartWorldTick =
        10;
    Phase.ResolveWorldTick =
        20;
    Phase.ProgressBps =
        7500;
    TestTrue(
        TEXT("Persist Project phase"),
        Store.UpsertProjectPhase(
            Phase,
            Error));

    FOGProjectAssignmentRecord Assignment;
    Assignment.ProjectId =
        Project.ProjectId;
    Assignment.AssigneeEntityId =
        SpecialistId;
    Assignment.RoleId =
        FOGContentId(TEXT("test:role.specialist"));
    TestTrue(
        TEXT("Persist named Project assignment"),
        Store.UpsertProjectAssignment(
            Assignment,
            Error));

    FOGProjectViewModel ProjectUi;
    TestTrue(
        TEXT("Build Project UI without worker-pawn simulation"),
        Ui.BuildProject(
            Project.ProjectId,
            ProjectUi,
            Error));
    TestEqual(
        TEXT("Project exposes authored phase"),
        ProjectUi.PhaseIds.Num(),
        1);
    TestEqual(
        TEXT("Project exposes named specialist assignment"),
        ProjectUi.NamedAssigneeIds.Num(),
        1);

    FOGDispatchRecord Dispatch;
    Dispatch.DispatchId =
        FOGEntityId::NewId();
    Dispatch.OwnerEntityId =
        RulerId;
    Dispatch.TargetEntityId =
        LocationId;
    Dispatch.Type =
        EOGDispatchType::Support;
    Dispatch.Status =
        EOGDispatchStatus::Active;
    Dispatch.ParticipantEntityIds =
    {
        SpecialistId
    };
    Dispatch.StartWorldTick =
        20;
    Dispatch.ResolveWorldTick =
        40;
    Dispatch.RiskBps =
        6200;
    Dispatch.RiskToleranceBps =
        5000;
    Dispatch.AbortPolicyJson =
        TEXT("{\"abort_if\":\"survival\"}");
    TestTrue(
        TEXT("Persist Dispatch"),
        Store.UpsertDispatch(
            Dispatch,
            20,
            Error));

    FOGDispatchObjectiveRecord Objective;
    Objective.DispatchId =
        Dispatch.DispatchId;
    Objective.ObjectiveId =
        FOGContentId(TEXT("test:objective.rescue"));
    Objective.Priority =
        100;
    Objective.bMandatory =
        true;
    Objective.TargetEntityId =
        SpecialistId;
    TestTrue(
        TEXT("Persist mandatory Dispatch objective"),
        Store.UpsertDispatchObjective(
            Objective,
            Error));

    FOGDispatchConstraintRecord Constraint;
    Constraint.DispatchId =
        Dispatch.DispatchId;
    Constraint.ConstraintId =
        FOGContentId(TEXT("test:constraint.survival"));
    TestTrue(
        TEXT("Persist Dispatch constraint"),
        Store.UpsertDispatchConstraint(
            Constraint,
            Error));

    const TArray<int32> AuthoredRiskThresholds =
    {
        2000,
        5000,
        8000
    };

    FOGDispatchViewModel DispatchUi;
    TestTrue(
        TEXT("Build knowledge-aware Dispatch UI"),
        Ui.BuildDispatch(
            Dispatch.DispatchId,
            EOGUiKnowledgeState::Estimated,
            false,
            AuthoredRiskThresholds,
            DispatchUi,
            Error));
    TestEqual(
        TEXT("Mandatory objective remains explicit"),
        DispatchUi.MandatoryObjectiveIds.Num(),
        1);
    TestFalse(
        TEXT("Estimated risk never invents exact probability"),
        DispatchUi.Risk.bExactProbabilityVisible);
    TestEqual(
        TEXT("Risk band uses caller-authored tuning thresholds"),
        DispatchUi.Risk.Band,
        EOGUiRiskBand::High);

    FOGFactionRecord Friendly;
    Friendly.FactionId =
        FOGEntityId::NewId();
    Friendly.LeaderRulerId =
        RulerId;
    Friendly.Kind =
        FName(TEXT("friendly"));
    Friendly.Population =
        100;

    FOGFactionRecord Enemy;
    Enemy.FactionId =
        FOGEntityId::NewId();
    Enemy.Kind =
        FName(TEXT("enemy"));
    Enemy.Population =
        100;

    TestTrue(
        TEXT("Persist friendly faction"),
        Store.UpsertFaction(
            Friendly,
            50,
            Error));
    TestTrue(
        TEXT("Persist enemy faction"),
        Store.UpsertFaction(
            Enemy,
            50,
            Error));

    FOGFactionWarService Wars(
        Store);
    FOGEntityId WarId;
    TestTrue(
        TEXT("Declare War"),
        Wars.DeclareWar(
            Friendly.FactionId,
            Enemy.FactionId,
            FName(TEXT("defend")),
            LocationId,
            50,
            WarId,
            Error));

    FOGEntityId FrontId;
    TestTrue(
        TEXT("Create War front"),
        Wars.CreateWarFront(
            WarId,
            LocationId,
            FOGEntityId(),
            51,
            TEXT("{}"),
            FrontId,
            Error));

    FOGWarObjectiveRecord WarObjective;
    WarObjective.WarId =
        WarId;
    WarObjective.FrontId =
        FrontId;
    WarObjective.ObjectiveId =
        FOGContentId(TEXT("test:war_objective.hold"));
    WarObjective.TargetEntityId =
        LocationId;
    WarObjective.Priority =
        100;
    WarObjective.Status =
        FName(TEXT("active"));
    TestTrue(
        TEXT("Persist War objective"),
        Wars.SetWarObjective(
            WarObjective,
            Error));

    FOGEntityId OrderId;
    TestTrue(
        TEXT("Issue War order"),
        Wars.IssueWarOrder(
            WarId,
            FrontId,
            RulerId,
            SpecialistId,
            FOGContentId(TEXT("test:war_intent.hold")),
            TEXT("{\"avoid_excessive_losses\":true}"),
            52,
            OrderId,
            Error));

    FOGWarViewModel WarUi;
    TestTrue(
        TEXT("Build knowledge-aware War UI"),
        Ui.BuildWar(
            WarId,
            EOGUiKnowledgeState::Rumor,
            7000,
            false,
            AuthoredRiskThresholds,
            WarUi,
            Error));
    TestEqual(
        TEXT("War front is visible"),
        WarUi.FrontIds.Num(),
        1);
    TestEqual(
        TEXT("War objective is visible"),
        WarUi.ObjectiveIds.Num(),
        1);
    TestEqual(
        TEXT("War intent is visible without rewriting order"),
        WarUi.IssuedIntentIds.Num(),
        1);
    TestFalse(
        TEXT("Rumor-level War confidence remains qualitative"),
        WarUi.OutcomeConfidence.bExactProbabilityVisible);

    FOGQualitativeRiskViewModel ConfirmedRisk;
    TestTrue(
        TEXT("Confirmed exact risk may be shown when explicitly authorized"),
        FOGUiViewModelService::ProjectRisk(
            7000,
            EOGUiKnowledgeState::Confirmed,
            true,
            AuthoredRiskThresholds,
            ConfirmedRisk,
            Error));
    TestTrue(
        TEXT("Exact probability appears only with confirmed authorized knowledge"),
        ConfirmedRisk.bExactProbabilityVisible);
    TestEqual(
        TEXT("Exact confirmed risk value is preserved"),
        ConfirmedRisk.ExactProbabilityBps,
        7000);

    Store.Close();
    IFileManager::Get().DeleteDirectory(
        *Directory,
        false,
        true);
    return true;
}

#endif
