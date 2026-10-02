#include "Core/OGEntityId.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGEntityIdValidityTest,
    "OfflineGame.Core.EntityId.NewIdIsValid",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGEntityIdValidityTest::RunTest(const FString& Parameters)
{
    const FOGEntityId Id = FOGEntityId::NewId();

    TestTrue(TEXT("New stable entity IDs are valid"), Id.IsValid());
    TestFalse(TEXT("New stable entity IDs serialize to a non-empty string"), Id.ToString().IsEmpty());

    return true;
}

#endif
