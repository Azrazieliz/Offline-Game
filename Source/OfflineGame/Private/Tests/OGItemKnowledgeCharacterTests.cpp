#include "Content/OGContentManifest.h"
#include "Persistence/OGSQLiteWorldStore.h"
#include "World/OGCharacterWorldStateService.h"
#include "World/OGInventoryEquipmentService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
FString MakeCharacterStateTestDirectory()
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

bool PersistTestEntity(
    FOGSQLiteWorldStore& Store,
    const FOGEntityId& Id,
    FName Kind,
    FString& Error)
{
    return Store.UpsertEntity(
        Id,
        Kind,
        0,
        TEXT("{}"),
        Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGItemAffinityProficiencyPresentationTest,
    "OfflineGame.Items.AffinityProficiencyTransferAndPresentationRemainDistinct",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGItemAffinityProficiencyPresentationTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = MakeCharacterStateTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("items.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open schema-14 database"), Store.Open(DatabasePath, Error));
    TestEqual(TEXT("Schema version is 14"), Store.GetSchemaVersion(Error), 14);

    const FOGEntityId OwnerA = FOGEntityId::NewId();
    const FOGEntityId OwnerB = FOGEntityId::NewId();
    const FOGEntityId RulerId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist owner A"), PersistTestEntity(Store, OwnerA, FName(TEXT("character")), Error));
    TestTrue(TEXT("Persist owner B"), PersistTestEntity(Store, OwnerB, FName(TEXT("character")), Error));
    TestTrue(TEXT("Persist Ruler"), PersistTestEntity(Store, RulerId, FName(TEXT("ruler")), Error));

    FOGInventoryEquipmentService Items(Store);

    FOGEntityId ItemId;
    TestTrue(
        TEXT("Create persistent item"),
        Items.CreateItem(
            OwnerA,
            FOGContentId(TEXT("test:item.ancestral_blade")),
            FOGContentId(TEXT("test:rank.sr")),
            FOGContentId(TEXT("test:quality.ancient")),
            10,
            ItemId,
            Error));

    FOGItemOwnerAffinityRecord AffinityA;
    AffinityA.ItemId = ItemId;
    AffinityA.OwnerEntityId = OwnerA;
    AffinityA.AffinityValue = 850;
    AffinityA.MilestoneId = FOGContentId(TEXT("test:affinity.bonded"));
    AffinityA.UpdatedWorldTick = 11;
    AffinityA.StateJson = TEXT("{\"memory\":\"first_wielder\"}");
    TestTrue(TEXT("Persist owner-specific item affinity"), Store.UpsertItemOwnerAffinity(AffinityA, Error));

    FOGEquipmentProficiencyRecord ProficiencyA;
    ProficiencyA.OwnerEntityId = OwnerA;
    ProficiencyA.ProficiencyId = FOGContentId(TEXT("test:proficiency.swordsmanship"));
    ProficiencyA.ProficiencyValue = 4200;
    ProficiencyA.GradeId = FOGContentId(TEXT("test:proficiency_grade.adept"));
    ProficiencyA.UpdatedWorldTick = 11;
    TestTrue(TEXT("Persist independent equipment proficiency"), Items.SetProficiency(ProficiencyA, Error));

    TestTrue(
        TEXT("Transfer applies item-authored affinity behavior"),
        Items.TransferItem(
            ItemId,
            OwnerB,
            20,
            [](
                const FOGItemInstanceRecord& ItemBefore,
                const FOGEntityId& NewOwner,
                const FOGItemOwnerAffinityRecord* Existing,
                int64 WorldTick,
                FOGItemOwnerAffinityRecord& OutAffinity,
                FString& OutError)
            {
                OutError.Reset();
                OutAffinity.ItemId = ItemBefore.ItemId;
                OutAffinity.OwnerEntityId = NewOwner;
                OutAffinity.AffinityValue = Existing ? Existing->AffinityValue : 75;
                OutAffinity.MilestoneId = FOGContentId(TEXT("test:affinity.new_wielder"));
                OutAffinity.UpdatedWorldTick = WorldTick;
                OutAffinity.StateJson = TEXT("{\"transfer_rule\":\"definition_authored\"}");
                return true;
            },
            Error));

    bool bFound = false;
    FOGItemInstanceRecord ItemAfter;
    TestTrue(TEXT("Read transferred item"), Store.TryReadItemInstance(ItemId, bFound, ItemAfter, Error));
    TestTrue(TEXT("Transferred item survives"), bFound);
    TestTrue(TEXT("Item owner changed"), ItemAfter.OwnerEntityId == OwnerB);
    TestTrue(TEXT("Item history was preserved"), ItemAfter.HistoryStateJson.Contains(TEXT("created")));

    FOGItemOwnerAffinityRecord OldAffinity;
    bFound = false;
    TestTrue(TEXT("Read former-owner affinity history"), Store.TryReadItemOwnerAffinity(ItemId, OwnerA, bFound, OldAffinity, Error));
    TestTrue(TEXT("Former-owner affinity remains historical state"), bFound);
    TestEqual(TEXT("Former-owner affinity is unchanged"), OldAffinity.AffinityValue, static_cast<int64>(850));

    FOGItemOwnerAffinityRecord NewAffinity;
    bFound = false;
    TestTrue(TEXT("Read new-owner affinity"), Store.TryReadItemOwnerAffinity(ItemId, OwnerB, bFound, NewAffinity, Error));
    TestTrue(TEXT("New-owner affinity exists"), bFound);
    TestEqual(TEXT("Authored transfer rule controls new affinity"), NewAffinity.AffinityValue, static_cast<int64>(75));

    FOGEquipmentProficiencyRecord PersistedProficiency;
    bFound = false;
    TestTrue(
        TEXT("Read proficiency by authored identity"),
        Store.TryReadEquipmentProficiency(
            OwnerA,
            FOGContentId(TEXT("test:proficiency.swordsmanship")),
            bFound,
            PersistedProficiency,
            Error));
    TestTrue(TEXT("Proficiency remains independent from item transfer"), bFound);
    TestEqual(TEXT("Proficiency value persists"), PersistedProficiency.ProficiencyValue, static_cast<int64>(4200));

    FOGCharacterManifestationRecord Manifestation;
    Manifestation.ManifestationId = FOGEntityId::NewId();
    Manifestation.OwningRulerId = RulerId;
    Manifestation.IdentityId = FOGContentId(TEXT("test:identity.adult_heroine"));
    Manifestation.ActiveVersionId = FOGContentId(TEXT("test:version.base"));
    Manifestation.Level = 1;
    Manifestation.AcquisitionWorldTick = 30;
    Manifestation.LifecycleState = FName(TEXT("active"));
    TestTrue(TEXT("Persist Manifestation"), Store.UpsertCharacterManifestation(Manifestation, 30, Error));

    FOGManifestationPresentationStateRecord Presentation;
    Presentation.ManifestationId = Manifestation.ManifestationId;
    Presentation.SelectedSkinId = FOGContentId(TEXT("test:skin.ceremonial"));
    Presentation.OutfitStateJson = TEXT("{\"outfit\":\"ceremonial\"}");
    Presentation.UpdatedWorldTick = 31;
    TestTrue(TEXT("Persist presentation-only skin state"), Items.SetPresentationState(Presentation, Error));

    FOGCharacterManifestationRecord ManifestationAfterSkin;
    bFound = false;
    TestTrue(TEXT("Read Manifestation after Skin selection"), Store.TryReadCharacterManifestation(Manifestation.ManifestationId, bFound, ManifestationAfterSkin, Error));
    TestTrue(TEXT("Manifestation still exists"), bFound);
    TestTrue(
        TEXT("Presentation skin did not fabricate a gameplay Version"),
        ManifestationAfterSkin.ActiveVersionId == Manifestation.ActiveVersionId);

    Store.Close();

    FOGSQLiteWorldStore Reopened;
    TestTrue(TEXT("Reopen item-state database"), Reopened.Open(DatabasePath, Error));
    FOGManifestationPresentationStateRecord PersistedPresentation;
    bFound = false;
    TestTrue(TEXT("Read Skin after restart"), Reopened.TryReadManifestationPresentationState(Manifestation.ManifestationId, bFound, PersistedPresentation, Error));
    TestTrue(TEXT("Skin state survives restart"), bFound);
    TestTrue(TEXT("Selected Skin survives restart"), PersistedPresentation.SelectedSkinId == Presentation.SelectedSkinId);

    Reopened.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGKnowledgePromotionAdultHeroicStateTest,
    "OfflineGame.Characters.KnowledgePromotionAdultRuntimeAndHeroicRecords",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGKnowledgePromotionAdultHeroicStateTest::RunTest(
    const FString& Parameters)
{
    const FString Directory = MakeCharacterStateTestDirectory();
    const FString DatabasePath = FPaths::Combine(Directory, TEXT("character_world.db"));
    IFileManager::Get().MakeDirectory(*Directory, true);

    FOGSQLiteWorldStore Store;
    FString Error;
    TestTrue(TEXT("Open database"), Store.Open(DatabasePath, Error));

    const FOGEntityId NpcId = FOGEntityId::NewId();
    const FOGEntityId ObserverId = FOGEntityId::NewId();
    TestTrue(TEXT("Persist NPC"), PersistTestEntity(Store, NpcId, FName(TEXT("npc")), Error));
    TestTrue(TEXT("Persist observer"), PersistTestEntity(Store, ObserverId, FName(TEXT("character")), Error));

    FOGWorldEvent RumorEvent;
    RumorEvent.EventId = FOGEntityId::NewId();
    RumorEvent.EventType = FName(TEXT("rumor.heard"));
    RumorEvent.WorldTick = 10;
    RumorEvent.PrimaryEntity = NpcId;
    RumorEvent.PayloadJson = TEXT("{\"claim\":\"hidden_gate\"}");
    TestTrue(TEXT("Persist rumor source event"), Store.AppendWorldEvent(RumorEvent, Error));

    FOGWorldEvent DeathEvent;
    DeathEvent.EventId = FOGEntityId::NewId();
    DeathEvent.EventType = FName(TEXT("character.death"));
    DeathEvent.WorldTick = 40;
    DeathEvent.PrimaryEntity = NpcId;
    DeathEvent.PayloadJson = TEXT("{\"canonical\":true}");
    DeathEvent.bChronicleEligible = true;
    TestTrue(TEXT("Persist canonical death event"), Store.AppendWorldEvent(DeathEvent, Error));

    FOGCharacterWorldStateService Service(Store);

    FOGKnowledgeFactRecord Fact;
    Fact.OwnerEntityId = ObserverId;
    Fact.FactKey = FName(TEXT("hidden_gate_location"));
    Fact.SubjectEntityId = NpcId;
    Fact.ValueJson = TEXT("{\"region\":\"north\"}");
    Fact.LearnedWorldTick = 11;
    Fact.UpdatedWorldTick = 12;
    Fact.BeliefState = FName(TEXT("suspected"));
    Fact.ConfidenceBps = 3500;
    Fact.SourceEntityId = NpcId;
    Fact.SourceEventId = RumorEvent.EventId;
    Fact.bHasEvidenceWorldTick = true;
    Fact.EvidenceWorldTick = 10;
    Fact.LanguageContextId = FOGContentId(TEXT("test:language.old_imperial"));
    TestTrue(TEXT("Persist subjective knowledge fact"), Service.RecordKnowledge(Fact, Error));

    FOGEntityLanguageRecord Language;
    Language.EntityId = ObserverId;
    Language.LanguageId = FOGContentId(TEXT("test:language.old_imperial"));
    Language.SpokenProficiencyBps = 2500;
    Language.WrittenProficiencyBps = 7000;
    TestTrue(TEXT("Persist separate spoken/written language ability"), Service.RecordLanguage(Language, Error));

    FOGSemanticMemoryRecord Memory;
    Memory.MemoryId = FOGEntityId::NewId();
    Memory.OwnerEntityId = ObserverId;
    Memory.SubjectEntityId = NpcId;
    Memory.SourceEventId = RumorEvent.EventId;
    Memory.MemoryTypeId = FOGContentId(TEXT("test:memory.rumor"));
    Memory.SalienceBps = 6000;
    Memory.StateJson = TEXT("{\"semantic_summary\":\"npc mentioned hidden northern gate\"}");
    TestTrue(TEXT("Persist bounded semantic memory"), Service.RecordSemanticMemory(Memory, 12, Error));

    TestTrue(
        TEXT("Promote NPC without replacing canonical entity"),
        Service.PromoteNpc(
            NpcId,
            FOGContentId(TEXT("test:simulation_tier.named")),
            20,
            RumorEvent.EventId,
            TEXT("{\"portrait_package\":\"test:npc.named\"}"),
            Error));

    bool bFound = false;
    FName Kind = NAME_None;
    FString EntityState;
    int64 Revision = -1;
    TestTrue(TEXT("Read promoted canonical entity"), Store.TryReadEntity(NpcId, bFound, Kind, EntityState, Revision, Error));
    TestTrue(TEXT("Promoted NPC retains original entity ID"), bFound);
    TestEqual(TEXT("Promotion does not replace entity kind/history"), Kind, FName(TEXT("npc")));

    FOGNpcPromotionStateRecord Promotion;
    bFound = false;
    TestTrue(TEXT("Read promotion metadata"), Store.TryReadNpcPromotionState(NpcId, bFound, Promotion, Error));
    TestTrue(TEXT("Promotion state exists"), bFound);
    TestTrue(TEXT("Promotion is attached to same entity"), Promotion.EntityId == NpcId);

    FOGCharacterAdultRuntimeStateRecord AdultRuntime;
    AdultRuntime.CharacterEntityId = NpcId;
    AdultRuntime.CurrentProfileVariantId = FOGContentId(TEXT("test:adult_profile.contextual"));
    AdultRuntime.MutableContextStateJson = TEXT("{\"expression\":\"reserved\",\"preference_context\":\"private\"}");
    AdultRuntime.UpdatedWorldTick = 25;
    TestTrue(TEXT("Persist mutable adult context without maturity gate"), Service.SetAdultRuntimeContext(AdultRuntime, Error));

    FOGEntityId HeroicRecordId;
    TestTrue(
        TEXT("Create Heroic Record from canonical death"),
        Service.CreateHeroicRecord(
            NpcId,
            FOGContentId(TEXT("test:identity.heroic_npc")),
            DeathEvent.EventId,
            FOGContentId(TEXT("test:heroic_pattern.last_stand")),
            41,
            HeroicRecordId,
            Error));

    FOGHeroicRecord Heroic;
    bFound = false;
    TestTrue(TEXT("Read Heroic Record"), Store.TryReadHeroicRecord(HeroicRecordId, bFound, Heroic, Error));
    TestTrue(TEXT("Heroic Record persists"), bFound);
    TestTrue(TEXT("Heroic Record points to canonical source death"), Heroic.DeathEventId == DeathEvent.EventId);
    TestTrue(TEXT("Heroic creation preserves source entity identity"), Heroic.SourceWorldEntityId == NpcId);

    FOGKnowledgeFactRecord PersistedFact;
    bFound = false;
    TestTrue(
        TEXT("Read knowledge fact with epistemic provenance"),
        Store.TryReadKnowledgeFact(
            ObserverId,
            FName(TEXT("hidden_gate_location")),
            NpcId,
            bFound,
            PersistedFact,
            Error));
    TestTrue(TEXT("Knowledge fact survives"), bFound);
    TestEqual(TEXT("Belief state survives"), PersistedFact.BeliefState, FName(TEXT("suspected")));
    TestEqual(TEXT("Confidence survives"), PersistedFact.ConfidenceBps, 3500);
    TestTrue(TEXT("Source event provenance survives"), PersistedFact.SourceEventId == RumorEvent.EventId);
    TestTrue(TEXT("Language context survives"), PersistedFact.LanguageContextId == Fact.LanguageContextId);

    Store.Close();
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
