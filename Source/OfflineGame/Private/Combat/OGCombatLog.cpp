#include "Combat/OGCombatLog.h"

void FOGCombatLog::Append(FOGCombatLogEvent Event)
{
    Event.Sequence = NextSequence++;
    Events.Add(MoveTemp(Event));
}
