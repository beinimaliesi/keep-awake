#include "tray_manager.hpp"

namespace keep_awake {
namespace {

// Linux 托盘占位实现：需要 GTK / AppIndicator 集成（参考实现同样依赖额外系统包）。
// 暂不支持：start() 返回 false 表示托盘不可用，应用将退化为无托盘 / headless 运行。
class LinuxTrayManager final : public TrayManager {
public:
    bool start(const TrayCallbacks&) override { return false; }
    void set_tooltip(const std::string&) override {}
    void stop() override {}
};

} // namespace

std::unique_ptr<TrayManager> make_tray_manager(bool) {
    return std::make_unique<LinuxTrayManager>();
}

} // namespace keep_awake
