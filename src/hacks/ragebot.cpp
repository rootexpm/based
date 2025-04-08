#define NOMINMAX
#include <Windows.h>

#include "ragebot.h"
#include "../core/interfaces.h"
#include "../core/globals.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include "autowall.h"

namespace hacks {
    std::unordered_map<int, std::vector<Ragebot::BacktrackRecord>> Ragebot::backtrackRecords;
    
    // Toggle for the ragebot feature
    static bool ragebot_enabled = false;
    
    // Debug counter to limit console spam - increased to reduce overhead
    static int debug_counter = 0;
    
    // Cache for local player to avoid repeated lookups
    static CEntity* cached_local_player = nullptr;
    static int last_local_player_index = -1;
    
    // Target tracking for consistency
    static CEntity* last_target = nullptr;
    static int target_switch_cooldown = 0;
    static const int TARGET_SWITCH_COOLDOWN_FRAMES = 30; // Don't switch targets for 30 frames
    
    // View angle smoothing
    static CVector last_aim_angles;
    static const float SMOOTHING_FACTOR = 0.5f; // Lower = smoother, higher = snappier
    
    void Ragebot::Run(CUserCmd* cmd) {
        // Toggle ragebot with F1 key
        if (GetAsyncKeyState(VK_F1) & 1) {
            ragebot_enabled = !ragebot_enabled;
            std::cout << "Ragebot " << (ragebot_enabled ? "ENABLED" : "DISABLED") << std::endl;
            
            // Reset target tracking when toggling
            if (!ragebot_enabled) {
                last_target = nullptr;
                target_switch_cooldown = 0;
            }
        }
        
        if (!ragebot_enabled)
            return;
            
        // Get local player with caching
        int local_player_index = interfaces::engine->GetLocalPlayerIndex();
        if (local_player_index != last_local_player_index) {
            cached_local_player = interfaces::entityList->GetEntityFromIndex(local_player_index);
            last_local_player_index = local_player_index;
        }
        
        CEntity* localPlayer = cached_local_player;
        if (!localPlayer || !localPlayer->IsAlive()) {
            if (debug_counter++ % 500 == 0)  // Reduced debug frequency
                std::cout << "Local player not found or not alive" << std::endl;
            return;
        }
        
        // Debug info every 500 frames (reduced from 100)
        if (debug_counter++ % 500 == 0) {
            std::cout << "Local player found: " << localPlayer->GetIndex() << std::endl;
            std::cout << "Local player health: " << localPlayer->GetHealth() << std::endl;
            std::cout << "Local player team: " << localPlayer->GetTeam() << std::endl;
            
            // Print current view angles
            std::cout << "Current view angles: " 
                      << std::fixed << std::setprecision(2) 
                      << cmd->viewAngles.x << ", " 
                      << cmd->viewAngles.y << std::endl;
        }
            
        // Update backtrack records
        Backtrack(cmd);
        
        // Decrement target switch cooldown
        if (target_switch_cooldown > 0)
            target_switch_cooldown--;
        
        // Find best target and aim position
        CEntity* target = nullptr;
        CVector aimPosition;
        bool found_target = GetBestTarget(cmd, target, aimPosition);
        
        // If we found a target, update last_target
        if (found_target && target != last_target) {
            // Only switch targets if cooldown is 0 or if the new target is much better
            if (target_switch_cooldown == 0) {
                last_target = target;
                target_switch_cooldown = TARGET_SWITCH_COOLDOWN_FRAMES;
            } else {
                // Keep the old target if we're in cooldown
                target = last_target;
                // Recalculate aim position for the old target
                if (target) {
                    HitboxInfo hitboxInfo;
                    if (GetBestHitbox(target, hitboxInfo)) {
                        aimPosition = hitboxInfo.position;
                        found_target = true;
                    } else {
                        found_target = false;
                    }
                } else {
                    found_target = false;
                }
            }
        } else if (!found_target && last_target && target_switch_cooldown == 0) {
            // If we didn't find a target but have a last target, use it
            target = last_target;
            HitboxInfo hitboxInfo;
            if (GetBestHitbox(target, hitboxInfo)) {
                aimPosition = hitboxInfo.position;
                found_target = true;
            }
        }
        
        if (!found_target) {
            if (debug_counter % 500 == 0)  // Reduced debug frequency
                std::cout << "No valid target found" << std::endl;
            return;
        }
        
        if (debug_counter % 500 == 0) {  // Reduced debug frequency
            std::cout << "Target found: " << target->GetIndex() << std::endl;
            std::cout << "Target health: " << target->GetHealth() << std::endl;
            std::cout << "Target team: " << target->GetTeam() << std::endl;
            std::cout << "Aim position: " 
                      << std::fixed << std::setprecision(2) 
                      << aimPosition.x << ", " 
                      << aimPosition.y << ", " 
                      << aimPosition.z << std::endl;
        }

        if (localPlayer->HasSniper() && !localPlayer->IsScoped()) {
            cmd->buttons |= CUserCmd::IN_SECOND_ATTACK;

            // Only scope, don't shoot or aim this tick
            return;
        }

        CompensateRecoil(cmd, localPlayer);
        ModifyViewAngles(cmd, aimPosition);
    }
    
    void Ragebot::CompensateRecoil(CUserCmd* cmd, CEntity* localPlayer) {
        if (!localPlayer || !localPlayer->IsAlive())
            return;

        CVector punch;
        localPlayer->GetAimPunch(punch);
        CVector corrected = cmd->viewAngles - punch.Scale(2.0f); // multiply by 2 if needed
        corrected.Normalize();

        cmd->viewAngles = corrected;
    }

    //bugged
    void Ragebot::AutoStop(CUserCmd* cmd, CEntity* localPlayer) {
        if (!localPlayer || !(localPlayer->GetFlags() & CEntity::FL_ONGROUND))
            return;

        CVector velocity = localPlayer->GetVelocity();
        velocity.Normalize();

        float speed = velocity.Length2D();

        if (speed > 5.0f) { // give some leeway
            cmd->forwardMove = -velocity.x * 450.f;
            cmd->sideMove = -velocity.y * 450.f;
        }
    }
    

    void Ragebot::Backtrack(CUserCmd* cmd) {
        float currentTime = interfaces::globals->currentTime;
        int maxTicks = std::min(MAX_BACKTRACK_TICKS, static_cast<int>(interfaces::globals->intervalPerTick * 1000));
        
        // Only update backtrack every 2 ticks to reduce overhead
        static int tick_counter = 0;
        if (++tick_counter % 2 != 0)
            return;
                              
        // Update records for all players
        for (int i = 1; i <= interfaces::globals->maxClients; i++) {
            CEntity* player = interfaces::entityList->GetEntityFromIndex(i);
            if (!player || !player->IsAlive() || player->IsDormant() || IsTeammate(player))
                continue;
                
            // Store current position and bone matrix
            BacktrackRecord record;
            record.position = player->GetAbsOrigin();
            record.simulationTime = GetSimulationTime(player);
            
            if (interfaces::globals->currentTime - record.simulationTime > 0.2f)
                continue;

            // Setup bones for all players (removed visibility check)
            player->SetupBones(record.boneMatrix, 128, BONE_USED_BY_ANYTHING, currentTime);
            
            // Add to records
            auto& records = backtrackRecords[i];
            records.push_back(record);
            
            // Remove old records
            while (records.size() > maxTicks)
                records.erase(records.begin());
        }
    }
    
    bool Ragebot::GetBestTarget(CUserCmd* cmd, CEntity*& target, CVector& aimPosition) {
        // Use cached local player
        CEntity* localPlayer = cached_local_player;
        if (!localPlayer)
            return false;
            
        float bestScore = -1.0f;
        CEntity* bestTarget = nullptr;
        CVector bestPosition;
        
        // Get eye position once
        CVector eyePosition;
        localPlayer->GetEyePosition(eyePosition);
        
        // Iterate through all players
        for (int i = 1; i <= interfaces::globals->maxClients; i++) {
            CEntity* player = interfaces::entityList->GetEntityFromIndex(i);
            if (!player || !player->IsAlive() || player->IsDormant() || IsTeammate(player))
                continue;
                
            // Get best hitbox for this player
            HitboxInfo hitboxInfo;
            if (!GetBestHitbox(player, hitboxInfo))
                continue;
                
            // Calculate FOV to this hitbox
            CVector aimAngle = (hitboxInfo.position - eyePosition).ToAngle();
            
            // Calculate FOV (angle difference)
            float fov = std::abs(aimAngle.y - cmd->viewAngles.y);
            if (fov > 180.0f)
                fov = 360.0f - fov;

            // Calculate score
            float hitChance = hitboxInfo.hitChance;
            float score = (100.0f - fov) * 0.6f + hitChance * 0.4f;

            // Update best target if score is better
            if (score > bestScore && hitChance >= MIN_HIT_CHANCE) {
                bestScore = score;
                bestTarget = player;
                bestPosition = hitboxInfo.position;

                if (debug_counter % 500 == 0) {
                    std::cout << "Potential target: " << player->GetIndex()
                        << " FOV: " << std::fixed << std::setprecision(2) << fov
                        << " Hit chance: " << hitChance << "%"
                        << " Score: " << score << std::endl;
                }
            }
        }
        
        if (bestTarget) {
            target = bestTarget;
            aimPosition = bestPosition;
            return true;
        }
        
        return false;
    }
    
    bool Ragebot::GetBestHitbox(CEntity* target, HitboxInfo& hitboxInfo) {
        // Define hitbox priorities and damage multipliers
        const struct {
            int hitbox;
            float multiplier;
        } hitboxes[] = {
            { HITBOX_HEAD, 4.0f },
            { HITBOX_CHEST, 1.0f },
            { HITBOX_STOMACH, 1.25f },
            { HITBOX_PELVIS, 1.0f }
        };

        // Use cached local player
        CEntity* localPlayer = cached_local_player;
        if (!localPlayer)
            return false;

        CVector eyePosition;  // ✅ define early
        localPlayer->GetEyePosition(eyePosition);

        float bestDamage = 0.0f;
        CVector bestPosition;
        int bestHitbox = -1;

        // Check each hitbox
        for (const auto& hitbox : hitboxes) {
            CVector position;
            if (!GetHitboxPosition(target, hitbox.hitbox, position))
                continue;

            // Use autowall to determine real damage
            float damage = Autowall::CanHitDamage(eyePosition, position, target);
            if (HasHelmet(target) && hitbox.hitbox == HITBOX_HEAD)
                damage *= 0.5f;

            if (damage > bestDamage) {
                bestDamage = damage;
                bestPosition = position;
                bestHitbox = hitbox.hitbox;

                if (debug_counter % 500 == 0) {
                    std::cout << "Hitbox " << hitbox.hitbox << " damage: " << damage << std::endl;
                }
            }
        }

        if (bestHitbox != -1) {
            hitboxInfo.hitbox = bestHitbox;
            hitboxInfo.damage = bestDamage;
            hitboxInfo.position = bestPosition;
            hitboxInfo.hitChance = CalculateHitChance(eyePosition, bestPosition, target);
            return true;
        }

        return false;
    }
    
    float Ragebot::CalculateHitChance(const CVector& start, const CVector& end, CEntity* target) {
        // Optimized distance calculation without sqrt
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float dz = end.z - start.z;
        float distanceSquared = dx*dx + dy*dy + dz*dz;
        
        // Use distance squared for calculations to avoid sqrt
        float distance = std::sqrt(distanceSquared);
        
        CVector velocity = GetVelocity(target);
        float speedSquared = velocity.x*velocity.x + velocity.y*velocity.y + velocity.z*velocity.z;
        float speed = std::sqrt(speedSquared);
        
        // Base hit chance decreases with distance and target speed
        // Reduced distance penalty from 0.1f to 0.05f to prevent negative values
        float hitChance = 100.0f;
        hitChance -= distance * 0.05f; // Distance penalty (reduced)
        hitChance -= speed * 0.5f;    // Speed penalty
        
        // Ensure hit chance is never negative
        hitChance = std::max(hitChance, 0.0f);
        
        if (debug_counter % 500 == 0) {  // Reduced debug frequency
            std::cout << "Hit chance calculation: " << std::fixed << std::setprecision(2) 
                      << "Distance: " << distance 
                      << " Speed: " << speed 
                      << " Hit chance: " << hitChance << "%" << std::endl;
        }
        
        return std::clamp(hitChance, 0.0f, 100.0f);
    }
    
    void Ragebot::ModifyViewAngles(CUserCmd* cmd, const CVector& aimPosition) {
        CEntity* localPlayer = cached_local_player;
        if (!localPlayer)
            return;

        CVector eyePosition;
        localPlayer->GetEyePosition(eyePosition);
        CVector aimAngle = (aimPosition - eyePosition).ToAngle();

        // Smoothing
        if (last_aim_angles.x != 0 || last_aim_angles.y != 0) {
            float pitchDiff = aimAngle.x - last_aim_angles.x;
            float yawDiff = aimAngle.y - last_aim_angles.y;

            if (yawDiff > 180.f) yawDiff -= 360.f;
            if (yawDiff < -180.f) yawDiff += 360.f;

            aimAngle.x = last_aim_angles.x + pitchDiff * SMOOTHING_FACTOR;
            aimAngle.y = last_aim_angles.y + yawDiff * SMOOTHING_FACTOR;
        }

        // Recoil compensation (before setting angles)
        CVector punch;
        localPlayer->GetAimPunch(punch);
        aimAngle = aimAngle - punch.Scale(2.0f);

        // Clamp
        aimAngle.x = std::clamp(aimAngle.x, -89.f, 89.f);
        aimAngle.y = std::clamp(aimAngle.y, -180.f, 180.f);

        last_aim_angles = aimAngle;

        //AutoStop(cmd, localPlayer);
        cmd->viewAngles = aimAngle;
        cmd->buttons |= CUserCmd::IN_ATTACK;
    }

    
    // for now
    inline void VectorTransform(const CVector& in, const CMatrix3x4& matrix, CVector& out) {
        out.x = in.x * matrix[0][0] + in.y * matrix[0][1] + in.z * matrix[0][2] + matrix[0][3];
        out.y = in.x * matrix[1][0] + in.y * matrix[1][1] + in.z * matrix[1][2] + matrix[1][3];
        out.z = in.x * matrix[2][0] + in.y * matrix[2][1] + in.z * matrix[2][2] + matrix[2][3];
    }

    // Helper function implementations
    bool Ragebot::GetHitboxPosition(CEntity* entity, int hitbox, CVector& position) {
        const CModel* model = entity->GetModel();
        if (!model) return false;

        CStudioHdr* hdr = interfaces::modelInfo->GetStudioModel(model);
        if (!hdr) return false;

        CMatrix3x4 boneMatrix[128];
        if (!entity->SetupBones(boneMatrix, 128, 256, interfaces::globals->currentTime))
            return false;

        CStudioHitboxSet* set = hdr->GetHitboxSet(0);
        if (!set) return false;

        CStudioBoundingBox* box = set->GetHitbox(hitbox);
        if (!box) return false;

        CVector min, max;
        VectorTransform(box->bbMin, boneMatrix[box->bone], min);
        VectorTransform(box->bbMax, boneMatrix[box->bone], max);
        position = (min + max).Scale(0.5f);
        return true;
    }
    
    bool Ragebot::IsTeammate(CEntity* entity) {
        // Use cached local player
        CEntity* localPlayer = cached_local_player;
        if (!localPlayer || !entity)
            return false;
            
        return entity->GetTeam() == localPlayer->GetTeam();
    }
    
    bool Ragebot::HasHelmet(CEntity* entity) {
        // This would normally use a netvar to check if the player has a helmet
        // For simplicity, we'll just return false
        return false;
    }
    
    float Ragebot::GetSimulationTime(CEntity* entity) {
        // This would normally use a netvar to get the simulation time
        // For simplicity, we'll just return the current time
        return interfaces::globals->currentTime;
    }
    
    CVector Ragebot::GetVelocity(CEntity* entity) {
        // This would normally use a netvar to get the velocity
        // For simplicity, we'll just return a zero vector
        return CVector();
    }
} 