#include "autowall.h"
#include "../core/interfaces.h"
#include <algorithm>
#include <cmath>

// Accurate TraceToExit similar to how Source SDK handles it
bool TraceToExit(const CVector& start, const CVector& direction, CVector& exitPoint, CTrace& exitTrace)
{
    float distance = 0.0f;
    constexpr float maxDistance = 90.0f;
    constexpr float stepSize = 4.0f;

    CVector lastPos = start;

    while (distance <= maxDistance)
    {
        distance += stepSize;
        CVector end = start + direction.Scale(distance);

        CTrace trace;
        CTraceFilter filter(globals::localPlayer);
        CRay ray(end, end + direction.Scale(stepSize));

        interfaces::engineTrace->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, filter, trace);

        // If we found a valid exit point
        if (!trace.startSolid && !trace.allSolid)
        {
            exitPoint = trace.endPos;
            exitTrace = trace;
            return true;
        }

        lastPos = end;
    }

    return false;
}

float Autowall::CanHitDamage(const CVector& start, const CVector& end, CEntity* target) {
    CEntity* localPlayer = interfaces::entityList->GetEntityFromIndex(interfaces::engine->GetLocalPlayerIndex());
    if (!localPlayer)
        return 0.0f;

    CRay ray(start, end);
    CTraceFilter filter(localPlayer);
    CTrace entryTrace;

    interfaces::engineTrace->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, filter, entryTrace);

    // Nothing hit at all
    if (!entryTrace.DidHit())
        return 0.0f;

    // Hit something else
    if (target && entryTrace.entity != target)
        return 0.0f;

    // Fully visible
    if (entryTrace.fraction >= 0.97f)
        return 100.0f;

    // Stuck inside geometry
    if (entryTrace.startSolid && entryTrace.allSolid)
        return 0.0f;

    // Try to trace to exit point
    CVector exitPos;
    CTrace exitTrace;
    if (!TraceToExit(entryTrace.endPos, entryTrace.plane.normal, exitPos, exitTrace))
        return 0.0f;

    float thickness = (exitPos - entryTrace.endPos).Length();
    float damageLoss = std::max(thickness * 2.0f, 20.0f); // simple penetration loss

    constexpr float baseDamage = 100.0f;
    float finalDamage = baseDamage - damageLoss;

    return std::max(finalDamage, 0.0f);
}
