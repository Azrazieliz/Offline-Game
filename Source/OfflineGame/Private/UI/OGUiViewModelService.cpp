#include "UI/OGUiViewModelService.h"

#include "Gacha/OGGachaService.h"
#include "World/OGSharedWorldStateService.h"
#include "World/OGTerritoryControlService.h"

namespace
{
int32 DecimalDigits(int64 Value)
{
    uint64 Absolute =
        Value < 0
            ? static_cast<uint64>(-(Value + 1)) + 1
            : static_cast<uint64>(Value);

    if (Absolute == 0)
    {
        return 1;
    }

    int32 Digits = 0;
    while (Absolute > 0)
    {
        Absolute /= 10;
        ++Digits;
    }
    return Digits;
}

FString TrimFixedTwo(FString Value)
{
    if (Value.Contains(TEXT(".")))
    {
        while (Value.EndsWith(TEXT("0")))
        {
            Value.LeftChopInline(1);
        }
        if (Value.EndsWith(TEXT(".")))
        {
            Value.LeftChopInline(1);
        }
    }
    return Value;
}

FString SuffixForGroup(int32 Group)
{
    static const TCHAR* Suffixes[] =
    {
        TEXT(""),
        TEXT("K"),
        TEXT("M"),
        TEXT("B"),
        TEXT("T"),
        TEXT("Qa"),
        TEXT("Qi"),
        TEXT("Sx"),
        TEXT("Sp"),
        TEXT("Oc"),
        TEXT("No"),
        TEXT("Dc"),
        TEXT("Ud"),
        TEXT("Dd"),
        TEXT("Td"),
        TEXT("Qad"),
        TEXT("Qid"),
        TEXT("Sxd"),
        TEXT("Spd"),
        TEXT("Ocd"),
        TEXT("Nod"),
        TEXT("Vg")
    };

    if (Group >= 0 &&
        Group < UE_ARRAY_COUNT(Suffixes))
    {
        return FString(Suffixes[Group]);
    }

    // Scalable suffix namespace, deliberately not scientific notation.
    return FString::Printf(
        TEXT("X%d"),
        Group);
}

bool IsCandidateLater(
    const FOGCharacterManifestationRecord& Candidate,
    const FOGCharacterManifestationRecord& Current)
{
    if (Candidate.AcquisitionWorldTick !=
        Current.AcquisitionWorldTick)
    {
        return Candidate.AcquisitionWorldTick >
            Current.AcquisitionWorldTick;
    }

    if (Candidate.AcquisitionOrdinal !=
        Current.AcquisitionOrdinal)
    {
        return Candidate.AcquisitionOrdinal >
            Current.AcquisitionOrdinal;
    }

    return Candidate.ManifestationId.ToString().Compare(
        Current.ManifestationId.ToString(),
        ESearchCase::CaseSensitive) > 0;
}
}

FString FOGUiNumberFormatter::Format(
    const FOGLargeNumber& Value,
    bool bPrecisionKnown,
    bool bCombatCompact)
{
    if (!bPrecisionKnown)
    {
        return TEXT("Unknown");
    }

    if (Value.IsZero())
    {
        return TEXT("0");
    }

    const int32 Digits =
        DecimalDigits(Value.Significand);
    const int32 TotalPower =
        (Digits - 1) +
        Value.Exponent10;

    if (TotalPower < 3)
    {
        if (Value.Exponent10 >= 0 &&
            Value.Exponent10 <= 9)
        {
            int64 Multiplier = 1;
            for (int32 Index = 0;
                 Index < Value.Exponent10;
                 ++Index)
            {
                if (Multiplier >
                    MAX_int64 / 10)
                {
                    break;
                }
                Multiplier *= 10;
            }

            if (FMath::Abs(Value.Significand) <=
                MAX_int64 / FMath::Max<int64>(1, Multiplier))
            {
                return FString::Printf(
                    TEXT("%lld"),
                    static_cast<long long>(
                        Value.Significand * Multiplier));
            }
        }

        const double SmallValue =
            static_cast<double>(Value.Significand) *
            FMath::Pow(
                10.0,
                static_cast<double>(Value.Exponent10));
        return TrimFixedTwo(
            FString::Printf(
                TEXT("%.2f"),
                SmallValue));
    }

    const int32 Group =
        FMath::Max(
            1,
            TotalPower / 3);

    if (bCombatCompact &&
        Group > 999)
    {
        return TEXT("Exceed");
    }

    const int32 SuffixPower =
        Group * 3;
    const double Scaled =
        static_cast<double>(Value.Significand) *
        FMath::Pow(
            10.0,
            static_cast<double>(
                Value.Exponent10 -
                SuffixPower));

    FString Result =
        FString::Printf(
            TEXT("%.2f%s"),
            Scaled,
            *SuffixForGroup(Group));

    const FString Suffix =
        SuffixForGroup(Group);
    if (!Suffix.IsEmpty())
    {
        const int32 SuffixIndex =
            Result.Find(
                Suffix,
                ESearchCase::CaseSensitive,
                ESearchDir::FromEnd);
        if (SuffixIndex > 0)
        {
            FString Numeric =
                Result.Left(SuffixIndex);
            Numeric =
                TrimFixedTwo(Numeric);
            Result =
                Numeric + Suffix;
        }
    }

    if (bCombatCompact &&
        Result.Len() > 10)
    {
        return TEXT("Exceed");
    }

    return Result;
}

FOGContentId FOGUiViewModelService::CharacterLastUsedContextId(
    const FOGContentId& IdentityId)
{
    FString Key =
        IdentityId.ToString();
    Key.ReplaceInline(
        TEXT(":"),
        TEXT("."));
    return FOGContentId(
        FString::Printf(
            TEXT("ui:last_used/%s"),
            *Key));
}

bool FOGUiViewModelService::BuildRulerShell(
    const FOGEntityId& RulerId,
    FOGRulerShellViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGRulerShellViewModel();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Ruler shell requires a valid Ruler.");
        return false;
    }

    OutViewModel.PrimaryDestinations =
    {
        EOGRulerPrimaryDestination::Home,
        EOGRulerPrimaryDestination::Characters,
        EOGRulerPrimaryDestination::Gacha,
        EOGRulerPrimaryDestination::Territory,
        EOGRulerPrimaryDestination::Records
    };

    bool bAccessFound = false;
    FOGRulerGachaAccessRecord Access;
    if (!Store.TryReadRulerGachaAccess(
            RulerId,
            bAccessFound,
            Access,
            OutError))
    {
        return false;
    }

    OutViewModel.bGachaUnlocked =
        bAccessFound &&
        Access.bPermanentlyUnlocked;

    TArray<FOGReportRecord> Reports;
    if (!Store.ListReportsByOwner(
            RulerId,
            Reports,
            OutError))
    {
        return false;
    }

    for (const FOGReportRecord& Report :
         Reports)
    {
        if (!Report.bAcknowledged)
        {
            ++OutViewModel.UnacknowledgedReportCount;
        }
    }

    return true;
}

bool FOGUiViewModelService::BuildRoster(
    const FOGEntityId& RulerId,
    TArray<FOGRosterIdentityViewModel>& OutRoster,
    FString& OutError) const
{
    OutRoster.Reset();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Roster projection requires a valid Ruler.");
        return false;
    }

    TArray<FOGCharacterManifestationRecord> Manifestations;
    if (!Store.ListCharacterManifestationsByOwner(
            RulerId,
            Manifestations,
            OutError))
    {
        return false;
    }

    TMap<FOGContentId, TArray<FOGCharacterManifestationRecord>> Grouped;
    for (const FOGCharacterManifestationRecord& Manifestation :
         Manifestations)
    {
        if (!Manifestation.IdentityId.IsValid())
        {
            OutError =
                TEXT("Roster contains a Manifestation with invalid Character Identity.");
            return false;
        }

        Grouped.FindOrAdd(
            Manifestation.IdentityId).Add(
                Manifestation);
    }

    TArray<FOGContentId> Identities;
    Grouped.GetKeys(
        Identities);
    Identities.Sort(
        [](const FOGContentId& A,
           const FOGContentId& B)
        {
            return A.ToString().Compare(
                B.ToString(),
                ESearchCase::CaseSensitive) < 0;
        });

    for (const FOGContentId& IdentityId :
         Identities)
    {
        TArray<FOGCharacterManifestationRecord>* Owned =
            Grouped.Find(
                IdentityId);
        if (!Owned ||
            Owned->IsEmpty())
        {
            continue;
        }

        bool bSelectionFound = false;
        FOGManifestationContextSelectionRecord Selection;
        const FOGContentId ContextId =
            CharacterLastUsedContextId(
                IdentityId);
        if (!Store.TryReadManifestationContextSelection(
                RulerId,
                ContextId,
                bSelectionFound,
                Selection,
                OutError))
        {
            return false;
        }

        const FOGCharacterManifestationRecord* Selected =
            nullptr;
        if (bSelectionFound)
        {
            Selected =
                Owned->FindByPredicate(
                    [&Selection](
                        const FOGCharacterManifestationRecord& Candidate)
                    {
                        return Candidate.ManifestationId ==
                            Selection.ManifestationId;
                    });
        }

        if (!Selected)
        {
            Selected =
                &(*Owned)[0];
            for (const FOGCharacterManifestationRecord& Candidate :
                 *Owned)
            {
                if (IsCandidateLater(
                        Candidate,
                        *Selected))
                {
                    Selected =
                        &Candidate;
                }
            }
        }

        FOGRosterIdentityViewModel IdentityView;
        IdentityView.IdentityId =
            IdentityId;
        IdentityView.SelectedManifestationId =
            Selected->ManifestationId;
        IdentityView.ManifestationCount =
            Owned->Num();

        Owned->Sort(
            [](const FOGCharacterManifestationRecord& A,
               const FOGCharacterManifestationRecord& B)
            {
                if (A.AcquisitionOrdinal !=
                    B.AcquisitionOrdinal)
                {
                    return A.AcquisitionOrdinal <
                        B.AcquisitionOrdinal;
                }
                return A.ManifestationId.ToString().Compare(
                    B.ManifestationId.ToString(),
                    ESearchCase::CaseSensitive) < 0;
            });

        for (const FOGCharacterManifestationRecord& Manifestation :
             *Owned)
        {
            FOGRosterManifestationViewModel ManifestationView;
            ManifestationView.ManifestationId =
                Manifestation.ManifestationId;
            ManifestationView.BuildLabel =
                Manifestation.BuildLabel;
            ManifestationView.Level =
                Manifestation.Level;
            ManifestationView.CurrentRarity =
                Manifestation.CurrentRarity;
            ManifestationView.bWorldModeAnchored =
                Manifestation.WorldModeAnchorTerritoryId.IsValid();
            ManifestationView.bLastUsed =
                Manifestation.ManifestationId ==
                    Selected->ManifestationId;
            ManifestationView.AcquisitionWorldTick =
                Manifestation.AcquisitionWorldTick;

            bool bRankFound = false;
            FOGEntityRankStateRecord Rank;
            if (!Store.TryReadEntityRankState(
                    Manifestation.ManifestationId,
                    bRankFound,
                    Rank,
                    OutError))
            {
                return false;
            }
            if (bRankFound)
            {
                ManifestationView.RankId =
                    Rank.EffectiveRankId.IsValid()
                        ? Rank.EffectiveRankId
                        : Rank.AttainedRankId;
            }

            bool bFantasmFound = false;
            FOGManifestationWorldFantasmStateRecord Fantasm;
            if (!Store.TryReadManifestationWorldFantasmState(
                    Manifestation.ManifestationId,
                    bFantasmFound,
                    Fantasm,
                    OutError))
            {
                return false;
            }
            if (bFantasmFound)
            {
                ManifestationView.WorldFantasmGradeId =
                    Fantasm.GradeId;
            }

            bool bManagementFound = false;
            FOGManifestationManagementMetadataRecord Management;
            if (!Store.TryReadManifestationManagementMetadata(
                    Manifestation.ManifestationId,
                    bManagementFound,
                    Management,
                    OutError))
            {
                return false;
            }
            if (bManagementFound)
            {
                ManifestationView.bFavorite =
                    Management.bFavorite;
                ManifestationView.bProtected =
                    Management.bProtected;
                ManifestationView.bLocked =
                    Management.bLocked;
                IdentityView.bFavorite |=
                    Management.bFavorite;
                IdentityView.bProtected |=
                    Management.bProtected ||
                    Management.bLocked;
            }

            IdentityView.Manifestations.Add(
                MoveTemp(
                    ManifestationView));
        }

        OutRoster.Add(
            MoveTemp(
                IdentityView));
    }

    return true;
}

bool FOGUiViewModelService::BuildCharacterIdentity(
    const FOGEntityId& RulerId,
    const FOGCharacterIdentityDefinition& Identity,
    FOGCharacterIdentityViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGCharacterIdentityViewModel();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        !Identity.IdentityId.IsValid())
    {
        OutError =
            TEXT("Character Identity projection requires valid Ruler and Identity IDs.");
        return false;
    }

    OutViewModel.IdentityId =
        Identity.IdentityId;
    OutViewModel.DisplayNameKey =
        Identity.DisplayNameKey;
    OutViewModel.CanonicalMaturity =
        Identity.CanonicalMaturity;
    OutViewModel.BottomTabs =
    {
        FName(TEXT("overview")),
        FName(TEXT("skills_world_fantasm")),
        FName(TEXT("equipment")),
        FName(TEXT("progression"))
    };
    OutViewModel.UtilityDestinations =
    {
        FName(TEXT("forms")),
        FName(TEXT("manifestations")),
        FName(TEXT("history"))
    };

    OutViewModel.bAdultUtilityVisible =
        Identity.CanonicalMaturity ==
            EOGCanonicalMaturity::Adult;
    if (OutViewModel.bAdultUtilityVisible)
    {
        OutViewModel.UtilityDestinations.Add(
            FName(TEXT("adult")));
    }

    TArray<FOGRosterIdentityViewModel> Roster;
    if (!BuildRoster(
            RulerId,
            Roster,
            OutError))
    {
        return false;
    }

    const FOGRosterIdentityViewModel* Owned =
        Roster.FindByPredicate(
            [&Identity](
                const FOGRosterIdentityViewModel& Candidate)
            {
                return Candidate.IdentityId ==
                    Identity.IdentityId;
            });

    if (Owned)
    {
        OutViewModel.SelectedManifestationId =
            Owned->SelectedManifestationId;
        OutViewModel.Manifestations =
            Owned->Manifestations;
    }

    return true;
}

bool FOGUiViewModelService::BuildGacha(
    const FOGEntityId& RulerId,
    const FOGGachaBannerDefinition& Banner,
    FOGGachaViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGGachaViewModel();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        !FOGGachaService::ValidateBanner(
            Banner,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Gacha projection requires a valid Ruler.");
        }
        return false;
    }

    OutViewModel.BannerId =
        Banner.BannerId;
    OutViewModel.PullCost =
        Banner.PullCost;
    OutViewModel.SoftPityStart =
        Banner.SoftPityStart;
    OutViewModel.HardPity =
        Banner.HardPity;

    bool bAccessFound = false;
    FOGRulerGachaAccessRecord Access;
    if (!Store.TryReadRulerGachaAccess(
            RulerId,
            bAccessFound,
            Access,
            OutError))
    {
        return false;
    }
    OutViewModel.bUnlocked =
        bAccessFound &&
        Access.bPermanentlyUnlocked;

    bool bCurrencyFound = false;
    int64 CurrencyBalance = 0;
    if (!Store.TryReadResourceBalance(
            RulerId,
            Banner.CurrencyId,
            bCurrencyFound,
            CurrencyBalance,
            OutError))
    {
        return false;
    }
    OutViewModel.CurrencyBalance =
        bCurrencyFound
            ? CurrencyBalance
            : 0;

    bool bTicketSelected = false;
    for (const FOGContentId& TicketId :
         Banner.CompatibleTicketIds)
    {
        bool bTicketKnown = false;
        int64 TicketBalance = 0;
        if (!Store.TryReadResourceBalance(
                RulerId,
                TicketId,
                bTicketKnown,
                TicketBalance,
                OutError))
        {
            return false;
        }

        FOGGachaTicketViewModel TicketView;
        TicketView.TicketId =
            TicketId;
        TicketView.Balance =
            bTicketKnown
                ? TicketBalance
                : 0;
        TicketView.bWillConsumeBeforeCurrency =
            !bTicketSelected &&
            TicketView.Balance > 0;

        if (TicketView.bWillConsumeBeforeCurrency)
        {
            bTicketSelected = true;
        }

        OutViewModel.CompatibleTickets.Add(
            MoveTemp(
                TicketView));
    }
    OutViewModel.bWillUseTicketFirst =
        bTicketSelected;

    bool bGachaStateFound = false;
    FOGGachaStateRecord State;
    if (!Store.TryReadGachaState(
            RulerId,
            Banner.PityCategory,
            bGachaStateFound,
            State,
            OutError))
    {
        return false;
    }

    if (bGachaStateFound)
    {
        OutViewModel.PityCount =
            State.PullsSinceTopRarity;
        OutViewModel.bFeaturedGuarantee =
            State.bFeaturedGuarantee;
    }

    int64 WeightTotal = 0;
    for (const FOGGachaPoolEntry& Entry :
         Banner.Entries)
    {
        WeightTotal +=
            Entry.Weight;
    }

    for (const FOGGachaPoolEntry& Entry :
         Banner.Entries)
    {
        FOGGachaProbabilityViewModel Probability;
        Probability.IdentityId =
            Entry.IdentityId;
        Probability.VersionId =
            Entry.VersionId;
        Probability.Rarity =
            Entry.Rarity;
        Probability.bFeatured =
            Entry.bFeatured;
        Probability.BaseProbabilityBps =
            WeightTotal > 0
                ? static_cast<int32>(
                    (static_cast<int64>(
                        Entry.Weight) *
                     10000) /
                    WeightTotal)
                : 0;
        OutViewModel.BasePool.Add(
            MoveTemp(
                Probability));
    }

    return true;
}

bool FOGUiViewModelService::BuildTerritory(
    const FOGEntityId& RulerId,
    const FOGEntityId& SelectedTerritoryId,
    FOGTerritoryViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGTerritoryViewModel();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Territory projection requires a valid Ruler.");
        return false;
    }

    OutViewModel.QuickSections =
    {
        FName(TEXT("territory")),
        FName(TEXT("domain_core")),
        FName(TEXT("dispatch")),
        FName(TEXT("projects")),
        FName(TEXT("armies_war")),
        FName(TEXT("logistics"))
    };
    OutViewModel.AnalyticalOverlays =
    {
        FName(TEXT("control")),
        FName(TEXT("logistics")),
        FName(TEXT("threat")),
        FName(TEXT("resources")),
        FName(TEXT("projects")),
        FName(TEXT("dispatch"))
    };

    TArray<FOGTerritoryClaimRecord> Claims;
    if (!Store.ListTerritoryClaimsByRuler(
            RulerId,
            Claims,
            OutError))
    {
        return false;
    }

    TSet<FOGEntityId> Seen;
    FOGTerritoryControlService TerritoryControl(
        Store);

    for (const FOGTerritoryClaimRecord& Claim :
         Claims)
    {
        if (Seen.Contains(
                Claim.TerritoryId))
        {
            continue;
        }
        Seen.Add(
            Claim.TerritoryId);

        bool bTerritoryFound = false;
        FOGTerritoryRecord Territory;
        if (!Store.TryReadTerritory(
                Claim.TerritoryId,
                bTerritoryFound,
                Territory,
                OutError))
        {
            return false;
        }
        if (!bTerritoryFound)
        {
            continue;
        }

        FOGTerritorySummaryViewModel Summary;
        Summary.TerritoryId =
            Territory.TerritoryId;
        Summary.RootLocationId =
            Territory.RootLocationId;
        Summary.bMainTerritory =
            Territory.bMainTerritory;
        Summary.ControlState =
            Claim.ControlState;

        FOGTerritoryEffectiveControlResult Effective;
        if (!TerritoryControl.EvaluateEffectiveControl(
                Territory.TerritoryId,
                Claim.UpdatedWorldTick,
                Effective,
                OutError))
        {
            return false;
        }
        Summary.bContested =
            Effective.bContested;

        bool bDomainFound = false;
        FOGTerritoryDomainStateRecord Domain;
        if (!Store.TryReadTerritoryDomainState(
                Territory.TerritoryId,
                bDomainFound,
                Domain,
                OutError))
        {
            return false;
        }
        if (bDomainFound)
        {
            Summary.DomainState =
                Domain.DomainState;
        }

        OutViewModel.Territories.Add(
            MoveTemp(
                Summary));
    }

    OutViewModel.Territories.Sort(
        [](const FOGTerritorySummaryViewModel& A,
           const FOGTerritorySummaryViewModel& B)
        {
            if (A.bMainTerritory !=
                B.bMainTerritory)
            {
                return A.bMainTerritory;
            }
            return A.TerritoryId.ToString().Compare(
                B.TerritoryId.ToString(),
                ESearchCase::CaseSensitive) < 0;
        });

    if (SelectedTerritoryId.IsValid() &&
        OutViewModel.Territories.ContainsByPredicate(
            [&SelectedTerritoryId](
                const FOGTerritorySummaryViewModel& Candidate)
            {
                return Candidate.TerritoryId ==
                    SelectedTerritoryId;
            }))
    {
        OutViewModel.SelectedTerritoryId =
            SelectedTerritoryId;
    }
    else if (!OutViewModel.Territories.IsEmpty())
    {
        OutViewModel.SelectedTerritoryId =
            OutViewModel.Territories[0].TerritoryId;
    }

    return true;
}

bool FOGUiViewModelService::BuildRecordsHub(
    const FOGEntityId& RulerId,
    FOGRecordsHubViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGRecordsHubViewModel();
    OutError.Reset();

    if (!RulerId.IsValid())
    {
        OutError =
            TEXT("Records projection requires a valid Ruler.");
        return false;
    }

    OutViewModel.Destinations =
    {
        FName(TEXT("reports")),
        FName(TEXT("chronicle")),
        FName(TEXT("codex")),
        FName(TEXT("intelligence"))
    };

    TArray<FOGReportRecord> Reports;
    if (!Store.ListReportsByOwner(
            RulerId,
            Reports,
            OutError))
    {
        return false;
    }

    OutViewModel.ReportCount =
        Reports.Num();
    for (const FOGReportRecord& Report :
         Reports)
    {
        if (!Report.bAcknowledged)
        {
            ++OutViewModel.UnacknowledgedReportCount;
        }
    }

    return true;
}

FOGKnowledgeFactViewModel FOGUiViewModelService::ProjectKnowledgeFact(
    const FOGKnowledgeFactRecord* Fact,
    bool bOutdated)
{
    FOGKnowledgeFactViewModel View;

    if (!Fact)
    {
        return View;
    }

    View.ValueJson =
        Fact->ValueJson;
    View.ConfidenceBps =
        Fact->ConfidenceBps;
    View.SourceEntityId =
        Fact->SourceEntityId;
    View.SourceEventId =
        Fact->SourceEventId;

    if (bOutdated ||
        Fact->BeliefState ==
            FName(TEXT("outdated")))
    {
        View.KnowledgeState =
            EOGUiKnowledgeState::Outdated;
    }
    else if (Fact->BeliefState ==
             FName(TEXT("confirmed")))
    {
        View.KnowledgeState =
            EOGUiKnowledgeState::Confirmed;
    }
    else if (Fact->BeliefState ==
             FName(TEXT("rumor")) ||
             Fact->BeliefState ==
             FName(TEXT("rumored")))
    {
        View.KnowledgeState =
            EOGUiKnowledgeState::Rumor;
    }
    else if (Fact->BeliefState ==
             FName(TEXT("contradicted")))
    {
        View.KnowledgeState =
            EOGUiKnowledgeState::Contradicted;
    }
    else
    {
        View.KnowledgeState =
            EOGUiKnowledgeState::Estimated;
    }

    View.bExactValueVisible =
        View.KnowledgeState ==
            EOGUiKnowledgeState::Confirmed;

    return View;
}

bool FOGUiViewModelService::BuildWorldHud(
    const FOGEntityId& KnowledgeOwnerId,
    const FOGEntityId& ControlledEntityId,
    const TArray<FOGEntityId>& CompanionIds,
    const TSet<FOGEntityId>& QteReadyIds,
    const FOGKnowledgeFactRecord* TargetConditionFact,
    bool bTargetConditionOutdated,
    FOGWorldHudViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGWorldHudViewModel();
    OutError.Reset();

    if (!KnowledgeOwnerId.IsValid() ||
        !ControlledEntityId.IsValid())
    {
        OutError =
            TEXT("World HUD projection requires valid knowledge-owner and controlled entity IDs.");
        return false;
    }

    OutViewModel.ControlledEntityId =
        ControlledEntityId;

    FOGSharedWorldStateService SharedWorld(
        Store);

    bool bPresenceFound = false;
    FOGWorldPresenceRecord Presence;
    if (!SharedWorld.TryReadPhysicalPresence(
            ControlledEntityId,
            bPresenceFound,
            Presence,
            OutError))
    {
        return false;
    }

    if (bPresenceFound)
    {
        OutViewModel.CurrentLocationId =
            Presence.LocationId;

        bool bKnown = false;
        EOGLocationKnowledgeLevel Knowledge =
            EOGLocationKnowledgeLevel::Rumored;
        if (!SharedWorld.TryReadLocationKnowledge(
                KnowledgeOwnerId,
                Presence.LocationId,
                bKnown,
                Knowledge,
                OutError))
        {
            return false;
        }

        OutViewModel.bShowMinimap =
            bKnown;
        OutViewModel.bCurrentLocationMapped =
            bKnown &&
            static_cast<uint8>(Knowledge) >=
                static_cast<uint8>(
                    EOGLocationKnowledgeLevel::Located);
        if (bKnown)
        {
            OutViewModel.LocationKnowledge =
                Knowledge;
        }
    }

    TSet<FOGEntityId> UniqueCompanions;
    for (const FOGEntityId& CompanionId :
         CompanionIds)
    {
        if (!CompanionId.IsValid() ||
            CompanionId ==
                ControlledEntityId ||
            UniqueCompanions.Contains(
                CompanionId))
        {
            OutError =
                TEXT("World HUD companion list contains invalid/duplicate state.");
            return false;
        }

        UniqueCompanions.Add(
            CompanionId);

        FOGWorldHudCompanionViewModel Companion;
        Companion.EntityId =
            CompanionId;
        Companion.bQteReady =
            QteReadyIds.Contains(
                CompanionId);
        OutViewModel.Companions.Add(
            MoveTemp(
                Companion));
    }

    if (OutViewModel.Companions.Num() > 2)
    {
        OutError =
            TEXT("World Mode HUD supports protagonist plus up to two companions.");
        return false;
    }

    OutViewModel.TargetCondition =
        ProjectKnowledgeFact(
            TargetConditionFact,
            bTargetConditionOutdated);
    OutViewModel.bShowExactEnemyState =
        OutViewModel.TargetCondition.bExactValueVisible;

    return true;
}

FOGOpeningViewModel FOGUiViewModelService::BuildOpening(
    bool bWorldExists,
    bool bRecoverableWorldExists,
    bool bImportBackupAvailable,
    bool bReducedMotionPresentation)
{
    FOGOpeningViewModel View;
    View.bContinueAvailable =
        bWorldExists;
    View.bRecoverExistingWorldAvailable =
        bRecoverableWorldExists;
    View.bImportBackupAvailable =
        bImportBackupAvailable;
    View.bReducedMotionPresentation =
        bReducedMotionPresentation;

    View.PrimaryActions =
    {
        FName(TEXT("continue")),
        FName(TEXT("recover_existing_world")),
        FName(TEXT("import_backup")),
        FName(TEXT("settings"))
    };

    return View;
}
