#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentManifest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGContentIdValidationTest,
    "OfflineGame.Content.ContentId.Validation",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGContentIdValidationTest::RunTest(const FString& Parameters)
{
    TestTrue(
        TEXT("Normalized content ID is valid"),
        FOGContentId(TEXT("core:character.test_001")).IsValid());

    TestFalse(
        TEXT("Content ID requires namespace separator"),
        FOGContentId(TEXT("character.test_001")).IsValid());

    TestFalse(
        TEXT("Content IDs are lowercase/stable"),
        FOGContentId(TEXT("Core:Character.Test")).IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGAdultEligibilityValidationTest,
    "OfflineGame.Content.Character.AdultReferencesRequireCanonicalAdult",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGAdultEligibilityValidationTest::RunTest(const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId = FOGContentId(TEXT("test:package"));
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId = FOGContentId(TEXT("test:identity"));
    Identity.CanonicalMaturity = EOGCanonicalMaturity::NonAdult;
    Manifest.CharacterIdentities.Add(Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId = FOGContentId(TEXT("test:version.base"));
    Version.IdentityId = Identity.IdentityId;
    Version.VersionKind = TEXT("base");
    Version.AdultContentProfileId = FOGContentId(TEXT("test:adult_profile.default"));
    Version.AdultSceneLibraryIds.Add(FOGContentId(TEXT("test:adult_scene_library.default")));
    Manifest.CharacterVersions.Add(Version);

    TArray<FString> Errors;
    const bool bValid = FOGContentManifestValidator::Validate(
        Manifest,
        TMap<FOGContentId, EOGCanonicalMaturity>(),
        Errors);

    TestFalse(TEXT("Manifest must be rejected"), bValid);
    TestTrue(TEXT("Validation returns an actionable error"), Errors.Num() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGAdultCharacterManifestValidationTest,
    "OfflineGame.Content.Character.AdultIdentityCanReferenceAdultContent",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGAdultCharacterManifestValidationTest::RunTest(const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId = FOGContentId(TEXT("test:package"));
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId = FOGContentId(TEXT("test:identity"));
    Identity.CanonicalMaturity = EOGCanonicalMaturity::Adult;
    Manifest.CharacterIdentities.Add(Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId = FOGContentId(TEXT("test:version.base"));
    Version.IdentityId = Identity.IdentityId;
    Version.VersionKind = TEXT("base");
    Version.AdultContentProfileId = FOGContentId(TEXT("test:adult_profile.default"));
    Version.AdultSceneLibraryIds.Add(FOGContentId(TEXT("test:adult_scene_library.default")));
    Manifest.CharacterVersions.Add(Version);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Canonically adult identity may validate descriptive adult content"),
        FOGContentManifestValidator::Validate(
            Manifest,
            TMap<FOGContentId, EOGCanonicalMaturity>(),
            Errors));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGExternalAdultIdentityVersionPackageTest,
    "OfflineGame.Content.Character.ExternalAdultIdentityCanReceiveAdultReferences",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGExternalAdultIdentityVersionPackageTest::RunTest(const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId = FOGContentId(TEXT("test:version_extension"));
    Manifest.Version = 1;

    FOGCharacterVersionDefinition Version;
    Version.VersionId = FOGContentId(TEXT("test:identity.special"));
    Version.IdentityId = FOGContentId(TEXT("test:identity"));
    Version.VersionKind = TEXT("special");
    Version.AdultContentProfileId = FOGContentId(TEXT("test:adult_profile.default"));
    Version.AdultSceneLibraryIds.Add(FOGContentId(TEXT("test:adult_scene_library.default")));
    Manifest.CharacterVersions.Add(Version);

    TMap<FOGContentId, EOGCanonicalMaturity> KnownIdentities;
    KnownIdentities.Add(
        FOGContentId(TEXT("test:identity")),
        EOGCanonicalMaturity::Adult);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Version-only package validates against known adult Identity"),
        FOGContentManifestValidator::Validate(
            Manifest,
            KnownIdentities,
            Errors));

    return true;
}

#endif
