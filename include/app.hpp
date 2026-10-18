#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "cli_parser.hpp"
#include "platform.hpp"
#include "tray_manager.hpp"

namespace keep_awake {

// 激活模式：无限 / 定时 / 每日窗口
enum class Mode { Indefinite, Duration, Window };

// 核心应用：状态机 + 主循环。
// 有效状态 = (模式判定 should_be_active()) 且 (非 paused)。
class App {
public:
    App(std::unique_ptr<KeepAwake> keep_awake, CliOptions options);

    // 主循环（阻塞），返回进程退出码
    int run();

    // 线程安全：供托盘线程投递命令
    void post_tray_command(TrayCommand cmd);

    // 设置 tooltip 更新回调（main 中接入 TrayManager）
    void set_tooltip_callback(std::function<void(const std::string&)> cb);

private:
    using Clock = std::chrono::system_clock;

    void tick();                                  // 评估并应用激活/释放状态
    bool should_be_active() const;                // 当前是否应处于激活态
    bool in_window() const;                       // 当前时刻是否落在每日窗口内
    void apply_command(TrayCommand cmd);          // 处理托盘命令
    std::string status_text() const;              // 用于 tooltip 的状态描述

    std::unique_ptr<KeepAwake> keep_awake_;
    CliOptions options_;
    Mode mode_;
    std::optional<Clock::time_point> deadline_;   // Duration 模式的到期时刻
    bool paused_ = false;
    bool configured_window_ = false;              // 是否配置了 --active-window
    std::atomic<bool> quit_{false};

    std::function<void(const std::string&)> tooltip_cb_;

    // 命令队列（互斥锁 + 条件变量，托盘线程 → 主循环）
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<TrayCommand> commands_;
};

} // namespace keep_awake
