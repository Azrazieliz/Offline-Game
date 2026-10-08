#include "Runtime/OGFoundationCharacterRuntime.h"
#include "Persistence/OGSQLiteWorldStore.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
struct FCharacterRuntimeFixture
{
    FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FOGSQLiteWorldStore Store;
    FOGEntityId Character = FOGEntityId::NewId();
    FOGEntityId Source = FOGEntityId::NewId();
    FOGFoundationCharacterContext Context;
    FString Error;
    bool Open()
    {
        Context.EntityId = Character;
        return Store.Open(FPaths::Combine(Directory, TEXT("character.db")), Error) &&
            Store.UpsertEntity(Character, FName(TEXT("ruler")), 0, TEXT("{\"custom_history\":\"preserved\"}"), Error) &&
            Store.UpsertEntity(Source, FName(TEXT("witness")), 0, TEXT("{}"), Error);
    }
    ~FCharacterRuntimeFixture()
    {
        Store.Close();
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationCharacterDialogueTest,
    "OfflineGame.Foundation.Character.LocalDialogueUsesBeliefAndCannotRewriteFacts",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationCharacterDialogueTest::RunTest(const FString& Parameters)
{
    FCharacterRuntimeFixture F;
    if (!TestTrue(TEXT("Open canonical fixture"), F.Open())) return false;
    FOGFoundationCharacterRuntime Runtime(F.Store);
    FOGKnowledgeFactRecord Fact;
    Fact.FactKey = FName(TEXT("local_report"));
    Fact.SubjectEntityId = F.Source;
    Fact.SourceEntityId = F.Source;
    Fact.ConfidenceBps = 3500;
    Fact.BeliefState = FName(TEXT("rumored"));
    Fact.ValueJson = TEXT("{\"claim\":\"witness_report\"}");
    TestTrue(TEXT("Record sourced subjective knowledge"), Runtime.RecordKnowledge(F.Context, Fact, 10, F.Error));
    FOGSemanticMemoryRecord Memory;
    Memory.MemoryTypeId = FOGContentId(TEXT("test:memory.report"));
    Memory.SubjectEntityId = F.Source; Memory.SalienceBps = 7000;
    Memory.StateJson = TEXT("{\"summary\":\"received_report\"}");
    TestTrue(TEXT("Store structured memory"), Runtime.RecordMemory(F.Context, Memory, 10, F.Error));
    const FOGContentId Tier(TEXT("test:tier.named"));
    TestTrue(TEXT("Promote existing canonical individual"), Runtime.PromoteNpc(F.Context,
        Tier, FOGEntityId(), TEXT("{}"), 11, F.Error));
    FOGFoundationDialogueLine Rumor;
    Rumor.LineId = FOGContentId(TEXT("test:dialogue.rumor"));
    Rumor.Text = TEXT("I have heard a report.");
    Rumor.RequiredKnowledgeKey = Fact.FactKey;
    Rumor.RequiredBeliefState = Fact.BeliefState;
    Rumor.KnowledgeSubjectId = F.Source;
    Rumor.MinimumConfidenceBps = 3000;
    Rumor.RequiredSimulationTier = Tier;
    Rumor.RequiredMemoryTypeId = Memory.MemoryTypeId;
    Rumor.MinimumMemorySalienceBps = 5000;
    FOGFoundationDialogueLine Certain = Rumor;
    Certain.LineId = FOGContentId(TEXT("test:dialogue.certain"));
    Certain.Text = TEXT("The report is certain.");
    Certain.MinimumConfidenceBps = 9000;
    TArray<FOGFoundationDialogueLine> Pool = { Certain, Rumor };
    FOGFoundationDialogueProjection Dialogue;
    TestTrue(TEXT("Invalid generated selection falls back locally"),
        Runtime.SelectGeneratedDialogueLine(F.Context, Pool, Certain.LineId, 7, Dialogue, F.Error));
    TestEqual(TEXT("Only knowledge-compatible authored line selected"), Dialogue.LineId.ToString(), Rumor.LineId.ToString());
    TestTrue(TEXT("Fallback reports local selection"), Dialogue.bUsedLocalFallback);
    TestEqual(TEXT("Relevant structured memory retrieved"), Dialogue.RetrievedMemories.Num(), 1);
    FOGFoundationCharacterProjection Character;
    TestTrue(TEXT("Promoted entity retains canonical identity"), Runtime.Project(F.Context, Character, F.Error));
    TestEqual(TEXT("Promotion uses same individual"), Character.Context.EntityId.ToString(), F.Character.ToString());
    TestTrue(TEXT("Promotion retains meaningful history"), Character.EntityStateJson.Contains(TEXT("custom_history")));
    FOGKnowledgeFactRecord Stored;
    bool Found = false;
    TestTrue(TEXT("Read retained subjective fact"),
        F.Store.TryReadKnowledgeFact(F.Character, Fact.FactKey, F.Source, Found, Stored, F.Error));
    TestEqual(TEXT("Generated candidate cannot change confidence"), Stored.ConfidenceBps, 3500);
    TestEqual(TEXT("Generated candidate cannot change facts"), Stored.ValueJson, Fact.ValueJson);
    TestEqual(TEXT("Knowledge retains source"), Stored.SourceEntityId.ToString(), F.Source.ToString());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationCharacterEquipmentTest,
    "OfflineGame.Foundation.Character.BreakRestoreAndPresentationPreserveCanonicalHistory",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationCharacterEquipmentTest::RunTest(const FString& Parameters)
{
    FCharacterRuntimeFixture F;
    if (!TestTrue(TEXT("Open canonical fixture"), F.Open())) return false;
    FOGFoundationCharacterRuntime Runtime(F.Store);
    FOGEntityId Item;
    TestTrue(TEXT("Acquire item in canonical inventory"), Runtime.AcquireItem(F.Context,
        FOGContentId(TEXT("test:item.neutral_tool")), FOGContentId(), FOGContentId(), 1, Item, F.Error));
    FOGEquipmentSlotValidator Compatible = [](const FOGEntityId&, const FOGContentId&,
        const FOGItemInstanceRecord&, FString& Error) { Error.Reset(); return true; };
    const FOGContentId Slot(TEXT("test:slot.hand"));
    TestTrue(TEXT("Equip with authored compatibility"), Runtime.Equip(F.Context, Slot, Item, Compatible, F.Error));
    TestTrue(TEXT("Break item without deleting its identity"), Runtime.DamageEquipment(F.Context,
        Item, 10000, F.Source, 2, F.Error));
    FOGFoundationCharacterProjection P;
    TestTrue(TEXT("Project canonical broken inventory"), Runtime.Project(F.Context, P, F.Error));
    TestEqual(TEXT("Broken item removed from functional equipment"), P.Equipment.Num(), 0);
    TestEqual(TEXT("Persistent inventory still contains item"), P.Inventory.Num(), 1);
    TestFalse(TEXT("Broken item cannot be equipped"), Runtime.Equip(F.Context, Slot, Item, Compatible, F.Error));
    TestTrue(TEXT("Restore condition while retaining item"), Runtime.RestoreEquipment(F.Context,
        Item, 5000, F.Source, 3, F.Error));
    TestTrue(TEXT("Restored item can be equipped"), Runtime.Equip(F.Context, Slot, Item, Compatible, F.Error));
    FOGManifestationPresentationStateRecord Presentation;
    Presentation.OutfitStateJson = TEXT("{\"layers\":[{\"id\":\"test:layer.outer\",\"condition_bps\":8500}]}");
    Presentation.PresentationVariantStateJson = TEXT("{\"hairstyle\":\"test:hair.neutral\"}");
    TestTrue(TEXT("Protagonist uses canonical entity presentation"), Runtime.SetPresentation(F.Context, Presentation, 4, F.Error));
    TestTrue(TEXT("Injury uses existing canonical entity"), Runtime.SetInjury(F.Context,
        FName(TEXT("wounded")), 2500, F.Source, 5, F.Error));
    TestTrue(TEXT("Project retained presentation and injury"), Runtime.Project(F.Context, P, F.Error));
    TestTrue(TEXT("Existing entity history survives"), P.EntityStateJson.Contains(TEXT("custom_history")));
    TestTrue(TEXT("Existing presentation survives injury"), P.bHasPresentation);
    TestEqual(TEXT("Layer data retained"), P.Presentation.OutfitStateJson, Presentation.OutfitStateJson);
    TestTrue(TEXT("Item provenance survives break and restore"), P.Inventory.Num() == 1 &&
        P.Inventory[0].HistoryStateJson.Contains(TEXT("damaged")) && P.Inventory[0].HistoryStateJson.Contains(TEXT("restored")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGFoundationCharacterCraftTest,
    "OfflineGame.Foundation.Character.KnownCraftCostsAreAtomicAndKnowledgeGated",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGFoundationCharacterCraftTest::RunTest(const FString& Parameters)
{
    FCharacterRuntimeFixture F;
    if (!TestTrue(TEXT("Open canonical fixture"), F.Open())) return false;
    FOGFoundationCharacterRuntime Runtime(F.Store);
    FOGKnowledgeFactRecord Fact;
    Fact.FactKey = FName(TEXT("knows_recipe")); Fact.SourceEntityId = F.Source;
    TestTrue(TEXT("Record recipe knowledge"), Runtime.RecordKnowledge(F.Context, Fact, 1, F.Error));
    const FOGContentId Material(TEXT("test:resource.material"));
    TestTrue(TEXT("Set actual material stock"), F.Store.SetResourceBalance(F.Character, Material, 4, F.Error));
    FOGFoundationKnownRecipe Recipe;
    Recipe.RecipeId = FOGContentId(TEXT("test:recipe.neutral_tool"));
    Recipe.RequiredKnowledgeKey = Fact.FactKey;
    Recipe.OutputDefinitionId = FOGContentId(TEXT("test:item.neutral_tool"));
    FOGFoundationCraftCost Cost; Cost.ResourceId = Material; Cost.Amount = 5;
    Recipe.Costs.Add(Cost);
    FOGEntityId Item;
    TestFalse(TEXT("Insufficient stock refuses crafting"), Runtime.CraftKnown(F.Context, Recipe, 2, Item, F.Error));
    int64 Balance = 0; bool Found = false;
    F.Store.TryReadResourceBalance(F.Character, Material, Found, Balance, F.Error);
    TestEqual(TEXT("Failed craft leaves all materials intact"), Balance, static_cast<int64>(4));
    Recipe.Costs[0].Amount = 3;
    TestTrue(TEXT("Known craft succeeds from actual stock"), Runtime.CraftKnown(F.Context, Recipe, 3, Item, F.Error));
    F.Store.TryReadResourceBalance(F.Character, Material, Found, Balance, F.Error);
    TestEqual(TEXT("Known craft consumes authored cost"), Balance, static_cast<int64>(1));
    FOGFoundationCharacterProjection P;
    TestTrue(TEXT("Project persisted crafted item"), Runtime.Project(F.Context, P, F.Error));
    TestEqual(TEXT("Exactly one output item"), P.Inventory.Num(), 1);
    TestTrue(TEXT("Output retains recipe provenance"), P.Inventory.Num() == 1 && P.Inventory[0].HistoryStateJson.Contains(TEXT("recipe_id")));
    return true;
}
#endif
