#include "app.hpp"

#include <cstdio>
#include <ctime>
#include <iostream>

namespace keep_awake {
namespace {

// 当前时刻的自午夜分钟数（本地时区）
int local_minutes_since_midnight() {
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm ltm{};
#ifdef _WIN32
    localtime_s(&ltm, &t);
#else
    localtime_r(&t, &ltm);
#endif
    return ltm.tm_hour * 60 + ltm.tm_min;
}

// 将「自午夜分钟数」格式化为 "HH:MM"
std::string format_minutes(int m) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
    return std::string(buf);
}

} // namespace

App::App(std::unique_ptr<KeepAwake> keep_awake, CliOptions options)
    : keep_awake_(std::move(keep_awake)),
      options_(std::move(options)) {
    // 依据 CLI 参数确定初始模式
    if (options_.duration_minutes) {
        mode_ = Mode::Duration;
        deadline_ = Clock::now() + std::chrono::minutes(*options_.duration_minutes);
    } else if (options_.active_window) {
        mode_ = Mode::Window;
        configured_window_ = true;
    } else {
        mode_ = Mode::Indefinite;
    }
}

void App::set_tooltip_callback(std::function<void(const std::string&)> cb) {
    tooltip_cb_ = std::move(cb);
}

void App::post_tray_command(TrayCommand cmd) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        commands_.push_back(cmd);
    }
    cv_.notify_one();
}

int App::run() {
    const auto interval = std::chrono::seconds(options_.interval_seconds);

    while (!quit_.load()) {
        // 1. 排空托盘命令
        {
            std::lock_guard<std::mutex> lock(mtx_);
            while (!commands_.empty()) {
                const TrayCommand cmd = commands_.front();
                commands_.pop_front();
                apply_command(cmd);
            }
        }
        if (quit_.load()) break;

        // 2. 评估并应用激活/释放状态
        tick();
        if (quit_.load()) break;

        // 3. 刷新托盘 tooltip
        if (tooltip_cb_) tooltip_cb_(status_text());

        // 4. 等待下一刷新周期，或命令到达提前唤醒
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, interval);
    }

    // 退出前统一释放防休眠状态
    keep_awake_->release();
    if (!options_.daemon) {
        std::cout << "keep-awake state released." << std::endl;
    }
    return 0;
}

void App::tick() {
    bool desired = should_be_active();

    // Duration 模式到期 → 请求退出
    if (mode_ == Mode::Duration && deadline_ && Clock::now() >= *deadline_) {
        if (!options_.daemon) {
            std::cout << "duration finished, exiting." << std::endl;
        }
        quit_.store(true);
        desired = false;
    }

    const bool currently = keep_awake_->active();
    if (desired && !currently) {
        keep_awake_->activate();
        if (options_.debug && !options_.daemon) {
            std::cout << "[debug] keep-awake activated" << std::endl;
        }
    } else if (!desired && currently) {
        keep_awake_->release();
        if (options_.debug && !options_.daemon) {
            std::cout << "[debug] keep-awake released" << std::endl;
        }
    } else if (desired && currently) {
        keep_awake_->refresh();
        if (options_.debug && !options_.daemon) {
            std::cout << "[debug] keep-awake refreshed" << std::endl;
        }
    }
}

bool App::should_be_active() const {
    if (paused_) return false;
    switch (mode_) {
        case Mode::Indefinite: return true;
        case Mode::Duration: return deadline_ && Clock::now() < *deadline_;
        case Mode::Window: return in_window();
    }
    return false;
}

bool App::in_window() const {
    if (!options_.active_window) return false;
    const TimeWindow& w = *options_.active_window;
    const int now = local_minutes_since_midnight();
    if (w.start_min <= w.end_min) {
        return now >= w.start_min && now < w.end_min;
    }
    // 跨午夜窗口
    return now >= w.start_min || now < w.end_min;
}

void App::apply_command(TrayCommand cmd) {
    switch (cmd) {
        case TrayCommand::ActivateIndefinite:
            mode_ = Mode::Indefinite;
            deadline_.reset();
            paused_ = false;
            break;
        case TrayCommand::ActivateFor15:
            mode_ = Mode::Duration;
            deadline_ = Clock::now() + std::chrono::minutes(15);
            paused_ = false;
            break;
        case TrayCommand::ActivateFor30:
            mode_ = Mode::Duration;
            deadline_ = Clock::now() + std::chrono::minutes(30);
            paused_ = false;
            break;
        case TrayCommand::ActivateFor60:
            mode_ = Mode::Duration;
            deadline_ = Clock::now() + std::chrono::minutes(60);
            paused_ = false;
            break;
        case TrayCommand::ActivateFor120:
            mode_ = Mode::Duration;
            deadline_ = Clock::now() + std::chrono::minutes(120);
            paused_ = false;
            break;
        case TrayCommand::TogglePause:
            paused_ = !paused_;
            break;
        case TrayCommand::FollowWindow:
            if (configured_window_) {
                mode_ = Mode::Window;
                deadline_.reset();
                paused_ = false;
            }
            break;
        case TrayCommand::Quit:
            quit_.store(true);
            break;
    }
}

std::string App::status_text() const {
    if (paused_) return "Paused";

    const bool active = should_be_active();
    switch (mode_) {
        case Mode::Indefinite:
            return active ? "Active (until stopped)" : "Stopped";
        case Mode::Duration:
            if (deadline_) {
                const auto remain = std::chrono::duration_cast<std::chrono::seconds>(*deadline_ - Clock::now());
                const auto minutes = remain.count() / 60;
                return "Active (" + std::to_string(minutes) + " min left)";
            }
            return "Active";
        case Mode::Window:
            if (options_.active_window) {
                if (active) {
                    return "Active (window until " + format_minutes(options_.active_window->end_min) + ")";
                }
                return "Next window at " + format_minutes(options_.active_window->start_min);
            }
            return "Stopped";
    }
    return "Active";
}

} // namespace keep_awake
