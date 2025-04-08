#pragma once
#include "../valve/centity.h"

namespace globals {
	inline CEntity* localPlayer = nullptr;

	// update the local player pointer
	void UpdateLocalPlayer() noexcept;

	struct Config {
		struct {
			bool enabled = false;
			float fov = 90.0f;
			float smooth = 1.0f;
		} aimbot;

		struct {
			bool esp_enabled = false;
			bool box_esp = false;
			bool name_esp = false;
			bool health_esp = false;
		} visuals;

		struct {
			bool bhop = false;
			bool no_flash = false;
			bool radar_hack = false;
		} misc;
	};

	inline Config config;
}
