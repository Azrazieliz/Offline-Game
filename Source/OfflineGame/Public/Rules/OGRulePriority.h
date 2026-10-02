#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "OGRulePriority.generated.h"

/**
 * Deterministic precedence metadata for conflicting rule claims.
 *
 * Higher values win for Authority, Specificity and SourceRank in that order.
 * Exact game terminology/tier names remain data-defined.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRulePriority
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Authority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Specificity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SourceRank = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SourceRuleId;

    /**
     * Returns >0 when A outranks B, <0 when B outranks A, 0 when identical.
     * Final tie-breaking is stable lexical SourceRuleId ordering.
     */
    static int32 Compare(
        const FOGRulePriority& A,
        const FOGRulePriority& B);
};
