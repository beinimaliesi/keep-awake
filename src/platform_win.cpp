#include "platform.hpp"

#include <windows.h>

namespace keep_awake {
namespace {

// Windows 实现：SetThreadExecutionState 请求系统与显示器保持可用。
// 线程退出或调用 release() 时自动恢复为正常休眠。
class WinKeepAwake final : public KeepAwake {
public:
    void activate() override {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        active_ = true;
    }

    void release() override {
        SetThreadExecutionState(ES_CONTINUOUS);
        active_ = false;
    }

    bool active() const override { return active_; }

private:
    bool active_ = false;
};

} // namespace

std::unique_ptr<KeepAwake> make_keep_awake() {
    return std::make_unique<WinKeepAwake>();
}

} // namespace keep_awake
