#include <iostream>
#include <memory>
#include <thread>

#include "app.hpp"
#include "cli_parser.hpp"
#include "platform.hpp"
#include "tray_manager.hpp"
#include "version.hpp"

int main(int argc, char** argv) {
    using namespace keep_awake;

    const CliOptions opts = parse_cli(argc, argv);

    if (opts.show_help) {
        std::cout << usage_text();
        return 0;
    }
    if (opts.show_version) {
        std::cout << "keep-awake " << KEEP_AWAKE_VERSION << std::endl;
        return 0;
    }
    if (!opts.error.empty()) {
        std::cerr << "error: " << opts.error << "\n\n" << usage_text();
        return 2;
    }

    auto keep_awake = make_keep_awake();
    if (!keep_awake) {
        std::cerr << "error: keep-awake mechanism not available on this platform" << std::endl;
        return 1;
    }

    App app(std::move(keep_awake), opts);

    // 托盘（可选，默认启用）。托盘跑在独立线程，通过命令队列与主循环解耦。
    std::unique_ptr<TrayManager> tray;
    std::thread tray_thread;
    if (!opts.no_tray) {
        tray = make_tray_manager(opts.active_window.has_value());

        TrayCallbacks callbacks;
        callbacks.on_command = [&app](TrayCommand cmd) { app.post_tray_command(cmd); };
        app.set_tooltip_callback([&tray](const std::string& text) {
            if (tray) tray->set_tooltip(text);
        });

        tray_thread = std::thread([&tray, &callbacks] {
            if (tray) tray->start(callbacks);
        });
    }

    const int rc = app.run();

    if (tray) tray->stop();
    if (tray_thread.joinable()) tray_thread.join();

    return rc;
}
