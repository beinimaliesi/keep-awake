#pragma once

#include <optional>
#include <string>

namespace keep_awake {

// 每日时间窗口：以「自午夜起的分钟数」表示。
// 当 start_min > end_min 时表示跨午夜窗口（如 21:00-06:00）。
struct TimeWindow {
    int start_min;
    int end_min;
};

// 命令行解析结果
struct CliOptions {
    std::optional<int> duration_minutes;     // --duration <minutes>
    std::optional<TimeWindow> active_window; // --active-window "9am-5pm"
    int interval_seconds = 30;               // --interval <seconds>
    bool no_tray = false;                    // --no-tray
    bool daemon = false;                     // --daemon（静默）
    bool debug = false;                      // --debug（打印状态变化）
    bool show_help = false;                  // -h / --help
    bool show_version = false;               // -V / --version
    std::string error;                       // 解析错误信息，空表示成功
};

// 解析命令行参数
CliOptions parse_cli(int argc, char** argv);

// 帮助文本（写入 stdout）
std::string usage_text();

} // namespace keep_awake
