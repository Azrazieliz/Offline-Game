#include "Rules/OGRuleResolver.h"

int32 FOGRuleResolver::SelectWinningIndex(
    const TArray<FOGRulePriority>& Priorities)
{
    if (Priorities.IsEmpty())
    {
        return INDEX_NONE;
    }

    int32 WinningIndex = 0;

    for (int32 Index = 1; Index < Priorities.Num(); ++Index)
    {
        if (FOGRulePriority::Compare(
                Priorities[Index],
                Priorities[WinningIndex]) > 0)
        {
            WinningIndex = Index;
        }
    }

    return WinningIndex;
}
