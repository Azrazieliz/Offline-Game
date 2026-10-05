#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentManifest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGContentIdValidationTest,
    "OfflineGame.Content.ContentId.Validation",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGContentIdValidationTest::RunTest(
    const FString& Parameters)
{
    TestTrue(
        TEXT("Normalized content ID is valid"),
        FOGContentId(
            TEXT("core:character.test_001")).IsValid());

    TestFalse(
        TEXT("Content ID requires namespace separator"),
        FOGContentId(
            TEXT("character.test_001")).IsValid());

    TestFalse(
        TEXT("Content IDs are lowercase/stable"),
        FOGContentId(
            TEXT("Core:Character.Test")).IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGNonAdultReferenceValidationTest,
    "OfflineGame.Content.Character.NonAdultIdentityRejectsAdultReferences",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGNonAdultReferenceValidationTest::RunTest(
    const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId =
        FOGContentId(
            TEXT("test:package"));
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId =
        FOGContentId(
            TEXT("test:identity"));
    Identity.CanonicalMaturity =
        EOGCanonicalMaturity::NonAdult;
    Manifest.CharacterIdentities.Add(
        Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId =
        FOGContentId(
            TEXT("test:version.base"));
    Version.IdentityId =
        Identity.IdentityId;
    Version.VersionKind =
        FName(TEXT("base"));
    Version.AdultContentProfileId =
        FOGContentId(
            TEXT("test:adult_profile.invalid_for_nonadult"));
    Manifest.CharacterVersions.Add(
        Version);

    TArray<FString> Errors;
    const bool bValid =
        FOGContentManifestValidator::Validate(
            Manifest,
            TMap<FOGContentId, EOGCanonicalMaturity>(),
            Errors);

    TestFalse(
        TEXT("NonAdult Identity rejects sexual/adult-content references"),
        bValid);
    TestTrue(
        TEXT("Validation returns an actionable error"),
        Errors.Num() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGAdultCharacterManifestValidationTest,
    "OfflineGame.Content.Character.AdultIdentityAllowsDescriptiveAdultReferences",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGAdultCharacterManifestValidationTest::RunTest(
    const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId =
        FOGContentId(
            TEXT("test:package"));
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId =
        FOGContentId(
            TEXT("test:identity"));
    Identity.CanonicalMaturity =
        EOGCanonicalMaturity::Adult;
    Manifest.CharacterIdentities.Add(
        Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId =
        FOGContentId(
            TEXT("test:version.base"));
    Version.IdentityId =
        Identity.IdentityId;
    Version.VersionKind =
        FName(TEXT("base"));
    Version.AdultContentProfileId =
        FOGContentId(
            TEXT("test:adult_profile.default"));
    Version.AdultPresentationTags.Add(
        FName(TEXT("body_state_aware")));
    Version.AdultSceneLibraryIds.Add(
        FOGContentId(
            TEXT("test:adult_scene_library.base")));
    Manifest.CharacterVersions.Add(
        Version);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Canonically Adult Identity validates descriptive adult references"),
        FOGContentManifestValidator::Validate(
            Manifest,
            TMap<FOGContentId, EOGCanonicalMaturity>(),
            Errors));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGAdultIdentityNeedsNoVersionPermissionTest,
    "OfflineGame.Content.Character.AdultIdentityHasNoVersionPermissionGate",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGAdultIdentityNeedsNoVersionPermissionTest::RunTest(
    const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId =
        FOGContentId(
            TEXT("test:adult_identity_without_profile"));
    Manifest.Version = 1;

    FOGCharacterIdentityDefinition Identity;
    Identity.IdentityId =
        FOGContentId(
            TEXT("test:adult_identity"));
    Identity.CanonicalMaturity =
        EOGCanonicalMaturity::Adult;
    Manifest.CharacterIdentities.Add(
        Identity);

    FOGCharacterVersionDefinition Version;
    Version.VersionId =
        FOGContentId(
            TEXT("test:adult_identity.base"));
    Version.IdentityId =
        Identity.IdentityId;
    Version.VersionKind =
        FName(TEXT("base"));
    Manifest.CharacterVersions.Add(
        Version);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Adult Identity validates without any Version-level eligibility switch"),
        FOGContentManifestValidator::Validate(
            Manifest,
            TMap<FOGContentId, EOGCanonicalMaturity>(),
            Errors));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGExternalAdultIdentityVersionPackageTest,
    "OfflineGame.Content.Character.ExternalAdultIdentityCanReceiveAdultReferences",
    EAutomationTestFlags::ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)

bool FOGExternalAdultIdentityVersionPackageTest::RunTest(
    const FString& Parameters)
{
    FOGContentPackageManifest Manifest;
    Manifest.PackageId =
        FOGContentId(
            TEXT("test:version_extension"));
    Manifest.Version = 1;

    FOGCharacterVersionDefinition Version;
    Version.VersionId =
        FOGContentId(
            TEXT("test:identity.special"));
    Version.IdentityId =
        FOGContentId(
            TEXT("test:identity"));
    Version.VersionKind =
        FName(TEXT("special"));
    Version.AdultSceneLibraryIds.Add(
        FOGContentId(
            TEXT("test:adult_scene_library.special")));
    Manifest.CharacterVersions.Add(
        Version);

    TMap<FOGContentId, EOGCanonicalMaturity> KnownIdentities;
    KnownIdentities.Add(
        FOGContentId(
            TEXT("test:identity")),
        EOGCanonicalMaturity::Adult);

    TArray<FString> Errors;
    TestTrue(
        TEXT("Version-only package validates adult references against known Adult Identity"),
        FOGContentManifestValidator::Validate(
            Manifest,
            KnownIdentities,
            Errors));

    return true;
}

#endif
