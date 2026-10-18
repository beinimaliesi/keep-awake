#pragma once

#include <memory>

namespace keep_awake {

// 平台防休眠抽象接口。
// 各平台通过 make_keep_awake() 返回具体实现（Windows / macOS / Linux）。
// 设计遵循 SOLID 的 D 原则：应用层只依赖此抽象，不关心平台原生 API。
class KeepAwake {
public:
    virtual ~KeepAwake() = default;

    // 请求保持系统与显示器唤醒
    virtual void activate() = 0;

    // 释放请求，恢复正常休眠
    virtual void release() = 0;

    // 可选周期性续期：部分平台（如 Linux）没有持久断言，需要定时刷新。
    // 默认空实现，Windows / macOS 无需重写。
    virtual void refresh() {}

    // 当前是否处于激活态
    virtual bool active() const = 0;
};

// 按编译平台返回具体实现
std::unique_ptr<KeepAwake> make_keep_awake();

} // namespace keep_awake
