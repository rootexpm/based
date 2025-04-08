#include "autowall.h"
#include "../core/interfaces.h"
#include <algorithm>
#include <cmath>
#include <iostream>

bool TraceToExit(const CVector& start, const CVector& direction, CVector& exitPoint, CTrace& exitTrace)
{
    float distance = 0.0f;
    constexpr float maxDistance = 90.0f;
    constexpr float stepSize = 4.0f;

    while (distance <= maxDistance)
    {
        distance += stepSize;
        CVector end = start + direction.Scale(distance);

        CTrace trace;
        CTraceFilter filter(globals::localPlayer);
        CRay ray(end, end + direction.Scale(stepSize));

        interfaces::engineTrace->TraceRay(ray, MASK_SHOT, filter, trace);

        if (!trace.startSolid && !trace.allSolid)
        {
            exitPoint = trace.endPos;
            exitTrace = trace;
            return true;
        }
    }

    return false;
}

float Autowall::CanHitDamage(const CVector& start, const CVector& end, CEntity* target, WeaponInfo* weapon) {
    static int logTick = 0;
    logTick++;

    CEntity* local = interfaces::entityList->GetEntityFromIndex(interfaces::engine->GetLocalPlayerIndex());
    if (!local || !weapon) return 0.0f;

    CVector direction = (end - start);
    float distance = direction.Length();
    direction.Normalize();

    if (logTick % 60 == 0) {
        std::cout << "[Autowall] Checking line from: ("
            << start.x << ", " << start.y << ", " << start.z << ") to ("
            << end.x << ", " << end.y << ", " << end.z << ") - Distance: " << distance << "\n";
    }

    CTrace trace;
    CTraceFilter filter(local);
    CRay ray(start, end);
    interfaces::engineTrace->TraceRay(ray, MASK_SHOT, filter, trace);

    if (trace.entity == target && trace.hitgroup > 0 && trace.hitgroup <= 7) {
        if (logTick % 60 == 0)
            std::cout << "[Autowall] Direct line of sight to target. Returning damage: " << weapon->iDamage << "\n";
        return weapon->iDamage;
    }
    else if (trace.entity == target && trace.hitgroup == 0) {
        if (logTick % 60 == 0)
            std::cout << "[Autowall] Hit target, but hitgroup is 0 (non-damageable)\n";
    }

    float remainingPenetration = 4;
    float currentDamage = weapon->iDamage;
    float traceLength = 0.0f;
    CVector traceStart = start;

    while (currentDamage > 1.0f && remainingPenetration > 0.0f) {
        CRay ray(traceStart, traceStart + direction * 128.0f);
        CTraceFilter filter(local);
        CTrace trace;

        interfaces::engineTrace->TraceRay(ray, MASK_SHOT, filter, trace);

        if (trace.fraction == 1.0f) {
            if (logTick % 60 == 0)
                std::cout << "[Autowall] Ray hit nothing, exiting.\n";
            break;
        }

        traceLength += trace.fraction * distance;
        currentDamage *= powf(weapon->flRangeModifier, traceLength / 500.0f);

        if (trace.entity == target && trace.hitgroup > 0 && trace.hitgroup <= 7) {
            if (logTick % 60 == 0)
                std::cout << "[Autowall] Hit target after wall. Damage: " << currentDamage << "\n";
            return currentDamage;
        }

        const auto surfaceData = interfaces::physicsSurface->GetSurfaceData(trace.surface.surfaceProps);
        if (!surfaceData) {
            if (logTick % 60 == 0)
                std::cout << "[Autowall] No surface data. Exiting.\n";
            break;
        }

        if (trace.surface.surfaceProps < 0 || trace.surface.surfaceProps > 127)
            if (logTick % 60 == 0)
                std::cout << "[Autowall] Invalid surface prop " << trace.surface.surfaceProps << "\n";
            break;

        float materialPenetrationMod = surfaceData->game.flPenetrationModifier;
        float damageMod = surfaceData->game.flDamageModifier;

        if (materialPenetrationMod <= 0.01f || std::isnan(materialPenetrationMod) || std::isinf(materialPenetrationMod)) {
            materialPenetrationMod = 1.0f;
        }

        CVector exitPos;
        CTrace exitTrace;
        if (!TraceToExit(trace.endPos, direction, exitPos, exitTrace)) {
            if (logTick % 60 == 0)
                std::cout << "[Autowall] Could not find exit point.\n";
            break;
        }

        float thickness = (exitPos - trace.endPos).Length();

        if (thickness > 200.0f) {
            if (logTick % 60 == 0)
                std::cout << "[Autowall] Thickness too high (" << thickness << "), aborting.\n";
            break;
        }

        float modifier = std::max(0.0f, 1.0f / materialPenetrationMod);

        float lostDamage = ((thickness * modifier) / 24.0f) + (currentDamage * damageMod) + 4.0f;
        lostDamage = std::clamp(lostDamage, 0.0f, currentDamage); // never more than we have

        currentDamage -= lostDamage;

        if (logTick % 60 == 0) {
            std::cout << "[Autowall] Penetrated material. Thickness: " << thickness
                << " | LostDamage: " << lostDamage
                << " | RemainingDamage: " << currentDamage
                << " | RemainingPenetration: " << remainingPenetration - 1 << "\n";
        }

        if (currentDamage < 1.0f)
            break;

        traceStart = exitPos;
        remainingPenetration -= 1.0f;
    }

    if (logTick % 60 == 0)
        std::cout << "[Autowall] Returning 0 damage. Could not hit.\n";

    return 0.0f;
}
