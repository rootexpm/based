#pragma once
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include "../core/interfaces.h"
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_impl_dx9.h"
#include "../../ext/imgui/imgui_impl_win32.h"

namespace menu {
    class Menu {
    public:
        static Menu& Get() {
            static Menu instance;
            return instance;
        }

        void Initialize(IDirect3DDevice9* device);
        void Render();
        void Shutdown();
        
        bool IsVisible() const { return m_visible; }
        void Toggle() { 
            m_visible = !m_visible; 
            UpdateCursor();
        }

    private:
        Menu() = default;
        ~Menu() = default;
        Menu(const Menu&) = delete;
        Menu& operator=(const Menu&) = delete;

        void RenderAimbotTab();
        void RenderVisualsTab();
        void RenderMiscTab();
        void UpdateCursor();

        bool m_visible = false;
        bool m_initialized = false;
        IDirect3DDevice9* m_device = nullptr;
        bool m_oldCursorState = false;
    };
} 