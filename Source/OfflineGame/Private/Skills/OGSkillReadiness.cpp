#include "Skills/OGSkillReadiness.h"

namespace
{
bool ContainsContentId(
    const TArray<FOGContentId>& Values,
    const FOGContentId& Target)
{
    return Values.ContainsByPredicate(
        [&Target](const FOGContentId& Value)
        {
            return Value == Target;
        });
}

void AddFailure(
    FOGSkillReadinessResult& Result,
    FName Code)
{
    if (!Result.FailureReasonCodes.Contains(Code))
    {
        Result.FailureReasonCodes.Add(Code);
    }
}
}

int32 FOGSkillReadiness::ComputeHpBps(
    const FOGSkillUserRuntimeState& UserState)
{
    if (UserState.MaxHp.GetSign() <= 0 ||
        UserState.CurrentHp.GetSign() <= 0)
    {
        return 0;
    }

    FOGLargeNumber Current = UserState.CurrentHp;
    FOGLargeNumber Maximum = UserState.MaxHp;
    Current.Normalize();
    Maximum.Normalize();

    const int32 Delta =
        Current.Exponent10 - Maximum.Exponent10;

    if (Delta > 9)
    {
        return 10000;
    }

    if (Delta < -9)
    {
        return 0;
    }

    auto Pow10 = [](int32 Power) -> int64
    {
        static constexpr int64 Values[] =
        {
            1LL, 10LL, 100LL, 1000LL, 10000LL,
            100000LL, 1000000LL, 10000000LL,
            100000000LL, 1000000000LL
        };
        return Power >= 0 && Power <= 9 ? Values[Power] : 1LL;
    };

    int64 CurrentSig = Current.Significand;
    int64 MaxSig = Maximum.Significand;

    if (Delta > 0)
    {
        MaxSig /= Pow10(Delta);
    }
    else if (Delta < 0)
    {
        CurrentSig /= Pow10(-Delta);
    }

    if (MaxSig <= 0)
    {
        return 10000;
    }

    return static_cast<int32>(
        FMath::Clamp<int64>(
            (CurrentSig * 10000LL) / MaxSig,
            0,
            10000));
}

FOGSkillReadinessResult FOGSkillReadiness::Evaluate(
    const FOGSkillActivationDefinition& Definition,
    const FOGSkillRuntimeState& SkillState,
    const FOGSkillUserRuntimeState& UserState)
{
    FOGSkillReadinessResult Result;
    Result.bReady = true;

    if (!Definition.SkillId.IsValid() ||
        SkillState.SkillId != Definition.SkillId)
    {
        Result.bReady = false;
        AddFailure(Result, TEXT("invalid_skill_state"));
        return Result;
    }

    for (const FOGSkillReadinessRequirement& Requirement : Definition.Requirements)
    {
        bool bSatisfied = false;

        switch (Requirement.Kind)
        {
        case EOGReadinessRequirementKind::ResourceAtLeast:
        {
            const int64* Value =
                UserState.PersonalResources.Find(
                    Requirement.ResourceKey);
            bSatisfied =
                Value != nullptr &&
                *Value >= Requirement.IntegerOperand;
            break;
        }

        case EOGReadinessRequirementKind::StatePresent:
            bSatisfied =
                ContainsContentId(
                    UserState.ActiveStates,
                    Requirement.ContentOperand);
            break;

        case EOGReadinessRequirementKind::StateAbsent:
            bSatisfied =
                !ContainsContentId(
                    UserState.ActiveStates,
                    Requirement.ContentOperand);
            break;

        case EOGReadinessRequirementKind::UsesThisBattleBelow:
            bSatisfied =
                SkillState.UsesThisBattle <
                Requirement.IntegerOperand;
            break;

        case EOGReadinessRequirementKind::ReadyAtActionValue:
            bSatisfied =
                UserState.CurrentActionValue >=
                SkillState.ReadyAtActionValue;
            break;

        case EOGReadinessRequirementKind::HpAtLeastBps:
            bSatisfied =
                ComputeHpBps(UserState) >=
                Requirement.IntegerOperand;
            break;

        case EOGReadinessRequirementKind::HpAtMostBps:
            bSatisfied =
                ComputeHpBps(UserState) <=
                Requirement.IntegerOperand;
            break;

        case EOGReadinessRequirementKind::PreviousSkillIs:
            bSatisfied =
                UserState.PreviousSkillId ==
                Requirement.ContentOperand;
            break;

        default:
            bSatisfied = false;
            break;
        }

        if (!bSatisfied)
        {
            Result.bReady = false;
            AddFailure(Result, TEXT("requirement_failed"));
        }
    }

    // Costs are also readiness gates.
    for (const FOGSkillCostDefinition& Cost : Definition.Costs)
    {
        switch (Cost.Kind)
        {
        case EOGSkillCostKind::PersonalResource:
        {
            const int64* Value =
                UserState.PersonalResources.Find(
                    Cost.ResourceKey);
            if (!Value || *Value < Cost.Amount)
            {
                Result.bReady = false;
                AddFailure(Result, TEXT("insufficient_resource"));
            }
            break;
        }

        case EOGSkillCostKind::HpPercentMax:
            if (Cost.Amount < 0 ||
                ComputeHpBps(UserState) <= Cost.Amount)
            {
                Result.bReady = false;
                AddFailure(Result, TEXT("insufficient_hp"));
            }
            break;

        case EOGSkillCostKind::FutureActionValue:
            if (Cost.Amount < 0)
            {
                Result.bReady = false;
                AddFailure(Result, TEXT("invalid_action_cost"));
            }
            break;

        default:
            break;
        }
    }

    return Result;
}

bool FOGSkillReadiness::ApplyCosts(
    const FOGSkillActivationDefinition& Definition,
    FOGSkillRuntimeState& SkillState,
    FOGSkillUserRuntimeState& UserState,
    int64& InOutFutureActionValue,
    FString& OutError)
{
    OutError.Reset();

    const FOGSkillReadinessResult Readiness =
        Evaluate(
            Definition,
            SkillState,
            UserState);

    if (!Readiness.bReady)
    {
        OutError = TEXT("Skill is not ready; costs were not applied.");
        return false;
    }

    for (const FOGSkillCostDefinition& Cost : Definition.Costs)
    {
        switch (Cost.Kind)
        {
        case EOGSkillCostKind::PersonalResource:
        {
            int64& Value =
                UserState.PersonalResources.FindOrAdd(
                    Cost.ResourceKey);
            Value -= Cost.Amount;
            break;
        }

        case EOGSkillCostKind::HpPercentMax:
        {
            const FOGLargeNumber CostAmount =
                FOGLargeNumber::ScaleByBasisPoints(
                    UserState.MaxHp,
                    static_cast<int32>(
                        FMath::Clamp<int64>(
                            Cost.Amount,
                            0,
                            10000)));

            const FOGLargeNumber NegativeCost(
                -CostAmount.Significand,
                CostAmount.Exponent10);

            UserState.CurrentHp =
                FOGLargeNumber::Add(
                    UserState.CurrentHp,
                    NegativeCost);
            break;
        }

        case EOGSkillCostKind::FutureActionValue:
            InOutFutureActionValue += Cost.Amount;
            break;

        default:
            break;
        }
    }

    ++SkillState.UsesThisBattle;

    if (Definition.CooldownActionValue > 0)
    {
        SkillState.ReadyAtActionValue =
            UserState.CurrentActionValue +
            Definition.CooldownActionValue;
    }

    UserState.PreviousSkillId =
        Definition.SkillId;

    return true;
}
