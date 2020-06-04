//
// See LICENSE.txt for this sample’s licensing information.
//
// RenderViewController.h
// ZOZO Technologies Cross Platform Renderer Example
//

#import <Cocoa/Cocoa.h>
//#import <QuartzCore/QuartzCore.h>
//#import <AppKit/AppKit.h>
//#import <Foundation/Foundation.h>

@interface RenderViewController : NSOpenGLView {
	CVDisplayLinkRef displayLink;
}

@end
