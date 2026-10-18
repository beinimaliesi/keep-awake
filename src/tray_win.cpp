#include "tray_manager.hpp"

#include "resource.h"

#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <mutex>
#include <string>

namespace keep_awake {
namespace {

// 自定义窗口消息
constexpr UINT WM_TRAYICON = WM_APP + 1;       // 托盘图标回调（点击）
constexpr UINT WM_UPDATE_TOOLTIP = WM_APP + 2; // 请求更新 tooltip

// 菜单命令 ID
enum MenuId : UINT {
    MI_ACTIVATE_INDEFINITE = 1000,
    MI_ACTIVATE_15,
    MI_ACTIVATE_30,
    MI_ACTIVATE_60,
    MI_ACTIVATE_120,
    MI_TOGGLE_PAUSE,
    MI_FOLLOW_WINDOW,
    MI_QUIT,
};

class WinTrayManager final : public TrayManager {
public:
    explicit WinTrayManager(bool has_daily_window) : has_daily_window_(has_daily_window) {}

    bool start(const TrayCallbacks& callbacks) override {
        callbacks_ = callbacks;

        // 注册隐藏窗口类，接收托盘消息
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &WinTrayManager::wnd_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"KeepAwakeTrayWindow";
        RegisterClassExW(&wc);

        hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"KeepAwake", 0,
                                0, 0, 0, 0, nullptr, nullptr, wc.hInstance, this);
        if (!hwnd_) return false;

        // 添加托盘图标
        nid_ = {};
        nid_.cbSize = sizeof(nid_);
        nid_.hWnd = hwnd_;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;
        nid_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_TRAY));
        if (!nid_.hIcon) {
            // 兜底：资源缺失时退回系统图标
            nid_.hIcon = LoadIcon(nullptr, IDI_INFORMATION);
        }
        lstrcpyW(nid_.szTip, L"keep-awake");
        if (!Shell_NotifyIconW(NIM_ADD, &nid_)) return false;

        // 消息循环
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return true;
    }

    void set_tooltip(const std::string& text) override {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tooltip_ = to_wide(text);
        }
        // 通知窗口线程刷新 tooltip（拷贝数据，无需担心生命周期）
        if (hwnd_) PostMessageW(hwnd_, WM_UPDATE_TOOLTIP, 0, 0);
    }

    void stop() override {
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WinTrayManager* self = nullptr;
        if (msg == WM_NCCREATE) {
            const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<WinTrayManager*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<WinTrayManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self) return self->handle_message(hwnd, msg, wParam, lParam);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_TRAYICON:
                if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                    show_menu(hwnd);
                }
                return 0;

            case WM_COMMAND:
                handle_menu(LOWORD(wParam));
                return 0;

            case WM_UPDATE_TOOLTIP: {
                std::lock_guard<std::mutex> lock(mtx_);
                // szTip 容量为 128 个 WCHAR，末尾需保留 NUL
                const std::size_t len = (tooltip_.size() < 127) ? tooltip_.size() : 127;
                std::wmemcpy(nid_.szTip, tooltip_.c_str(), len);
                nid_.szTip[len] = L'\0';
                nid_.uFlags = NIF_TIP;
                Shell_NotifyIconW(NIM_MODIFY, &nid_);
                return 0;
            }

            case WM_DESTROY:
                Shell_NotifyIconW(NIM_DELETE, &nid_);
                PostQuitMessage(0);
                return 0;

            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    void show_menu(HWND hwnd) {
        HMENU menu = CreatePopupMenu();

        AppendMenuW(menu, MF_STRING, MI_ACTIVATE_INDEFINITE, L"Until stopped");
        AppendMenuW(menu, MF_STRING, MI_ACTIVATE_15, L"15 minutes");
        AppendMenuW(menu, MF_STRING, MI_ACTIVATE_30, L"30 minutes");
        AppendMenuW(menu, MF_STRING, MI_ACTIVATE_60, L"1 hour");
        AppendMenuW(menu, MF_STRING, MI_ACTIVATE_120, L"2 hours");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, MI_TOGGLE_PAUSE, L"Pause / Resume");
        AppendMenuW(menu, MF_STRING, MI_FOLLOW_WINDOW, L"Follow daily window");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, MI_QUIT, L"Quit");

        // 无每日窗口时灰度「Follow daily window」
        EnableMenuItem(menu, MI_FOLLOW_WINDOW,
                       has_daily_window_ ? MF_ENABLED : MF_GRAYED);

        SetForegroundWindow(hwnd);
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
    }

    void handle_menu(UINT id) {
        TrayCommand cmd;
        switch (id) {
            case MI_ACTIVATE_INDEFINITE: cmd = TrayCommand::ActivateIndefinite; break;
            case MI_ACTIVATE_15: cmd = TrayCommand::ActivateFor15; break;
            case MI_ACTIVATE_30: cmd = TrayCommand::ActivateFor30; break;
            case MI_ACTIVATE_60: cmd = TrayCommand::ActivateFor60; break;
            case MI_ACTIVATE_120: cmd = TrayCommand::ActivateFor120; break;
            case MI_TOGGLE_PAUSE: cmd = TrayCommand::TogglePause; break;
            case MI_FOLLOW_WINDOW: cmd = TrayCommand::FollowWindow; break;
            case MI_QUIT: cmd = TrayCommand::Quit; break;
            default: return;
        }
        if (callbacks_.on_command) callbacks_.on_command(cmd);
    }

    static std::wstring to_wide(const std::string& s) {
        if (s.empty()) return L"";
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
        if (!w.empty() && w.back() == L'\0') w.pop_back();
        return w;
    }

    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_{};
    TrayCallbacks callbacks_;
    bool has_daily_window_ = false;
    std::mutex mtx_;
    std::wstring tooltip_ = L"keep-awake";
};

} // namespace

std::unique_ptr<TrayManager> make_tray_manager(bool has_daily_window) {
    return std::make_unique<WinTrayManager>(has_daily_window);
}

} // namespace keep_awake
