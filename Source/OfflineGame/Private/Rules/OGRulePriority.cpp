#include "Rules/OGRulePriority.h"

int32 FOGRulePriority::Compare(
    const FOGRulePriority& A,
    const FOGRulePriority& B)
{
    if (A.Authority != B.Authority)
    {
        return A.Authority > B.Authority ? 1 : -1;
    }

    if (A.Specificity != B.Specificity)
    {
        return A.Specificity > B.Specificity ? 1 : -1;
    }

    if (A.SourceRank != B.SourceRank)
    {
        return A.SourceRank > B.SourceRank ? 1 : -1;
    }

    const int32 Lexical =
        A.SourceRuleId.Value.Compare(
            B.SourceRuleId.Value,
            ESearchCase::CaseSensitive);

    if (Lexical == 0)
    {
        return 0;
    }

    // Lexically smaller stable IDs win a complete numeric tie.
    return Lexical < 0 ? 1 : -1;
}
