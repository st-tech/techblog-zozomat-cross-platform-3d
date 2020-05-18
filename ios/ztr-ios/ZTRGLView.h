//
//  ZTRGLView.h
//  ZOZO Technologies Cross Platform Renderer Example
//

#import <UIKit/UIKit.h>

@interface ZTRGLView : UIView

@property (readonly, nonatomic, getter=isAnimating) BOOL animating;
@property (nonatomic) NSInteger animationFrameInterval;

- (void) startAnimation;
- (void) stopAnimation;
- (void) drawView:(id)sender;

- (void) free;

@end
