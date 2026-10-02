#include "Skills/OGResolvedSkillSet.h"

bool FOGResolvedSkillSet::HasDuplicateSkills() const
{
    TSet<FOGContentId> Seen;

    auto AddUnique = [&Seen](const FOGContentId& SkillId)
    {
        if (SkillId.IsEmpty())
        {
            return true;
        }

        if (Seen.Contains(SkillId))
        {
            return false;
        }

        Seen.Add(SkillId);
        return true;
    };

    for (const FOGContentId& SkillId : ActiveSkills)
    {
        if (!AddUnique(SkillId))
        {
            return true;
        }
    }

    for (const FOGContentId& SkillId : PassiveSkills)
    {
        if (!AddUnique(SkillId))
        {
            return true;
        }
    }

    if (!UltimateSkill.IsEmpty() &&
        !AddUnique(UltimateSkill))
    {
        return true;
    }

    return false;
}
