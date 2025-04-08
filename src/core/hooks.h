#pragma once
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include "../valve/cusercmd.h"
#include "interfaces.h"
#include "../menu/menu.h"
#include <cstdint>

namespace hooks
{
	// call once to emplace all hooks
	void Setup() noexcept;

	// call once to restore all hooks
	void Destroy() noexcept;

	// bypass return address checks (thx osiris)
	using AllocKeyValuesMemoryFn = void* (__thiscall*)(void*, const std::int32_t) noexcept;
	inline AllocKeyValuesMemoryFn AllocKeyValuesMemoryOriginal;
	void* __stdcall AllocKeyValuesMemory(const std::int32_t size) noexcept;

	// example CreateMove hook
	using CreateMoveFn = bool(__thiscall*)(IClientModeShared*, float, CUserCmd*) noexcept;
	inline CreateMoveFn CreateMoveOriginal = nullptr;
	bool __stdcall CreateMove(float frameTime, CUserCmd* cmd) noexcept;

	// New hooks
	inline HRESULT(__stdcall* EndSceneOriginal)(IDirect3DDevice9*);
	inline WNDPROC OriginalWndProc;
	HRESULT __stdcall EndScene(IDirect3DDevice9* device) noexcept;
	LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
}
