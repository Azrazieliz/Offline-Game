#pragma once

#include "Gacha/OGGachaService.h"
#include "Gacha/OGRulerGachaAccessService.h"
#include "UI/OGUiViewModelService.h"
#include "World/OGTerritoryControlService.h"

namespace OGDiagnosticGacha
{
    // Authored diagnostic calendar only. One actual canonical tick is one local day.
    // This mapping must never be installed as the ordinary world calendar.
    constexpr int64 MonthTicks = 30;
    constexpr int64 EarnedAccessTick = MonthTicks + 1;
    FOGGachaBannerDefinition MakeBanner();
    FOGCalendarElapsedResolver MakeCalendarResolver();
    FOGEntityId ScopedId(uint32 Ordinal);
    FOGEntityId TrainingRealityId();
    bool EnsureScopedFixture(IOGWorldStore& Store, int64 CanonicalWorldTick, FString& Error);

    bool Project(IOGWorldStore& Store, const FOGEntityId& RulerId,
        const FOGGachaBannerDefinition& Banner, FOGGachaViewModel& OutGacha,
        TArray<FOGGachaHistoryEntryViewModel>& OutHistory,
        TArray<FOGRosterIdentityViewModel>& OutRoster, FString& Error);
}
