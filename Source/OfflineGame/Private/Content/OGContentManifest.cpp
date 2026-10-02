#include "Content/OGContentManifest.h"

namespace
{
void AddError(TArray<FString>& Errors, const FString& Message)
{
    Errors.Add(Message);
}
}

bool FOGContentManifestValidator::Validate(
    const FOGContentPackageManifest& Manifest,
    const TMap<FOGContentId, EOGCanonicalMaturity>& KnownExternalIdentities,
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    if (!Manifest.PackageId.IsValid())
    {
        AddError(OutErrors, TEXT("PackageId is invalid."));
    }

    if (Manifest.Version <= 0)
    {
        AddError(OutErrors, TEXT("Package version must be greater than zero."));
    }

    TSet<FOGContentId> DependencyIds;
    for (const FOGPackageDependency& Dependency : Manifest.Dependencies)
    {
        if (!Dependency.PackageId.IsValid())
        {
            AddError(OutErrors, TEXT("A dependency has an invalid PackageId."));
            continue;
        }

        if (Dependency.MinimumVersion <= 0)
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Dependency %s has an invalid minimum version."),
                    *Dependency.PackageId.ToString()));
        }

        if (Dependency.PackageId == Manifest.PackageId)
        {
            AddError(OutErrors, TEXT("A package cannot depend on itself."));
        }

        if (DependencyIds.Contains(Dependency.PackageId))
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Duplicate dependency: %s"),
                    *Dependency.PackageId.ToString()));
        }

        DependencyIds.Add(Dependency.PackageId);
    }

    TMap<FOGContentId, EOGCanonicalMaturity> IdentityMaturity =
        KnownExternalIdentities;

    TSet<FOGContentId> LocalIdentityIds;
    for (const FOGCharacterIdentityDefinition& Identity : Manifest.CharacterIdentities)
    {
        if (!Identity.IdentityId.IsValid())
        {
            AddError(OutErrors, TEXT("A Character Identity has an invalid IdentityId."));
            continue;
        }

        if (LocalIdentityIds.Contains(Identity.IdentityId) ||
            KnownExternalIdentities.Contains(Identity.IdentityId))
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Character Identity is already defined: %s"),
                    *Identity.IdentityId.ToString()));
            continue;
        }

        LocalIdentityIds.Add(Identity.IdentityId);
        IdentityMaturity.Add(Identity.IdentityId, Identity.CanonicalMaturity);
    }

    TSet<FOGContentId> VersionIds;
    for (const FOGCharacterVersionDefinition& Version : Manifest.CharacterVersions)
    {
        if (!Version.VersionId.IsValid())
        {
            AddError(OutErrors, TEXT("A Character Version has an invalid VersionId."));
            continue;
        }

        if (!Version.IdentityId.IsValid())
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Version %s has an invalid IdentityId."),
                    *Version.VersionId.ToString()));
            continue;
        }

        if (VersionIds.Contains(Version.VersionId))
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Duplicate Character Version: %s"),
                    *Version.VersionId.ToString()));
            continue;
        }

        VersionIds.Add(Version.VersionId);

        const EOGCanonicalMaturity* Maturity =
            IdentityMaturity.Find(Version.IdentityId);

        if (Maturity == nullptr)
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Version %s references unknown Identity %s."),
                    *Version.VersionId.ToString(),
                    *Version.IdentityId.ToString()));
            continue;
        }

        if (Version.bSexualContentEligible &&
            *Maturity != EOGCanonicalMaturity::Adult)
        {
            AddError(
                OutErrors,
                FString::Printf(
                    TEXT("Version %s requests sexual-content eligibility for a Character Identity that is not canonically Adult."),
                    *Version.VersionId.ToString()));
        }
    }

    return OutErrors.IsEmpty();
}
