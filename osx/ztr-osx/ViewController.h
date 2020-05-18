//
//  ViewController.h
//  ZOZO Technologies Cross Platform Renderer Example
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

@interface ViewController : NSOpenGLView {
	CVDisplayLinkRef displayLink;
}

@end
