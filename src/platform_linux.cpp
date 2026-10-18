#include "platform.hpp"

#include <cstdlib>

namespace keep_awake {
namespace {

// Linux 占位实现：没有跨桌面统一的持久断言，通过 xdg-screensaver reset 周期续期。
// activate()/release() 仅维护标志位，真正的抑制动作由 refresh() 在每个刷新周期执行一次。
class LinuxKeepAwake final : public KeepAwake {
public:
    void activate() override { active_ = true; }

    void release() override { active_ = false; }

    void refresh() override {
        if (active_) {
            std::system("xdg-screensaver reset >/dev/null 2>&1");
        }
    }

    bool active() const override { return active_; }

private:
    bool active_ = false;
};

} // namespace

std::unique_ptr<KeepAwake> make_keep_awake() {
    return std::make_unique<LinuxKeepAwake>();
}

} // namespace keep_awake
