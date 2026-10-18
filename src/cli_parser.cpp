#include "cli_parser.hpp"

#include <cctype>
#include <cstdlib>

namespace keep_awake {
namespace {

// 将单个时刻（"HH"、"HH:MM"、"HHam/pm"）解析为自午夜分钟数；失败返回 false。
// 支持 12 小时制（带 am/pm 后缀）与 24 小时制（纯数字）。
bool parse_clock(const std::string& text, int& out_min) {
    if (text.empty()) return false;

    std::string t = text;

    // 提取 am/pm 后缀
    bool has_suffix = false;
    bool is_pm = false;
    if (t.size() >= 2) {
        char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(t[t.size() - 2])));
        char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(t[t.size() - 1])));
        if (c1 == 'a' && c2 == 'm') {
            has_suffix = true;
            is_pm = false;
            t = t.substr(0, t.size() - 2);
        } else if (c1 == 'p' && c2 == 'm') {
            has_suffix = true;
            is_pm = true;
            t = t.substr(0, t.size() - 2);
        }
    }

    // 拆分 hour[:min]
    std::string hstr;
    std::string mstr;
    const std::size_t colon = t.find(':');
    if (colon == std::string::npos) {
        hstr = t;
    } else {
        hstr = t.substr(0, colon);
        mstr = t.substr(colon + 1);
    }

    if (hstr.empty()) return false;
    for (char c : hstr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    for (char c : mstr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }

    int hour = std::atoi(hstr.c_str());
    int minute = mstr.empty() ? 0 : std::atoi(mstr.c_str());

    if (has_suffix) {
        // 12 小时制：12am=0、1-11am、12pm=12、1-11pm=13-23
        if (hour < 1 || hour > 12) return false;
        int h24 = hour % 12;
        if (is_pm) h24 += 12;
        hour = h24;
    } else {
        if (hour < 0 || hour > 23) return false;
    }

    if (minute < 0 || minute > 59) return false;
    out_min = hour * 60 + minute;
    return true;
}

// 解析 "start-end" 时间窗口
std::optional<TimeWindow> parse_time_window(const std::string& text, std::string& error) {
    const std::size_t dash = text.find('-');
    if (dash == std::string::npos) {
        error = "active-window must be 'start-end' (e.g. 9am-5pm or 21:00-06:00)";
        return std::nullopt;
    }

    int start = 0;
    int end = 0;
    if (!parse_clock(text.substr(0, dash), start) || !parse_clock(text.substr(dash + 1), end)) {
        error = "invalid active-window '" + text + "' (expected e.g. 9am-5pm or 21:00-06:00)";
        return std::nullopt;
    }
    if (start == end) {
        error = "active-window start and end are equal";
        return std::nullopt;
    }
    return TimeWindow{start, end};
}

// 将 "123" 解析为正整数；失败返回 false
bool parse_positive_int(const std::string& text, int& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    long n = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || n <= 0) return false;
    out = static_cast<int>(n);
    return true;
}

} // namespace

CliOptions parse_cli(int argc, char** argv) {
    CliOptions opt;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            opt.show_help = true;
        } else if (arg == "-V" || arg == "--version") {
            opt.show_version = true;
        } else if (arg == "--no-tray") {
            opt.no_tray = true;
        } else if (arg == "--tray") {
            opt.no_tray = false;
        } else if (arg == "--daemon") {
            opt.daemon = true;
        } else if (arg == "--debug") {
            opt.debug = true;
        } else if (arg == "--duration" || arg == "--active-window" || arg == "--interval") {
            if (i + 1 >= argc) {
                opt.error = "option '" + arg + "' requires a value";
                return opt;
            }
            const std::string value = argv[++i];

            if (arg == "--duration") {
                int minutes = 0;
                if (!parse_positive_int(value, minutes)) {
                    opt.error = "invalid --duration value '" + value + "' (expected positive minutes)";
                    return opt;
                }
                opt.duration_minutes = minutes;
            } else if (arg == "--interval") {
                int seconds = 0;
                if (!parse_positive_int(value, seconds)) {
                    opt.error = "invalid --interval value '" + value + "' (expected positive seconds)";
                    return opt;
                }
                opt.interval_seconds = seconds;
            } else { // --active-window
                std::string err;
                auto window = parse_time_window(value, err);
                if (!window) {
                    opt.error = err;
                    return opt;
                }
                opt.active_window = *window;
            }
        } else {
            opt.error = "unknown option '" + arg + "'";
            return opt;
        }
    }

    return opt;
}

std::string usage_text() {
    return
        "keep-awake - keep your screen awake from the tray or terminal\n"
        "\n"
        "USAGE:\n"
        "    keep-awake [OPTIONS]\n"
        "\n"
        "OPTIONS:\n"
        "    --duration <minutes>          Exit after the given number of minutes\n"
        "    --active-window <start-end>   Stay awake during a local-time window\n"
        "                                  (e.g. 9am-5pm or 21:00-06:00)\n"
        "    --interval <seconds>          Refresh interval (default: 30)\n"
        "    --no-tray                     Run without the system tray icon\n"
        "    --tray                        Explicitly enable the tray (default)\n"
        "    --daemon                      Suppress output\n"
        "    --debug                       Print state changes and refresh activity\n"
        "    -h, --help                    Print help\n"
        "    -V, --version                 Print version\n";
}

} // namespace keep_awake
