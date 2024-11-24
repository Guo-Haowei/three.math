#include "cocoa_display_manager.h"

#import <Cocoa/Cocoa.h>

namespace my {

class NSWindowWrapperImpl {
public:
    auto Initialize(const DisplayManager::CreateInfo& p_info) -> Result<void>;

private:
    NSWindow* window_; // Pointer to the NSWindow
    bool shouldClose_;

    friend class CocoaDisplayManager;
};

auto NSWindowWrapperImpl::Initialize(const DisplayManager::CreateInfo& p_info) -> Result<void> {
            // Initialize the application
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]; // Ensure regular activation

    window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, p_info.width, p_info.height)
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
    NSString *title = [NSString stringWithUTF8String:p_info.title.c_str()];
    [window_ setTitle:title];

    [window_ cascadeTopLeftFromPoint:NSMakePoint(20, 20)];
    [window_ setMinSize:NSMakeSize(300, 200)];
    [window_ setAcceptsMouseMovedEvents:YES];
    [window_ makeKeyAndOrderFront:nil];
    [window_ center];

    // Activate the application
    [NSApp activateIgnoringOtherApps:YES];
    [window_ makeKeyAndOrderFront:nil];

    // Add a close button handler
    shouldClose_ = NO;
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                                                          object:window_
                                                           queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* note) {
        unused(note);
        shouldClose_ = YES;
    }];

    return Result<void>();
}

CocoaDisplayManager::CocoaDisplayManager() {
    m_impl = std::make_shared<NSWindowWrapperImpl>();
}

void CocoaDisplayManager::Finalize() {
}

bool CocoaDisplayManager::ShouldClose() {
    @autoreleasepool {
        // Process events manually
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:[NSDate distantPast]
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES])) {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }

        // Your custom logic here
        // For example, print "running" to simulate custom tasks
        // NSLog(@"Running custom tasks...");
                
        // Add a small delay to avoid high CPU usage
        [NSThread sleepForTimeInterval:0.016]; // ~60 FPS (16ms)
    }

    return m_impl->shouldClose_;
}

std::tuple<int, int> CocoaDisplayManager::GetWindowSize() {
    return std::make_tuple(0, 0);
}

std::tuple<int, int> CocoaDisplayManager::GetWindowPos() {
    return std::make_tuple(0, 0);
}

void CocoaDisplayManager::BeginFrame() {

}

void CocoaDisplayManager::Present() {

}

auto CocoaDisplayManager::InitializeWindow(const CreateInfo& p_info) -> Result<void> {
    return m_impl->Initialize(p_info);
}

void CocoaDisplayManager::InitializeKeyMapping() {

}

}
