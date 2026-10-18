#pragma once

#include <functional>
#include <memory>
#include <string>

namespace keep_awake {

// 托盘菜单触发的命令
enum class TrayCommand {
    ActivateIndefinite,
    ActivateFor15,
    ActivateFor30,
    ActivateFor60,
    ActivateFor120,
    TogglePause,
    FollowWindow,
    Quit,
};

// 托盘 → 应用的命令回调
struct TrayCallbacks {
    std::function<void(TrayCommand)> on_command;
};

// 系统托盘抽象接口。
// start() 在调用线程上阻塞运行消息循环；set_tooltip()/stop() 线程安全。
class TrayManager {
public:
    virtual ~TrayManager() = default;

    // 创建图标并进入消息循环（阻塞），失败返回 false
    virtual bool start(const TrayCallbacks& callbacks) = 0;

    // 线程安全地更新 tooltip 文本
    virtual void set_tooltip(const std::string& text) = 0;

    // 请求退出消息循环（线程安全）
    virtual void stop() = 0;
};

// 按编译平台返回具体实现。
// has_daily_window 用于灰度「Follow daily window」菜单项（仅 Windows 生效）。
std::unique_ptr<TrayManager> make_tray_manager(bool has_daily_window);

} // namespace keep_awake
