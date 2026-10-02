#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "OGResolvedSkillSet.generated.h"

/**
 * Current resolved kit used by combat executors.
 *
 * Learned/evolved skills are integrated into this character-specific kit.
 * The runtime does not expose an endlessly growing bag of duplicate skills.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGResolvedSkillSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> ActiveSkills;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> PassiveSkills;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId UltimateSkill;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CurrentForm;

    bool HasDuplicateSkills() const;
};
