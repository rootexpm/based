#include "menu.h"
#include "../core/globals.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace menu {
    void Menu::Initialize(IDirect3DDevice9* device) {
        if (!device || m_initialized) return;

        try {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            if (!ImGui::GetCurrentContext()) return;

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange; // Allow ImGui to manage cursor

            // Setup style
            ImGui::StyleColorsDark();
            auto style = &ImGui::GetStyle();
            style->WindowRounding = 5.0f;
            style->FrameRounding = 4.0f;
            style->GrabRounding = 3.0f;

            // Setup platform/renderer bindings
            HWND window = FindWindowA("Valve001", nullptr);
            if (!window) return;

            if (!ImGui_ImplWin32_Init(window)) {
                ImGui::DestroyContext();
                return;
            }

            if (!ImGui_ImplDX9_Init(device)) {
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return;
            }

            m_device = device;
            m_initialized = true;
            m_oldCursorState = false;
            UpdateCursor(); // Initialize cursor state
        }
        catch (...) {
            Shutdown();
        }
    }

    void Menu::UpdateCursor() {
        if (!m_initialized) return;

        static HCURSOR gameAimCursor = CreateCursor(nullptr, 0, 0, 32, 32, nullptr, nullptr);
        
        if (m_visible) {
            // Store current cursor visibility state if menu is being opened
            m_oldCursorState = ShowCursor(TRUE) >= 0;
            while (ShowCursor(TRUE) < 0) {} // Make sure cursor is visible
        }
        else {
            // Restore previous cursor state when menu is closed
            if (!m_oldCursorState) {
                while (ShowCursor(FALSE) >= 0) {} // Hide cursor if it was hidden before
            }
        }

        // Update game's input mode
        if (interfaces::inputSystem) {
            interfaces::inputSystem->EnableInput(!m_visible);
        }
    }

    void Menu::RenderAimbotTab() {
        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Checkbox("Enable Aimbot", &globals::config.aimbot.enabled);
            if (globals::config.aimbot.enabled) {
                ImGui::SliderFloat("FOV", &globals::config.aimbot.fov, 0.0f, 180.0f, "%.1f");
                ImGui::SliderFloat("Smooth", &globals::config.aimbot.smooth, 1.0f, 100.0f, "%.1f");
            }
            ImGui::EndTabItem();
        }
    }

    void Menu::RenderVisualsTab() {
        if (ImGui::BeginTabItem("Visuals")) {
            ImGui::Checkbox("ESP Enabled", &globals::config.visuals.esp_enabled);
            if (globals::config.visuals.esp_enabled) {
                ImGui::Checkbox("Box ESP", &globals::config.visuals.box_esp);
                ImGui::Checkbox("Name ESP", &globals::config.visuals.name_esp);
                ImGui::Checkbox("Health ESP", &globals::config.visuals.health_esp);
            }
            ImGui::EndTabItem();
        }
    }

    void Menu::RenderMiscTab() {
        if (ImGui::BeginTabItem("Misc")) {
            ImGui::Checkbox("Bhop", &globals::config.misc.bhop);
            ImGui::Checkbox("No Flash", &globals::config.misc.no_flash);
            ImGui::Checkbox("Radar Hack", &globals::config.misc.radar_hack);
            ImGui::EndTabItem();
        }
    }

    void Menu::Render() {
        if (!m_initialized || !m_visible)
            return;

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Based CSGO", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        if (ImGui::BeginTabBar("MainTabs")) {
            RenderAimbotTab();
            RenderVisualsTab();
            RenderMiscTab();
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    void Menu::Shutdown() {
        if (!m_initialized)
            return;

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        m_device = nullptr;
        m_initialized = false;
    }
} 