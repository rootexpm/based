#pragma once
#include "../valve/centity.h"
#include "../valve/cusercmd.h"
#include "../valve/cvector.h"
#include "../valve/cmatrix.h"
#include <unordered_map>
#include <algorithm>

namespace hacks {
    // Hitbox definitions
    enum Hitbox {
        HITBOX_HEAD = 0,
        HITBOX_NECK = 1,
        HITBOX_PELVIS = 2,
        HITBOX_STOMACH = 3,
        HITBOX_CHEST = 4,
        HITBOX_UPPER_CHEST = 5,
        HITBOX_LEFT_THIGH = 6,
        HITBOX_RIGHT_THIGH = 7,
        HITBOX_LEFT_CALF = 8,
        HITBOX_RIGHT_CALF = 9,
        HITBOX_LEFT_FOOT = 10,
        HITBOX_RIGHT_FOOT = 11,
        HITBOX_LEFT_HAND = 12,
        HITBOX_RIGHT_HAND = 13,
        HITBOX_LEFT_UPPER_ARM = 14,
        HITBOX_RIGHT_UPPER_ARM = 15,
        HITBOX_LEFT_FOREARM = 16,
        HITBOX_RIGHT_FOREARM = 17,
        HITBOX_MAX = 18
    };

    // Bone mask for SetupBones
    constexpr int BONE_USED_BY_ANYTHING = 0x0007FF00;

    class Ragebot {
    public:
        static void Run(CUserCmd* cmd);
        static void Backtrack(CUserCmd* cmd);
        static void Predict(CUserCmd* cmd);
        
    private:
        struct BacktrackRecord {
            CVector position;
            float simulationTime;
            CMatrix3x4 boneMatrix[128];
        };
        
        struct HitboxInfo {
            int hitbox;
            float damage;
            CVector position;
            float hitChance;
        };
        
        static bool GetBestTarget(CUserCmd* cmd, CEntity*& target, CVector& aimPosition);
        static bool GetBestHitbox(CEntity* target, HitboxInfo& hitboxInfo);
        static float CalculateHitChance(const CVector& start, const CVector& end, CEntity* target);
        static void ModifyViewAngles(CUserCmd* cmd, const CVector& aimPosition);
        static void AutoStop(CUserCmd* cmd, CEntity* localPlayer);
        static void CompensateRecoil(CUserCmd* cmd, CEntity* localPlayer);
        
        // Helper functions
        static bool GetHitboxPosition(CEntity* entity, int hitbox, CVector& position);
        static bool IsTeammate(CEntity* entity);
        static bool HasHelmet(CEntity* entity);
        static float GetSimulationTime(CEntity* entity);
        static CVector GetVelocity(CEntity* entity);
        
        static std::unordered_map<int, std::vector<BacktrackRecord>> backtrackRecords;
        static constexpr int MAX_BACKTRACK_TICKS = 12; // 200ms at 60 tick
        static constexpr float MIN_HIT_CHANCE = 60.0f; // Minimum hit chance percentage
    };
} // namespace hacks 