#include "hooks.h"
#include "../util/memory.h"
#include "../../ext/minhook/minhook.h"
#include "../../ext/x86retspoof/x86RetSpoof.h"
#include <intrin.h>
#include "../hacks/misc.h"

// Forward declare ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Helper to get D3D device safely
IDirect3DDevice9* GetD3DDevice(HWND window) {
	static IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d) return nullptr;

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = window;
	pp.BackBufferFormat = D3DFMT_UNKNOWN;
	pp.BackBufferWidth = 0;    // Use window width
	pp.BackBufferHeight = 0;   // Use window height
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D16;

	IDirect3DDevice9* device = nullptr;
	if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device))) {
		return nullptr;
	}

	return device;
}

void hooks::Setup() noexcept
{
	MH_Initialize();

	// AllocKeyValuesMemory hook
	MH_CreateHook(
		memory::Get(interfaces::keyValuesSystem, 1),
		&AllocKeyValuesMemory,
		reinterpret_cast<void**>(&AllocKeyValuesMemoryOriginal)
	);

	// CreateMove hook
	MH_CreateHook(
		memory::Get(interfaces::clientMode, 24),
		&CreateMove,
		reinterpret_cast<void**>(&CreateMoveOriginal)
	);

	// Find CSGO window
	HWND window = FindWindowA("Valve001", nullptr);
	if (window) {
		// Hook WndProc
		OriginalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

		// Get D3D9 device pointer safely
		IDirect3DDevice9* device = GetD3DDevice(window);
		if (device) {
			void** vTable = *reinterpret_cast<void***>(device);
			device->Release(); // Release our temporary device

			// Hook the actual game's EndScene
			MH_CreateHook(
				vTable[42], // EndScene is at index 42
				&EndScene,
				reinterpret_cast<void**>(&EndSceneOriginal)
			);
		}
	}

	MH_EnableHook(MH_ALL_HOOKS);
}

void hooks::Destroy() noexcept
{
	// restore hooks
	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);

	// Restore WndProc
	HWND window = FindWindowA("Valve001", nullptr);
	if (window && OriginalWndProc)
		SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)OriginalWndProc);

	// Shutdown menu
	menu::Menu::Get().Shutdown();

	// uninit minhook
	MH_Uninitialize();
}

void* __stdcall hooks::AllocKeyValuesMemory(const std::int32_t size) noexcept
{
	// if function is returning to speficied addresses, return nullptr to "bypass"
	if (const std::uint32_t address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesEngine) ||
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesClient)) 
		return nullptr;

	// return original
	return AllocKeyValuesMemoryOriginal(interfaces::keyValuesSystem, size);
}

bool __stdcall hooks::CreateMove(float frameTime, CUserCmd* cmd) noexcept
{
	static const auto sequence = reinterpret_cast<std::uintptr_t>(memory::PatternScan("client.dll", "FF 23"));
	const auto result = x86RetSpoof::invokeStdcall<bool>((uintptr_t)hooks::CreateMoveOriginal, sequence, frameTime, cmd);

	// make sure this function is being called from CInput::CreateMove
	if (!cmd || !cmd->commandNumber)
		return result;

	// this would be done anyway by returning true
	if (CreateMoveOriginal(interfaces::clientMode, frameTime, cmd))
		interfaces::engine->SetViewAngles(cmd->viewAngles);

	// get our local player here
	globals::UpdateLocalPlayer();

	if (globals::localPlayer && globals::localPlayer->IsAlive())
	{
		// example bhop
		hacks::RunBunnyHop(cmd);
	}

	return false;
}

LRESULT __stdcall hooks::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	if (!hWnd) 
		return DefWindowProc(hWnd, msg, wParam, lParam);

	try {
		// Toggle menu with Insert key
		if (msg == WM_KEYUP && wParam == VK_INSERT) {
			menu::Menu::Get().Toggle();
			return true;
		}

		// Handle input when menu is visible
		if (menu::Menu::Get().IsVisible()) {
			// Pass events to ImGui
			if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
				return true;

			// Block game input when interacting with menu
			switch (msg) {
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				case WM_MOUSEWHEEL:
				case WM_MOUSEMOVE:
				case WM_KEYDOWN:
				case WM_KEYUP:
					return true;
			}
		}

		if (OriginalWndProc)
			return CallWindowProc(OriginalWndProc, hWnd, msg, wParam, lParam);
	}
	catch (...) {
		// If anything fails, use default window procedure
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

HRESULT __stdcall hooks::EndScene(IDirect3DDevice9* device) noexcept
{
	if (!device) return EndSceneOriginal(device);

	static bool init = false;
	if (!init) {
		try {
			menu::Menu::Get().Initialize(device);
			init = true;
		}
		catch (...) {
			// If initialization fails, continue without the menu
			return EndSceneOriginal(device);
		}
	}

	if (init) {
		try {
			menu::Menu::Get().Render();
		}
		catch (...) {
			// If rendering fails, continue without the menu
		}
	}

	return EndSceneOriginal(device);
}
