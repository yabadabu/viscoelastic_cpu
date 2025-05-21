#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CVDisplayLink.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "apple_platform.h"

// https://github.com/gamedevtech/CocoaOpenGLWindow
// https://github.com/metal-by-example/learn-metal-cpp-ios/blob/master/learn-metal/05-perspective/05-perspective.cpp

@class View;

@interface MyView : MTKView <NSWindowDelegate, MTKViewDelegate> {
@public
    CVDisplayLinkRef displayLink;
    bool running;
}
@end

@implementation MyView
// Initialize
- (id) initWithFrame: (NSRect) frame {
    self = [super initWithFrame:frame];
    running = true;

    self.device = MTLCreateSystemDefaultDevice();
    self.delegate = self;
    self.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    self.depthStencilPixelFormat = MTLPixelFormatDepth16Unorm;
    self.clearDepth = 1.0f;

    renderOpen( (__bridge void*)self );
    [NSApp activateIgnoringOtherApps:YES];

    return self;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    //NSLog(@"drawableSizeWillChange");
    // Handle changes in drawable size, if needed
}

- (void)drawInMTKView:(MTKView *)view {
    //NSLog(@"drawInMTKView");

    MTLRenderPassDescriptor *passDescriptor = view.currentRenderPassDescriptor;
    if (passDescriptor != nil) {
        NSSize size = [ view frame ].size;

        struct RenderArgs args;
        args.renderPassDescriptor = (__bridge void*) (passDescriptor);
        args.drawable = (__bridge void*)(view.currentDrawable);
        args.width = size.width;
        args.height = size.height;
        args.view = (__bridge void*)(view);

        renderFrame( &args );
    }

}

// Update
- (CVReturn) getFrameForTime:(const CVTimeStamp*)outputTime {
    //NSLog(@"Update");
    if (false) { // Update loop returns false
        [NSApp terminate:self];
    }
    return kCVReturnSuccess;
}

// Terminate window when the red X is pressed
-(void)windowWillClose:(NSNotification *)notification {
    if (running) {
        renderClose();
        running = false;
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }

    [NSApp terminate:self];
}

- (NSPoint)TransformPoint:(NSEvent*) event {
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSSize size = [self frame ].size;
    point.y = size.height - 1 - point.y;
    return point;
}

- (void)mouseMoved:(NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    inputSend( "mouse.move", point.x, point.y );
}

// Resize
- (void)windowDidResize:(NSNotification*)notification {
    //NSSize size = [ [ self contentView ] frame ].size;
    //NSLog(@"Window resize: %lf, %lf", size.width, size.height);
}

@end

// static CVReturn GlobalDisplayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext) {
//     CVReturn result = [(__bridge MyView*)displayLinkContext getFrameForTime:outputTime];
//     return result;
// }

int main(int argc, const char * argv[])  { 

    @autoreleasepool{

    // Autorelease Pool: 
    // Objects declared in this scope will be automatically 
    // released at the end of it, when the pool is "drained". 
 
    // Create a shared app instance. 
    // This will initialize the global variable 
    // 'NSApp' with the application instance. 
    [NSApplication sharedApplication]; 
 
    // Create a window: 
 
    // Style flags 
    NSUInteger windowStyle = NSWindowStyleMaskTitled  | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable; 

    // Window bounds (x, y, width, height) 
    NSRect screenRect = [[NSScreen mainScreen] frame];
    NSRect viewRect = NSMakeRect(0, 0, 1024, 600); 
    NSRect windowRect = NSMakeRect(NSMidX(screenRect) - NSMidX(viewRect),
                                 NSMidY(screenRect) - NSMidY(viewRect),
                                 viewRect.size.width, 
                                 viewRect.size.height);

    NSWindow * window = [[NSWindow alloc] initWithContentRect:windowRect 
                        styleMask:windowStyle 
                        backing:NSBackingStoreBuffered 
                        defer:NO]; 
 
    // Window controller 
    //NSWindowController * windowController = [[NSWindowController alloc] initWithWindow:window]; 

    // Since Snow Leopard, programs without application bundles and Info.plist files don't get a menubar 
    // and can't be brought to the front unless the presentation option is changed
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    
    // Next, we need to create the menu bar. You don't need to give the first item in the menubar a name 
    // (it will get the application's name automatically)
    id menubar = [NSMenu new];
    id appMenuItem = [NSMenuItem new];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];

    // Then we add the quit item to the menu. Fortunately the action is simple since terminate: is 
    // already implemented in NSApplication and the NSApplication is always in the responder chain.
    id appMenu = [NSMenu new];
    id appName = [[NSProcessInfo processInfo] processName];
    id quitTitle = [@"Quit " stringByAppendingString:appName];
    id quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
        action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];

    // Create app delegate to handle system events
    MyView* view = [[MyView alloc] initWithFrame:windowRect];

    [window setAcceptsMouseMovedEvents:YES];
    [window setContentView:view];
    [window setDelegate:view];

    // Set app title
    [window setTitle:appName];

    // Add fullscreen button
    [window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];

    // Show window and run event loop 
    [window orderFrontRegardless]; 
    [NSApp run]; 
    
    }
 
    return (0); 
}

