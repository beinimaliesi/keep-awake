#include "tray_manager.hpp"

// macOS 托盘实现：NSStatusItem + 菜单。
// 注意：本文件未在 macOS 环境验证，交由 CI 的 macos-latest runner 编译校验。
// 简化约定：不启用 ARC，一次性创建的对象在进程生命周期内常驻（轻微泄漏可接受）。
#import <Cocoa/Cocoa.h>

// Objective-C 的 @interface/@implementation 必须位于全局作用域，
// 不能放进 C++ namespace，故相关声明移出 keep_awake 命名空间。
namespace {

// 全局回调存储（macOS 托盘由单一实例使用）
keep_awake::TrayCallbacks g_callbacks;

// 菜单项 tag → TrayCommand 的映射
enum : NSInteger {
    kTagIndefinite = 1,
    kTag15 = 2,
    kTag30 = 3,
    kTag60 = 4,
    kTag120 = 5,
    kTagTogglePause = 6,
    kTagFollowWindow = 7,
    kTagQuit = 8,
};

} // namespace

@interface KeepAwakeTrayDelegate : NSObject {
@public
    NSStatusItem* statusItem;
}
- (void)menuAction:(id)sender;
@end

@implementation KeepAwakeTrayDelegate
- (void)menuAction:(id)sender {
    NSInteger tag = [(NSMenuItem*)sender tag];
    keep_awake::TrayCommand cmd;
    switch (tag) {
        case kTagIndefinite: cmd = keep_awake::TrayCommand::ActivateIndefinite; break;
        case kTag15: cmd = keep_awake::TrayCommand::ActivateFor15; break;
        case kTag30: cmd = keep_awake::TrayCommand::ActivateFor30; break;
        case kTag60: cmd = keep_awake::TrayCommand::ActivateFor60; break;
        case kTag120: cmd = keep_awake::TrayCommand::ActivateFor120; break;
        case kTagTogglePause: cmd = keep_awake::TrayCommand::TogglePause; break;
        case kTagFollowWindow: cmd = keep_awake::TrayCommand::FollowWindow; break;
        case kTagQuit: cmd = keep_awake::TrayCommand::Quit; break;
        default: return;
    }
    if (g_callbacks.on_command) g_callbacks.on_command(cmd);
}
@end

namespace keep_awake {

class MacTrayManager final : public TrayManager {
public:
    explicit MacTrayManager(bool /*has_daily_window*/) {}

    bool start(const TrayCallbacks& callbacks) override {
        g_callbacks = callbacks;

        delegate_ = [[KeepAwakeTrayDelegate alloc] init];

        NSStatusBar* bar = [NSStatusBar systemStatusBar];
        NSStatusItem* item = [bar statusItemWithLength:NSVariableStatusItemLength];
        delegate_->statusItem = item;
        item.button.title = @"keep-awake";

        NSMenu* menu = [[NSMenu alloc] init];
        [self buildMenuItem:menu title:@"Until stopped" tag:kTagIndefinite];
        [self buildMenuItem:menu title:@"15 minutes" tag:kTag15];
        [self buildMenuItem:menu title:@"30 minutes" tag:kTag30];
        [self buildMenuItem:menu title:@"1 hour" tag:kTag60];
        [self buildMenuItem:menu title:@"2 hours" tag:kTag120];
        [menu addItem:[NSMenuItem separatorItem]];
        [self buildMenuItem:menu title:@"Pause / Resume" tag:kTagTogglePause];
        [self buildMenuItem:menu title:@"Follow daily window" tag:kTagFollowWindow];
        [menu addItem:[NSMenuItem separatorItem]];
        [self buildMenuItem:menu title:@"Quit" tag:kTagQuit];

        item.menu = menu;

        [NSApplication sharedApplication];
        [NSApp run];
        return true;
    }

    void set_tooltip(const std::string& text) override {
        // 简化实现：直接更新按钮标题。正式实现应派发到主线程。
        NSString* t = [NSString stringWithUTF8String:text.c_str()];
        if (delegate_ && delegate_->statusItem) {
            delegate_->statusItem.button.title = t;
        }
    }

    void stop() override {
        [NSApp terminate:nil];
    }

private:
    // C++ 成员函数用于复用菜单项构造逻辑，避免重复（DRY）
    void buildMenuItem(NSMenu* menu, NSString* title, NSInteger tag) {
        NSMenuItem* mi = [[NSMenuItem alloc] initWithTitle:title
                                                    action:@selector(menuAction:)
                                             keyEquivalent:@""];
        mi.target = delegate_;
        mi.tag = tag;
        [menu addItem:mi];
    }

    KeepAwakeTrayDelegate* delegate_ = nullptr;
};

} // namespace

std::unique_ptr<TrayManager> make_tray_manager(bool has_daily_window) {
    return std::make_unique<MacTrayManager>(has_daily_window);
}

} // namespace keep_awake
