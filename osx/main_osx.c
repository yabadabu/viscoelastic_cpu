#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CVDisplayLink.h>

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
//#import "game_controller_manager.h"

#include "apple_platform.h"
//#include "input/keys.h"

// https://github.com/gamedevtech/CocoaOpenGLWindow

// https://github.com/metal-by-example/learn-metal-cpp-ios/blob/master/learn-metal/05-perspective/05-perspective.cpp

@class View;

@interface MyView : MTKView <NSWindowDelegate, MTKViewDelegate> {
@public
    CVDisplayLinkRef displayLink;
    bool running;
    int  current_flags;
    //GameControllerManager *game_controller;
    id<NSObject, NSCopying> touch0;
    id<NSObject, NSCopying> touch1;
    //- (void) runOSCommand:(const char*)cmd withArgs:(void*)args;
}

@end

void runOSCommand( const char* cmd, void* args ) {
    NSLog(@"Hi from runOSCommand %s", cmd);
}

@implementation MyView
// Initialize
- (id) initWithFrame: (NSRect) frame {
    self = [super initWithFrame:frame];
    running = true;
    current_flags = 0;

    self.device = MTLCreateSystemDefaultDevice();
    self.delegate = self;

    //self.wantsRestingTouches = TRUE;
    self.allowedTouchTypes = NSTouchTypeMaskDirect |NSTouchTypeMaskIndirect;;

    self.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    self.depthStencilPixelFormat = MTLPixelFormatDepth16Unorm;
    self.clearDepth = 1.0f;

    renderOpen();

    //game_controller = [[GameControllerManager alloc] init];

    touch0 = nil;
    touch1 = nil;

    return self;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    NSLog(@"drawableSizeWillChange");
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

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (NSPoint)TransformPoint:(NSEvent*) event {
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSSize size = [self frame ].size;
    point.y = size.height - 1 - point.y;
    return point;
}

- (void)mouseMoved:(NSEvent*) event {
    //NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.move", point.x, point.y );
}

- (void) mouseDragged: (NSEvent*) event {
    //NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.move", point.x, point.y );
}

- (void) rightMouseDragged: (NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.move", point.x, point.y );
    //inputSend( "mouse.drag", point.x, point.y );
}

- (void)scrollWheel: (NSEvent*) event  {
    float dx = [event scrollingDeltaX];
    float dy = [event scrollingDeltaY];
    //inputSend( "mouse.wheel", dx, dy  );
}

- (void) mouseDown: (NSEvent*) event {
    //NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.left.dw", point.x, point.y);
}

- (void) mouseUp: (NSEvent*) event {
    //NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.left.up", point.x, point.y);
}

- (void) rightMouseDown: (NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.right.dw", point.x, point.y);
}

- (void) rightMouseUp: (NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.right.up", point.x, point.y);
}

- (void)otherMouseDown: (NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.middle.dw", point.x, point.y );
}

- (void)otherMouseUp: (NSEvent*) event {
    NSPoint point = [self TransformPoint:event];
    //inputSend( "mouse.middle.up", point.x, point.y );
}

- (void)magnifyWithEvent:(NSEvent *)event {
    CGFloat factor = [event magnification];
    //inputSend( "magnify", factor, 0.0f );
}

- (void)rotateWithEvent:(NSEvent *)event {
    CGFloat factor = [event rotation];
    //inputSend( "rotation", factor, 0.0f );
}

- (void) mouseEntered: (NSEvent*)event {
    NSLog(@"Mouse entered");
}

- (void) mouseExited: (NSEvent*)event {
    NSLog(@"Mouse left");
}

- (void)flagsChanged:(NSEvent *)event {
    int new_flags = ([event modifierFlags])>>16;
    int changes = new_flags ^ current_flags;
    // if( changes & 2)
    //     inputSend( new_flags & 2 ? "key.dw" : "key.up", KeyShift, 0);
    // if( changes & 4)
    //     inputSend( new_flags & 4 ? "key.dw" : "key.up", KeyControl, 0);
    // if( changes & 8)
    //     inputSend( new_flags & 8 ? "key.dw" : "key.up", KeyOption, 0);
    // if( changes & 16)
    //     inputSend( new_flags & 16 ? "key.dw" : "key.up", KeyCommand, 0);
    current_flags = new_flags;
}

- (BOOL) acceptsFirstMouse: (NSEvent*)event {
    return true;
}

- (void)touchesBeganWithEvent:(NSEvent *)event {
  NSSet *touches = [event touchesMatchingPhase:NSTouchPhaseBegan inView:self];
  NSSize size = [self frame ].size;

  for (NSTouch *touch in touches) {
    NSPoint point = [self TransformPoint:event];
    NSPoint npoint = NSMakePoint( point.x / size.height, point.y / size.height );
    id<NSObject, NSCopying> identity = touch.identity;
    if( touch0 == nil ) {
        touch0 = identity;
        // inputSend( "touch.0.dw", npoint.x, npoint.y );
        // inputSend( "mouse.left.dw", point.x, point.y);
        //NSLog(@"Touch began: %@ as 0 T0:%p T1:%p", touch, touch0, touch1);
    }
    else if( touch1 == nil ) {
        touch1 = identity;
        // inputSend( "touch.1.dw", npoint.x, npoint.y );
        // inputSend( "mouse.left.dw", point.x, point.y);
        //NSLog(@"Touch began: %@ as 1 T0:%p T1:%p", touch, touch0, touch1);
    }
    else {
        //NSLog(@"Touch began: %@ but both touch0 and touch1 are already being tracked %p T0:%p T1:%p", touch, identity, touch0, touch1);
    }
  }
}

- (void)touchesMovedWithEvent:(NSEvent *)event {
  NSSet *touches = [event touchesMatchingPhase:NSTouchPhaseMoved inView:self];
  NSSize size = [self frame ].size;

  for (NSTouch *touch in touches) {
    NSPoint point = [self TransformPoint:event];
    NSPoint npoint = NSMakePoint( point.x / size.height, point.y / size.height );
    id<NSObject, NSCopying> identity = touch.identity;
    if( touch0 == identity ) {
        // inputSend( "touch.0.pos", npoint.x, npoint.y );
        // inputSend( "mouse.move", point.x, point.y);
    } else if( touch1 == identity ) {
        // inputSend( "touch.1.pos", npoint.x, npoint.y );
        // inputSend( "mouse.move", point.x, point.y);
    } else {
        //NSLog(@"Touch moved: %@ (%p) T0:%p T1:%p", touch, identity, touch0, touch1);
    }
  }
}

- (void)touchesEndedWithEvent:(NSEvent *)event {
  NSSet *touches = [event touchesMatchingPhase:NSTouchPhaseEnded inView:self];
  NSSize size = [self frame ].size;

  for (NSTouch *touch in touches) {
    NSPoint point = [self TransformPoint:event];
    NSPoint npoint = NSMakePoint( point.x / size.height, point.y / size.height );
    id<NSObject, NSCopying> identity = touch.identity;
    if( touch0 == identity ) {
        // inputSend( "touch.0.up", npoint.x, npoint.y );
        // inputSend( "mouse.left.up", point.x, point.y);
        touch0 = nil;
        //NSLog(@"Touch end: %@ as 0 T0:%p T1:%p", touch, touch0, touch1);
    }
    else if( touch1 == identity ) {
        // inputSend( "touch.1.up", npoint.x, npoint.y );
        // inputSend( "mouse.left.up", point.x, point.y);
        touch1 = nil;
        //NSLog(@"Touch end: %@ as 1 T0:%p T1:%p", touch, touch0, touch1);
    }
    else {
        //NSLog(@"Touch ended: %@ (%p) T0:%p T1:%p", touch, identity, touch0, touch1);
    }    
  }

  NSSet *active_touches = [event touchesMatchingPhase:NSTouchPhaseTouching inView:self];
  bool t0_found = false;
  bool t1_found = false;
  for (NSTouch *touch in active_touches) {
    id<NSObject, NSCopying> identity = touch.identity;
    if( touch0 == identity ) 
        t0_found = true;
    if( touch1 == identity ) 
        t1_found = true;
  }

    if( !t0_found && touch0 != nil) {
//        inputSend( "touch.0.up", 0, 0 );
        touch0 = nil;
        //NSLog(@"Touch forced end0 T0:%p T1:%p", touch0, touch1);
    }
    if( !t1_found && touch1 != nil) {
        //inputSend( "touch.1.up", 0, 0 );
        touch1 = nil;
        //NSLog(@"Touch forced end1 T0:%p T1:%p", touch0, touch1);
    }
}

- (void)touchesCancelledWithEvent:(NSEvent *)event {
  NSSet *touches = [event touchesMatchingPhase:NSTouchPhaseCancelled inView:self];
  NSSize size = [self frame ].size;

  for (NSTouch *touch in touches) {
    NSPoint point = [self TransformPoint:event];
    NSPoint npoint = NSMakePoint( point.x / size.height, point.y / size.height );
    id<NSObject, NSCopying> identity = touch.identity;
    if( touch0 == identity ) {
        //inputSend( "touch.0.up", npoint.x, npoint.y );
        touch0 = nil;
        //NSLog(@"Touch cancelled: %@ as 0 T0:%p T1:%p", touch, touch0, touch1);
    }
    else if( touch1 == identity ) {
        //inputSend( "touch.1.up", npoint.x, npoint.y );
        touch1 = nil;
        //NSLog(@"Touch cancelled: %@ as 1 T0:%p T1:%p", touch, touch0, touch1);
    }
    else {
        //NSLog(@"Touch cancelled: %@ (%p) T0:%p T1:%p", touch, identity, touch0, touch1);
    }     
  }
  //[super touchesCancelledWithEvent:event];
}

int translateKeyCode( int code ) {
    switch( code ) {
        // case kVK_Escape: return KeyEsc;
        // case kVK_LeftArrow:  return KeyLeftArrow;
        // case kVK_RightArrow: return KeyRightArrow;
        // case kVK_DownArrow:  return KeyDownArrow;
        // case kVK_UpArrow:    return KeyUpArrow;
        // case kVK_Tab:    return KeyTab;
        // case kVK_Return: return KeyReturn;
        case kVK_Space: return ' ';
        case kVK_ANSI_A: return 'A';
        case kVK_ANSI_B: return 'B';
        case kVK_ANSI_C: return 'C';
        case kVK_ANSI_D: return 'D';
        case kVK_ANSI_E: return 'E';
        case kVK_ANSI_F: return 'F';
        case kVK_ANSI_G: return 'G';
        case kVK_ANSI_H: return 'H';
        case kVK_ANSI_I: return 'I';
        case kVK_ANSI_J: return 'J';
        case kVK_ANSI_K: return 'K';
        case kVK_ANSI_L: return 'L';
        case kVK_ANSI_M: return 'M';
        case kVK_ANSI_N: return 'N';
        case kVK_ANSI_O: return 'O';
        case kVK_ANSI_P: return 'P';
        case kVK_ANSI_Q: return 'Q';
        case kVK_ANSI_R: return 'R';
        case kVK_ANSI_S: return 'S';
        case kVK_ANSI_T: return 'T';
        case kVK_ANSI_U: return 'U';
        case kVK_ANSI_V: return 'V';
        case kVK_ANSI_W: return 'W';
        case kVK_ANSI_X: return 'X';
        case kVK_ANSI_Y: return 'Y';
        case kVK_ANSI_Z: return 'Z';
    }
    NSLog( @"Unknown Key %d\n", code );
    return 0;
}

- (void) keyDown: (NSEvent*) event {
    if ([event isARepeat] == NO) {
        int code = translateKeyCode( event.keyCode );
        //inputSend( "key.dw", code, 0 );
    }
}

- (void) keyUp: (NSEvent*) event {
    int code = translateKeyCode( event.keyCode );
    //inputSend( "key.up", code, 0 );
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

