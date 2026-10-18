#include "platform.hpp"

// macOS 实现：IOPMAssertion 原生防休眠断言。
// 注意：本文件未在 macOS 环境验证，交由 CI 的 macos-latest runner 编译校验。
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <CoreFoundation/CoreFoundation.h>

namespace keep_awake {
namespace {

class MacKeepAwake final : public KeepAwake {
public:
    void activate() override {
        if (assertion_id_ == kIOPMNullAssertionID) {
            CFStringRef reason = CFSTR("keep-awake");
            IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                                        kIOPMAssertionLevelOn, reason, &assertion_id_);
        }
        active_ = true;
    }

    void release() override {
        if (assertion_id_ != kIOPMNullAssertionID) {
            IOPMAssertionRelease(assertion_id_);
            assertion_id_ = kIOPMNullAssertionID;
        }
        active_ = false;
    }

    bool active() const override { return active_; }

private:
    IOPMAssertionID assertion_id_ = kIOPMNullAssertionID;
    bool active_ = false;
};

} // namespace

std::unique_ptr<KeepAwake> make_keep_awake() {
    return std::make_unique<MacKeepAwake>();
}

} // namespace keep_awake
