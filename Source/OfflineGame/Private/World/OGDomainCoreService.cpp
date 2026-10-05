#include "World/OGDomainCoreService.h"

#include "Events/OGWorldEvent.h"

FName FOGDomainCoreService::HeartStateForIntactCore(
    const FOGDomainCoreRecord& Core)
{
    return FOGLargeNumber::Compare(
               Core.CurrentDurability,
               Core.MaxDurability) < 0
        ? FName(TEXT("damaged"))
        : FName(TEXT("functional"));
}

bool FOGDomainCoreService::EnsureConceptProjection(
    const FOGDomainCoreRecord& Core,
    TArray<FOGDomainCoreConceptRecord>& OutConcepts,
    FString& OutError)
{
    if (!Store.ListDomainCoreConcepts(
            Core.CoreId,
            OutConcepts,
            OutError))
    {
        return false;
    }

    TSet<FString> KnownConceptIds;
    for (const FOGDomainCoreConceptRecord& Concept :
         OutConcepts)
    {
        KnownConceptIds.Add(
            Concept.ConceptId.ToString());
    }

    bool bAdded = false;
    for (const FOGDomainCoreAspect& Aspect :
         Core.Aspects)
    {
        const FString ConceptKey =
            Aspect.AspectId.ToString();
        if (KnownConceptIds.Contains(
                ConceptKey))
        {
            continue;
        }

        FOGDomainCoreConceptRecord Concept;
        Concept.CoreId =
            Core.CoreId;
        Concept.ConceptId =
            Aspect.AspectId;
        Concept.Grade =
            Aspect.Grade;
        Concept.OriginSourceCoreId =
            Core.CoreId;

        if (!Store.UpsertDomainCoreConcept(
                Concept,
                OutError))
        {
            return false;
        }

        KnownConceptIds.Add(
            ConceptKey);
        bAdded = true;
    }

    if (bAdded)
    {
        return Store.ListDomainCoreConcepts(
            Core.CoreId,
            OutConcepts,
            OutError);
    }

    return true;
}

bool FOGDomainCoreService::ActivateAwakenedCoreAsHeart(
    const FOGEntityId& CoreId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!CoreId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain-heart activation requires a valid Core and non-negative world tick.");
        return false;
    }

    bool bCoreFound = false;
    FOGDomainCoreRecord Core;
    if (!Store.TryReadDomainCore(
            CoreId,
            bCoreFound,
            Core,
            OutError))
    {
        return false;
    }

    if (!bCoreFound)
    {
        OutError =
            TEXT("Cannot activate an unknown Domain Core as a heart.");
        return false;
    }

    if (Core.Lifecycle !=
            EOGDomainCoreLifecycle::Awakened ||
        Core.CurrentDurability.GetSign() <= 0)
    {
        OutError =
            TEXT("Only an intact awakened Domain Core can become the Territory heart.");
        return false;
    }

    bool bStateFound = false;
    FOGTerritoryDomainStateRecord State;
    if (!Store.TryReadTerritoryDomainState(
            Core.TerritoryId,
            bStateFound,
            State,
            OutError))
    {
        return false;
    }

    if (bStateFound &&
        State.ActiveCoreId.IsValid() &&
        State.ActiveCoreId !=
            Core.CoreId)
    {
        OutError =
            TEXT("Territory already has a different active Domain heart.");
        return false;
    }

    State.TerritoryId =
        Core.TerritoryId;
    State.ActiveCoreId =
        Core.CoreId;
    State.DomainState =
        HeartStateForIntactCore(Core);
    State.bHasHeartLostWorldTick =
        false;
    State.HeartLostWorldTick = 0;
    State.bHasRuinStartedWorldTick =
        false;
    State.RuinStartedWorldTick = 0;
    State.ReconstitutionProjectId =
        FOGEntityId();

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryDomainState(
            State,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain.heart_activated"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Core.TerritoryId;
    Event.RelatedEntities.Add(
        Core.CoreId);
    Event.bChronicleEligible =
        true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGDomainCoreService::MarkHeartLostIfActive(
    const FOGDomainCoreRecord& Core,
    int64 WorldTick,
    FString& OutError)
{
    bool bStateFound = false;
    FOGTerritoryDomainStateRecord State;
    if (!Store.TryReadTerritoryDomainState(
            Core.TerritoryId,
            bStateFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bStateFound ||
        State.ActiveCoreId !=
            Core.CoreId)
    {
        return true;
    }

    State.ActiveCoreId =
        FOGEntityId();
    State.DomainState =
        FName(TEXT("heart_lost_ruining"));

    if (!State.bHasHeartLostWorldTick)
    {
        State.bHasHeartLostWorldTick =
            true;
        State.HeartLostWorldTick =
            WorldTick;
    }

    if (!State.bHasRuinStartedWorldTick)
    {
        State.bHasRuinStartedWorldTick =
            true;
        State.RuinStartedWorldTick =
            WorldTick;
    }

    State.ReconstitutionProjectId =
        FOGEntityId();

    if (!Store.UpsertTerritoryDomainState(
            State,
            OutError))
    {
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain.heart_lost"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Core.TerritoryId;
    Event.RelatedEntities.Add(
        Core.CoreId);
    Event.bChronicleEligible =
        true;

    return Store.AppendWorldEvent(
        Event,
        OutError);
}

bool FOGDomainCoreService::ApplyCoreDurabilityDamage(
    const FOGEntityId& CoreId,
    const FOGLargeNumber& Damage,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!CoreId.IsValid() ||
        Damage.GetSign() < 0 ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain Core damage request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGDomainCoreRecord Core;
    if (!Store.TryReadDomainCore(
            CoreId,
            bFound,
            Core,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Domain Core does not exist.");
        return false;
    }

    if (Core.Lifecycle ==
            EOGDomainCoreLifecycle::Broken ||
        Core.Lifecycle ==
            EOGDomainCoreLifecycle::Absorbed)
    {
        OutError =
            TEXT("Domain Core is already broken or absorbed.");
        return false;
    }

    const FOGLargeNumber NegativeDamage(
        -Damage.Significand,
        Damage.Exponent10);

    Core.CurrentDurability =
        FOGLargeNumber::Add(
            Core.CurrentDurability,
            NegativeDamage);

    bool bJustBroken = false;
    if (FOGLargeNumber::Compare(
            Core.CurrentDurability,
            FOGLargeNumber()) <= 0)
    {
        Core.CurrentDurability =
            FOGLargeNumber();
        Core.Lifecycle =
            EOGDomainCoreLifecycle::Broken;
        Core.ControllerRulerId =
            FOGEntityId();
        bJustBroken = true;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertDomainCore(
            Core,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (bJustBroken)
    {
        if (!MarkHeartLostIfActive(
                Core,
                WorldTick,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        FOGWorldEvent Event;
        Event.EventId =
            FOGEntityId::NewId();
        Event.EventType =
            FName(TEXT("domain_core.broken"));
        Event.WorldTick =
            WorldTick;
        Event.PrimaryEntity =
            Core.CoreId;
        Event.RelatedEntities.Add(
            Core.TerritoryId);
        Event.bChronicleEligible =
            true;

        if (!Store.AppendWorldEvent(
                Event,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }
    else
    {
        bool bStateFound = false;
        FOGTerritoryDomainStateRecord State;
        if (!Store.TryReadTerritoryDomainState(
                Core.TerritoryId,
                bStateFound,
                State,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (bStateFound &&
            State.ActiveCoreId ==
                Core.CoreId)
        {
            State.DomainState =
                HeartStateForIntactCore(
                    Core);
            if (!Store.UpsertTerritoryDomainState(
                    State,
                    OutError))
            {
                FString RollbackError;
                Store.RollbackTransaction(
                    RollbackError);
                return false;
            }
        }
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGDomainCoreService::CaptureIntactCore(
    const FOGEntityId& CoreId,
    const FOGEntityId& NewControllerRulerId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!CoreId.IsValid() ||
        !NewControllerRulerId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Core capture request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGDomainCoreRecord Core;
    if (!Store.TryReadDomainCore(
            CoreId,
            bFound,
            Core,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Domain Core does not exist.");
        return false;
    }

    if (Core.Lifecycle ==
            EOGDomainCoreLifecycle::Broken ||
        Core.Lifecycle ==
            EOGDomainCoreLifecycle::Absorbed ||
        Core.CurrentDurability.GetSign() <= 0)
    {
        OutError =
            TEXT("Broken/destroyed Domain Cores cannot be captured.");
        return false;
    }

    Core.ControllerRulerId =
        NewControllerRulerId;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertDomainCore(
            Core,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (Core.Lifecycle ==
        EOGDomainCoreLifecycle::Awakened)
    {
        bool bStateFound = false;
        FOGTerritoryDomainStateRecord State;
        if (!Store.TryReadTerritoryDomainState(
                Core.TerritoryId,
                bStateFound,
                State,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (!bStateFound ||
            !State.ActiveCoreId.IsValid() ||
            State.ActiveCoreId ==
                Core.CoreId)
        {
            State.TerritoryId =
                Core.TerritoryId;
            State.ActiveCoreId =
                Core.CoreId;
            State.DomainState =
                HeartStateForIntactCore(
                    Core);

            if (!Store.UpsertTerritoryDomainState(
                    State,
                    OutError))
            {
                FString RollbackError;
                Store.RollbackTransaction(
                    RollbackError);
                return false;
            }
        }
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain_core.captured"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Core.CoreId;
    Event.RelatedEntities.Add(
        Core.TerritoryId);
    Event.RelatedEntities.Add(
        NewControllerRulerId);
    Event.bChronicleEligible =
        true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGDomainCoreService::FuseCores(
    const FOGDomainCoreFusionRequest& Request,
    int64 WorldTick,
    FOGDomainCoreFusionRecord& OutFusion,
    FString& OutError)
{
    OutFusion =
        FOGDomainCoreFusionRecord();
    OutError.Reset();

    if (!Request.AbsorberCoreId.IsValid() ||
        !Request.AbsorbedCoreId.IsValid() ||
        Request.AbsorberCoreId ==
            Request.AbsorbedCoreId ||
        !Request.SynthesisRuleId.IsValid() ||
        Request.OutcomeKind.IsNone() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain Core fusion request is invalid.");
        return false;
    }

    TSet<FString> SynthesizedIds;
    for (const FOGDomainCoreSynthesisConcept& Concept :
         Request.SynthesizedConcepts)
    {
        if (!Concept.ConceptId.IsValid() ||
            Concept.Grade < 0 ||
            SynthesizedIds.Contains(
                Concept.ConceptId.ToString()))
        {
            OutError =
                TEXT("Domain Core fusion contains invalid or duplicate synthesized Concepts.");
            return false;
        }

        SynthesizedIds.Add(
            Concept.ConceptId.ToString());
    }

    bool bAbsorberFound = false;
    bool bAbsorbedFound = false;
    FOGDomainCoreRecord Absorber;
    FOGDomainCoreRecord Absorbed;

    if (!Store.TryReadDomainCore(
            Request.AbsorberCoreId,
            bAbsorberFound,
            Absorber,
            OutError) ||
        !Store.TryReadDomainCore(
            Request.AbsorbedCoreId,
            bAbsorbedFound,
            Absorbed,
            OutError))
    {
        return false;
    }

    if (!bAbsorberFound ||
        !bAbsorbedFound)
    {
        OutError =
            TEXT("Domain Core fusion requires both source Cores to exist.");
        return false;
    }

    const auto IsFuseable =
        [](const FOGDomainCoreRecord& Core)
        {
            return Core.Lifecycle !=
                       EOGDomainCoreLifecycle::Broken &&
                Core.Lifecycle !=
                       EOGDomainCoreLifecycle::Absorbed &&
                Core.CurrentDurability.GetSign() > 0;
        };

    if (!IsFuseable(Absorber) ||
        !IsFuseable(Absorbed))
    {
        OutError =
            TEXT("Broken, absorbed, or zero-durability Cores cannot enter normal fusion.");
        return false;
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    TArray<FOGDomainCoreConceptRecord> AbsorberConcepts;
    TArray<FOGDomainCoreConceptRecord> AbsorbedConcepts;

    if (!EnsureConceptProjection(
            Absorber,
            AbsorberConcepts,
            OutError) ||
        !EnsureConceptProjection(
            Absorbed,
            AbsorbedConcepts,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    TSet<FString> ResultConceptIds;
    for (const FOGDomainCoreConceptRecord& Concept :
         AbsorberConcepts)
    {
        ResultConceptIds.Add(
            Concept.ConceptId.ToString());
    }

    // Preserve distinct Concepts from the absorbed Core. Identical Concept IDs
    // are not additively stacked; their source identity remains reconstructible
    // through the source Core and persistent fusion lineage.
    for (const FOGDomainCoreConceptRecord& SourceConcept :
         AbsorbedConcepts)
    {
        const FString ConceptKey =
            SourceConcept.ConceptId.ToString();
        if (ResultConceptIds.Contains(
                ConceptKey))
        {
            continue;
        }

        FOGDomainCoreConceptRecord Preserved =
            SourceConcept;
        Preserved.CoreId =
            Absorber.CoreId;
        if (!Preserved.OriginSourceCoreId.IsValid())
        {
            Preserved.OriginSourceCoreId =
                Absorbed.CoreId;
        }

        if (!Store.UpsertDomainCoreConcept(
                Preserved,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        ResultConceptIds.Add(
            ConceptKey);
    }

    // New synthesis outputs are already bounded/resolved by content rules.
    for (const FOGDomainCoreSynthesisConcept& Synthesized :
         Request.SynthesizedConcepts)
    {
        FOGDomainCoreConceptRecord Concept;
        Concept.CoreId =
            Absorber.CoreId;
        Concept.ConceptId =
            Synthesized.ConceptId;
        Concept.Grade =
            Synthesized.Grade;
        Concept.SynthesisRuleId =
            Request.SynthesisRuleId;
        Concept.StateJson =
            Synthesized.StateJson.IsEmpty()
                ? TEXT("{}")
                : Synthesized.StateJson;

        if (!Store.UpsertDomainCoreConcept(
                Concept,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    TArray<FOGDomainCoreFusionRecord> PriorFusions;
    if (!Store.ListDomainCoreFusionsForResult(
            Absorber.CoreId,
            PriorFusions,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    int32 SequenceOrdinal = 0;
    for (const FOGDomainCoreFusionRecord& Prior :
         PriorFusions)
    {
        SequenceOrdinal =
            FMath::Max(
                SequenceOrdinal,
                Prior.SequenceOrdinal + 1);
    }

    FOGDomainCoreFusionRecord Fusion;
    Fusion.FusionId =
        FOGEntityId::NewId();
    Fusion.ResultCoreId =
        Absorber.CoreId;
    Fusion.AbsorberCoreId =
        Absorber.CoreId;
    Fusion.AbsorbedCoreId =
        Absorbed.CoreId;
    Fusion.FusionWorldTick =
        WorldTick;
    Fusion.SequenceOrdinal =
        SequenceOrdinal;
    Fusion.SynthesisRuleId =
        Request.SynthesisRuleId;
    Fusion.OutcomeKind =
        Request.OutcomeKind;
    Fusion.ResolutionSeed =
        Request.ResolutionSeed;
    Fusion.InstabilityStateJson =
        Request.InstabilityStateJson.IsEmpty()
            ? TEXT("{}")
            : Request.InstabilityStateJson;

    if (!Store.UpsertDomainCoreFusion(
            Fusion,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGDomainCoreLineageRecord AbsorberLineage;
    AbsorberLineage.ResultCoreId =
        Absorber.CoreId;
    AbsorberLineage.SourceCoreId =
        Absorber.CoreId;
    AbsorberLineage.FusionId =
        Fusion.FusionId;
    AbsorberLineage.LineageRole =
        FName(TEXT("absorber"));

    FOGDomainCoreLineageRecord AbsorbedLineage;
    AbsorbedLineage.ResultCoreId =
        Absorber.CoreId;
    AbsorbedLineage.SourceCoreId =
        Absorbed.CoreId;
    AbsorbedLineage.FusionId =
        Fusion.FusionId;
    AbsorbedLineage.LineageRole =
        FName(TEXT("absorbed"));

    if (!Store.UpsertDomainCoreLineage(
            AbsorberLineage,
            OutError) ||
        !Store.UpsertDomainCoreLineage(
            AbsorbedLineage,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    // Absorption is not breakage: preserve the absorbed Core's positive
    // durability/history but remove it from active control.
    Absorbed.Lifecycle =
        EOGDomainCoreLifecycle::Absorbed;
    Absorbed.ControllerRulerId =
        FOGEntityId();

    if (!Store.UpsertDomainCore(
            Absorbed,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!MarkHeartLostIfActive(
            Absorbed,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (Absorber.Lifecycle ==
        EOGDomainCoreLifecycle::Awakened)
    {
        bool bStateFound = false;
        FOGTerritoryDomainStateRecord State;
        if (!Store.TryReadTerritoryDomainState(
                Absorber.TerritoryId,
                bStateFound,
                State,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }

        if (!bStateFound ||
            !State.ActiveCoreId.IsValid() ||
            State.ActiveCoreId ==
                Absorber.CoreId)
        {
            State.TerritoryId =
                Absorber.TerritoryId;
            State.ActiveCoreId =
                Absorber.CoreId;
            State.DomainState =
                HeartStateForIntactCore(
                    Absorber);
            State.bHasHeartLostWorldTick =
                false;
            State.HeartLostWorldTick = 0;
            State.bHasRuinStartedWorldTick =
                false;
            State.RuinStartedWorldTick = 0;
            State.ReconstitutionProjectId =
                FOGEntityId();

            if (!Store.UpsertTerritoryDomainState(
                    State,
                    OutError))
            {
                FString RollbackError;
                Store.RollbackTransaction(
                    RollbackError);
                return false;
            }
        }
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain_core.fused"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Absorber.CoreId;
    Event.RelatedEntities.Add(
        Absorbed.CoreId);
    Event.RelatedEntities.Add(
        Absorber.TerritoryId);
    if (Absorbed.TerritoryId !=
        Absorber.TerritoryId)
    {
        Event.RelatedEntities.Add(
            Absorbed.TerritoryId);
    }
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"fusion_id\":\"%s\",\"rule\":\"%s\",\"outcome\":\"%s\",\"seed\":%lld}"),
        *Fusion.FusionId.ToString(),
        *Fusion.SynthesisRuleId.ToString(),
        *Fusion.OutcomeKind.ToString(),
        static_cast<long long>(
            Fusion.ResolutionSeed));

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    OutFusion =
        Fusion;
    return true;
}

bool FOGDomainCoreService::RefreshDomainHeartConsequences(
    const FOGEntityId& TerritoryId,
    int64 WorldTick,
    bool bRuinThresholdReached,
    FOGTerritoryDomainStateRecord& OutState,
    FString& OutError)
{
    OutState =
        FOGTerritoryDomainStateRecord();
    OutError.Reset();

    if (!TerritoryId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain-heart refresh requires a valid Territory and non-negative world tick.");
        return false;
    }

    bool bFound = false;
    FOGTerritoryDomainStateRecord State;
    if (!Store.TryReadTerritoryDomainState(
            TerritoryId,
            bFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        State.TerritoryId =
            TerritoryId;
        State.DomainState =
            FName(TEXT("none"));

        if (!Store.UpsertTerritoryDomainState(
                State,
                OutError))
        {
            return false;
        }

        OutState =
            State;
        return true;
    }

    if (State.DomainState ==
        FName(TEXT("reconstituting")))
    {
        OutState =
            State;
        return true;
    }

    if (State.ActiveCoreId.IsValid())
    {
        bool bCoreFound = false;
        FOGDomainCoreRecord Core;
        if (!Store.TryReadDomainCore(
                State.ActiveCoreId,
                bCoreFound,
                Core,
                OutError))
        {
            return false;
        }

        if (!bCoreFound ||
            Core.TerritoryId !=
                TerritoryId)
        {
            OutError =
                TEXT("Territory Domain-heart state references a missing or foreign Core.");
            return false;
        }

        if (Core.Lifecycle ==
                EOGDomainCoreLifecycle::Broken ||
            Core.Lifecycle ==
                EOGDomainCoreLifecycle::Absorbed ||
            Core.CurrentDurability.GetSign() <= 0)
        {
            State.ActiveCoreId =
                FOGEntityId();
            State.DomainState =
                bRuinThresholdReached
                    ? FName(TEXT("ruined"))
                    : FName(TEXT("heart_lost_ruining"));

            if (!State.bHasHeartLostWorldTick)
            {
                State.bHasHeartLostWorldTick =
                    true;
                State.HeartLostWorldTick =
                    WorldTick;
            }

            if (!State.bHasRuinStartedWorldTick)
            {
                State.bHasRuinStartedWorldTick =
                    true;
                State.RuinStartedWorldTick =
                    WorldTick;
            }
        }
        else if (Core.Lifecycle ==
                 EOGDomainCoreLifecycle::Awakened)
        {
            // Physical destruction is deliberately absent from this decision.
            // An intact protected Core keeps the Domain metaphysically alive.
            State.DomainState =
                HeartStateForIntactCore(
                    Core);
        }
        else
        {
            OutError =
                TEXT("A dormant Core cannot be an active Territory heart.");
            return false;
        }
    }
    else if (State.DomainState ==
                 FName(TEXT("heart_lost_ruining")) &&
             bRuinThresholdReached)
    {
        State.DomainState =
            FName(TEXT("ruined"));
    }

    if (!Store.UpsertTerritoryDomainState(
            State,
            OutError))
    {
        return false;
    }

    OutState =
        State;
    return true;
}

bool FOGDomainCoreService::BeginHeartReconstitution(
    const FOGEntityId& TerritoryId,
    const FOGEntityId& ReconstitutionProjectId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!TerritoryId.IsValid() ||
        !ReconstitutionProjectId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain-heart reconstitution request is invalid.");
        return false;
    }

    bool bStateFound = false;
    FOGTerritoryDomainStateRecord State;
    if (!Store.TryReadTerritoryDomainState(
            TerritoryId,
            bStateFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bStateFound ||
        (State.DomainState !=
             FName(TEXT("heart_lost_ruining")) &&
         State.DomainState !=
             FName(TEXT("ruined"))))
    {
        OutError =
            TEXT("Only a Territory with a lost Domain heart can begin reconstitution.");
        return false;
    }

    bool bProjectFound = false;
    FOGProjectRecord Project;
    if (!Store.TryReadProject(
            ReconstitutionProjectId,
            bProjectFound,
            Project,
            OutError))
    {
        return false;
    }

    if (!bProjectFound ||
        Project.Status !=
            EOGProjectStatus::Active)
    {
        OutError =
            TEXT("Heart reconstitution requires an active validated high-order Project.");
        return false;
    }

    State.DomainState =
        FName(TEXT("reconstituting"));
    State.ReconstitutionProjectId =
        ReconstitutionProjectId;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryDomainState(
            State,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain.heart_reconstitution_started"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        TerritoryId;
    Event.RelatedEntities.Add(
        ReconstitutionProjectId);
    Event.bChronicleEligible =
        true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGDomainCoreService::CompleteHeartReconstitution(
    const FOGEntityId& TerritoryId,
    const FOGEntityId& NewCoreId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!TerritoryId.IsValid() ||
        !NewCoreId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Domain-heart reconstitution completion request is invalid.");
        return false;
    }

    bool bStateFound = false;
    FOGTerritoryDomainStateRecord State;
    if (!Store.TryReadTerritoryDomainState(
            TerritoryId,
            bStateFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bStateFound ||
        State.DomainState !=
            FName(TEXT("reconstituting")) ||
        !State.ReconstitutionProjectId.IsValid())
    {
        OutError =
            TEXT("Territory is not in an active Domain-heart reconstitution.");
        return false;
    }

    bool bProjectFound = false;
    FOGProjectRecord Project;
    if (!Store.TryReadProject(
            State.ReconstitutionProjectId,
            bProjectFound,
            Project,
            OutError))
    {
        return false;
    }

    if (!bProjectFound ||
        Project.Status !=
            EOGProjectStatus::Completed)
    {
        OutError =
            TEXT("Domain-heart reconstitution cannot complete before its Project completes.");
        return false;
    }

    bool bCoreFound = false;
    FOGDomainCoreRecord NewCore;
    if (!Store.TryReadDomainCore(
            NewCoreId,
            bCoreFound,
            NewCore,
            OutError))
    {
        return false;
    }

    if (!bCoreFound ||
        NewCore.TerritoryId !=
            TerritoryId ||
        NewCore.Lifecycle !=
            EOGDomainCoreLifecycle::Awakened ||
        NewCore.CurrentDurability.GetSign() <= 0)
    {
        OutError =
            TEXT("Reconstituted heart must be an intact awakened Core belonging to this Territory.");
        return false;
    }

    State.ActiveCoreId =
        NewCoreId;
    State.DomainState =
        HeartStateForIntactCore(
            NewCore);
    State.bHasHeartLostWorldTick =
        false;
    State.HeartLostWorldTick = 0;
    State.bHasRuinStartedWorldTick =
        false;
    State.RuinStartedWorldTick = 0;
    const FOGEntityId CompletedProjectId =
        State.ReconstitutionProjectId;
    State.ReconstitutionProjectId =
        FOGEntityId();

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryDomainState(
            State,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("domain.heart_reconstituted"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        TerritoryId;
    Event.RelatedEntities.Add(
        NewCoreId);
    Event.RelatedEntities.Add(
        CompletedProjectId);
    Event.bChronicleEligible =
        true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}
