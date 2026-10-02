#include "Gacha/OGGachaService.h"

#include "Events/OGWorldEvent.h"
#include "Random/OGDeterministicRng.h"

namespace
{
bool SelectWeightedEntry(
    const FOGGachaBannerDefinition& Banner,
    FOGDeterministicRng& Rng,
    int32 PullOrdinalSinceTop,
    bool bTopOnly,
    bool bFeaturedTopOnly,
    int32& OutIndex,
    FString& OutError)
{
    TArray<int64> EffectiveWeights;
    EffectiveWeights.Init(0, Banner.Entries.Num());

    int64 TotalWeight = 0;
    for (int32 Index = 0; Index < Banner.Entries.Num(); ++Index)
    {
        const FOGGachaPoolEntry& Entry = Banner.Entries[Index];
        const bool bTop = Entry.Rarity == Banner.TopRarity;

        if ((bTopOnly && !bTop) ||
            (bFeaturedTopOnly && (!bTop || !Entry.bFeatured)))
        {
            continue;
        }

        int64 Weight = Entry.Weight;
        if (!bTopOnly &&
            !bFeaturedTopOnly &&
            bTop &&
            Banner.SoftPityStart > 0 &&
            PullOrdinalSinceTop > Banner.SoftPityStart &&
            Banner.SoftPityBonusPerPullBps > 0)
        {
            const int64 SoftSteps =
                static_cast<int64>(PullOrdinalSinceTop - Banner.SoftPityStart);
            const int64 MultiplierBps =
                10000 +
                SoftSteps *
                static_cast<int64>(Banner.SoftPityBonusPerPullBps);

            Weight = FMath::Max<int64>(
                1,
                (Weight * MultiplierBps) / 10000);
        }

        if (Weight <= 0)
        {
            continue;
        }

        if (TotalWeight > static_cast<int64>(MAX_int32) - Weight)
        {
            OutError = TEXT("Effective gacha weight exceeds supported deterministic range.");
            return false;
        }

        EffectiveWeights[Index] = Weight;
        TotalWeight += Weight;
    }

    if (TotalWeight <= 0)
    {
        OutError = TEXT("No eligible gacha entry remains after pity/guarantee filtering.");
        return false;
    }

    const int32 Roll =
        Rng.NextRange(0, static_cast<int32>(TotalWeight));

    int64 Cursor = 0;
    for (int32 Index = 0; Index < EffectiveWeights.Num(); ++Index)
    {
        Cursor += EffectiveWeights[Index];
        if (Roll < Cursor)
        {
            OutIndex = Index;
            return true;
        }
    }

    OutError = TEXT("Gacha weighted selection failed unexpectedly.");
    return false;
}
}

bool FOGGachaService::ValidateBanner(
    const FOGGachaBannerDefinition& Banner,
    FString& OutError)
{
    OutError.Reset();

    if (!Banner.BannerId.IsValid() ||
        Banner.PityCategory.IsNone() ||
        !Banner.CurrencyId.IsValid() ||
        Banner.PullCost < 0 ||
        Banner.TopRarity.IsNone() ||
        Banner.HardPity <= 0 ||
        Banner.SoftPityStart < 0 ||
        Banner.SoftPityStart >= Banner.HardPity ||
        Banner.SoftPityBonusPerPullBps < 0 ||
        Banner.Entries.IsEmpty())
    {
        OutError = TEXT("Gacha banner has invalid identity, cost, pity, or pool configuration.");
        return false;
    }

    bool bHasTop = false;
    bool bHasFeaturedTop = false;
    int64 BaseWeightTotal = 0;

    for (const FOGGachaPoolEntry& Entry : Banner.Entries)
    {
        if (!Entry.IdentityId.IsValid() ||
            !Entry.VersionId.IsValid() ||
            Entry.Rarity.IsNone() ||
            Entry.Weight <= 0)
        {
            OutError = TEXT("Gacha pool entry has invalid content IDs, rarity, or weight.");
            return false;
        }

        BaseWeightTotal += Entry.Weight;
        if (BaseWeightTotal > MAX_int32)
        {
            OutError = TEXT("Gacha base weight total exceeds supported deterministic range.");
            return false;
        }

        if (Entry.Rarity == Banner.TopRarity)
        {
            bHasTop = true;
            bHasFeaturedTop |= Entry.bFeatured;
        }
    }

    if (!bHasTop)
    {
        OutError = TEXT("Gacha banner has no entry matching TopRarity.");
        return false;
    }

    if (Banner.bFeaturedGuaranteeAfterMiss && !bHasFeaturedTop)
    {
        OutError = TEXT("Featured guarantee requires at least one featured top-rarity entry.");
        return false;
    }

    return true;
}

bool FOGGachaService::Pull(
    const FOGGachaBannerDefinition& Banner,
    const FOGEntityId& RulerId,
    int64 WorldTick,
    int64 Seed,
    FOGGachaPullResult& OutResult,
    FString& OutError)
{
    OutResult = FOGGachaPullResult();
    OutError.Reset();

    if (!ValidateBanner(Banner, OutError))
    {
        return false;
    }

    if (!RulerId.IsValid())
    {
        OutError = TEXT("Gacha pull requires a valid owning Ruler ID.");
        return false;
    }

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    auto Fail = [this, &OutError](const FString& Error)
    {
        OutError = Error;
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        if (!RollbackError.IsEmpty())
        {
            OutError += FString::Printf(TEXT(" | Rollback error: %s"), *RollbackError);
        }
        return false;
    };

    bool bRulerFound = false;
    FName RulerKind = NAME_None;
    FString RulerStateJson;
    int64 RulerRevision = 0;
    FString Error;

    if (!Store.TryReadEntity(
            RulerId,
            bRulerFound,
            RulerKind,
            RulerStateJson,
            RulerRevision,
            Error))
    {
        return Fail(Error);
    }

    if (!bRulerFound)
    {
        return Fail(TEXT("Gacha pull owner does not exist in authoritative state."));
    }

    bool bCurrencyKnown = false;
    int64 CurrencyBalance = 0;
    if (Banner.PullCost > 0)
    {
        if (!Store.TryReadResourceBalance(
                RulerId,
                Banner.CurrencyId,
                bCurrencyKnown,
                CurrencyBalance,
                Error))
        {
            return Fail(Error);
        }

        if (!bCurrencyKnown || CurrencyBalance < Banner.PullCost)
        {
            return Fail(TEXT("Insufficient gacha currency."));
        }
    }

    bool bStateFound = false;
    FOGGachaStateRecord State;
    if (!Store.TryReadGachaState(
            RulerId,
            Banner.PityCategory,
            bStateFound,
            State,
            Error))
    {
        return Fail(Error);
    }

    if (!bStateFound)
    {
        State.RulerId = RulerId;
        State.PityCategory = Banner.PityCategory;
    }

    const int32 PullOrdinalSinceTop =
        State.PullsSinceTopRarity + 1;
    const bool bHardPity =
        PullOrdinalSinceTop >= Banner.HardPity;

    FOGDeterministicRng Rng(static_cast<uint64>(Seed));
    int32 SelectedIndex = INDEX_NONE;
    if (!SelectWeightedEntry(
            Banner,
            Rng,
            PullOrdinalSinceTop,
            bHardPity,
            false,
            SelectedIndex,
            Error))
    {
        return Fail(Error);
    }

    const FOGGachaPoolEntry* Selected =
        &Banner.Entries[SelectedIndex];

    bool bTopRarity =
        Selected->Rarity == Banner.TopRarity;

    if (bTopRarity &&
        State.bFeaturedGuarantee &&
        !Selected->bFeatured)
    {
        if (!SelectWeightedEntry(
                Banner,
                Rng,
                PullOrdinalSinceTop,
                true,
                true,
                SelectedIndex,
                Error))
        {
            return Fail(Error);
        }

        Selected = &Banner.Entries[SelectedIndex];
        bTopRarity = true;
    }

    bool bManifestationFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryFindCharacterManifestationByOwnerAndIdentity(
            RulerId,
            Selected->IdentityId,
            bManifestationFound,
            Manifestation,
            Error))
    {
        return Fail(Error);
    }

    const bool bDuplicate = bManifestationFound;
    if (bDuplicate)
    {
        ++Manifestation.DuplicateAcquisitionCount;
        Manifestation.ActiveVersionId = Selected->VersionId;

        if (!Store.UpsertCharacterManifestation(
                Manifestation,
                WorldTick,
                Error))
        {
            return Fail(Error);
        }
    }
    else
    {
        Manifestation.ManifestationId = FOGEntityId::NewId();
        Manifestation.OwningRulerId = RulerId;
        Manifestation.IdentityId = Selected->IdentityId;
        Manifestation.ActiveVersionId = Selected->VersionId;
        Manifestation.Level = 1;
        Manifestation.CurrentRarity = Selected->Rarity;
        Manifestation.DuplicateAcquisitionCount = 0;

        if (!Store.UpsertCharacterManifestation(
                Manifestation,
                WorldTick,
                Error))
        {
            return Fail(Error);
        }
    }

    if (Banner.PullCost > 0 &&
        !Store.SetResourceBalance(
            RulerId,
            Banner.CurrencyId,
            CurrencyBalance - Banner.PullCost,
            Error))
    {
        return Fail(Error);
    }

    ++State.TotalPulls;
    State.UpdatedWorldTick = WorldTick;

    if (bTopRarity)
    {
        State.PullsSinceTopRarity = 0;
        State.bFeaturedGuarantee =
            Banner.bFeaturedGuaranteeAfterMiss &&
            !Selected->bFeatured;
    }
    else
    {
        State.PullsSinceTopRarity = PullOrdinalSinceTop;
    }

    if (!Store.UpsertGachaState(State, Error))
    {
        return Fail(Error);
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType = TEXT("gacha_pull");
    Event.WorldTick = WorldTick;
    Event.PrimaryEntity = RulerId;
    Event.RelatedEntities.Add(Manifestation.ManifestationId);
    Event.bChronicleEligible = false;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"banner\":\"%s\",\"identity\":\"%s\",\"version\":\"%s\",")
        TEXT("\"rarity\":\"%s\",\"featured\":%s,\"duplicate\":%s,")
        TEXT("\"seed\":%lld,\"rng_draws\":%llu}"),
        *Banner.BannerId.ToString(),
        *Selected->IdentityId.ToString(),
        *Selected->VersionId.ToString(),
        *Selected->Rarity.ToString(),
        Selected->bFeatured ? TEXT("true") : TEXT("false"),
        bDuplicate ? TEXT("true") : TEXT("false"),
        Seed,
        static_cast<unsigned long long>(Rng.GetDrawCount()));

    if (!Store.AppendWorldEvent(Event, Error))
    {
        return Fail(Error);
    }

    if (!Store.CommitTransaction(OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(RollbackError);
        return false;
    }

    OutResult.BannerId = Banner.BannerId;
    OutResult.IdentityId = Selected->IdentityId;
    OutResult.VersionId = Selected->VersionId;
    OutResult.Rarity = Selected->Rarity;
    OutResult.ManifestationId = Manifestation.ManifestationId;
    OutResult.bTopRarity = bTopRarity;
    OutResult.bFeatured = Selected->bFeatured;
    OutResult.bDuplicateIdentity = bDuplicate;
    OutResult.DuplicateAcquisitionCount =
        Manifestation.DuplicateAcquisitionCount;
    OutResult.Seed = Seed;
    OutResult.RngDrawCount =
        static_cast<int64>(Rng.GetDrawCount());
    OutResult.UpdatedState = State;

    return true;
}
