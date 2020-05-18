//
// See LICENSE.txt for this sample’s licensing information.
//
// RenderView.m
// ZOZO Technologies Cross Platform Renderer Example
//

#import "ztr_platform_abstraction_layer.h"

#import "RenderView.h"

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

#import <CoreText/CoreText.h>

#include <sys/mman.h>

@interface RenderView ()
{
    EAGLContext* _context;
    NSInteger _animationFrameInterval;
    CADisplayLink* _displayLink;

    GLuint _colorRenderbuffer;
    GLuint _depthRenderbuffer;
    GLuint _defaultFBOName;
}
@end

@implementation RenderView

static ztr_hid_t g_hid;
static ztr_platform_api_t g_platform;

+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

PLATFORM_OPEN_FILE (openFile)
{
    ztr_file_t result = {};

    NSArray *components =
        [[NSString stringWithUTF8String:fileName]
            componentsSeparatedByString:@"."];
    if (components.count == 2)
    {
        NSString *fileNameBase =
            [NSString stringWithFormat:@"res/%@", components[0]];
        NSURL *fileUrl =
            [[NSBundle mainBundle] URLForResource:fileNameBase
                                    withExtension:components[1]];

        NSFileManager *manager = [NSFileManager defaultManager];
        NSData *data = [manager contentsAtPath:fileUrl.path];

        if (data)
        {
            result.data = (void *) data.bytes;
            result.dataSize = (unsigned int) data.length;
        }
    }

    return (result);
}

- (void) setup
{

    self.backgroundColor = [UIColor whiteColor];

    CGFloat screenScale = [UIScreen mainScreen].scale;

    unsigned int major, minor;
    EAGLGetVersion(&major, &minor);
    printf ("Version major: %d minor: %d\n", major, minor);

    // Get the layer
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    eaglLayer.opaque = YES;

    eaglLayer.contentsScale = screenScale;
    eaglLayer.drawableProperties =
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:NO],
            kEAGLDrawablePropertyRetainedBacking,
            kEAGLColorFormatRGBA8,
            kEAGLDrawablePropertyColorFormat,
            nil];

	_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];

    if (!_context || ![EAGLContext setCurrentContext:_context])
	{
        return;
	}

    id<EAGLDrawable> drawable = (id<EAGLDrawable>) self.layer;

    // Create default framebuffer object. The backing will be allocated for the
    // current layer in -resizeFromLayer
    glGenFramebuffers(1, &_defaultFBOName);

    glGenRenderbuffers(1, &_colorRenderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);

    // This call associates the storage for the current render buffer with the
    // EAGLDrawable (our CAEAGLLayer) allowing us to draw into a buffer that
    // will later be rendered to the screen wherever the layer is (which
    // corresponds with our view).
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:drawable];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderbuffer);

    // Get the drawable buffer's width and height so we can create a depth buffer for the FBO
    GLint backingWidth;
    GLint backingHeight;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    // Create a depth buffer to use with our drawable FBO
    glGenRenderbuffers(1, &_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderbuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }

    UITapGestureRecognizer *doubleTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(doubleTapAction:)];
    doubleTap.numberOfTapsRequired = 2;
    [self addGestureRecognizer:doubleTap];

    UIPinchGestureRecognizer *pinchGestureRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(pinchZoomAction:)];
    [self addGestureRecognizer:pinchGestureRecognizer];

    g_platform.openFile = openFile;

    ztrInit(&g_platform);
    ztrResize (0, backingWidth, backingHeight);

    _animating = NO;
    _animationFrameInterval = 1;
    _displayLink = nil;

    [self startAnimation];
}

- (void) doubleTapAction:(NSIndexPath *)indexPath
{
    g_hid.doubleTap = 1;

    // Disable normal input
    g_hid.mouseTransition = 0;
    g_hid.mouseDown = 0;

    currentTouch = nil;
}

- (void) pinchZoomAction:(UIPinchGestureRecognizer *)recognizer
{
    switch (recognizer.state)
    {
        case UIGestureRecognizerStateBegan:
            {
                g_hid.pinchZoomActive = 1;
                g_hid.pinchZoomTransition = 1;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        case UIGestureRecognizerStateChanged:
            {
                g_hid.pinchZoomActive = 1;
                g_hid.pinchZoomTransition = 0;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        case UIGestureRecognizerStateEnded:
            {
                g_hid.pinchZoomActive = 0;
                g_hid.pinchZoomTransition = 1;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        default:
            break;
    }

    // Disable normal input
    g_hid.mouseTransition = 0;
    g_hid.mouseDown = 0;

    currentTouch = nil;
}

- (instancetype) initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame]))
    {
        [self setup];
    }

    return self;
}


- (instancetype) initWithCoder:(NSCoder*)coder
{
    if ((self = [super initWithCoder:coder]))
	{
        [self setup];
    }

    return self;
}

- (void) drawView:(id)sender
{
	[EAGLContext setCurrentContext:_context];

    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBOName);

    ztrDraw(0, g_hid);

    g_hid.mouseTransition = 0;
    g_hid.doubleTap = 0;
    g_hid.pinchZoomTransition = 0;

    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);

    [_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void) layoutSubviews
{
    [self drawView:nil];
}

- (void) startAnimation
{
	if (!_animating)
	{
        // Create the display link and set the callback to our drawView method
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(drawView:)];

        // Set it to our _animationFrameInterval
        [_displayLink setFrameInterval:_animationFrameInterval];

        // Have the display link run on the default runn loop (and the main thread)
        [_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];

		_animating = YES;
	}
}

- (void) stopAnimation
{
	if (_animating)
	{
        [_displayLink invalidate];
        _displayLink = nil;
		_animating = NO;
	}
}

- (void) free
{
    glDeleteFramebuffers(1, &_defaultFBOName);
    _defaultFBOName = 0;

	if ([EAGLContext currentContext] == _context)
    {
        printf ("EAGLContext setCurrentContext:nil\n");
        [EAGLContext setCurrentContext:nil];
    }

    ztrFree ();
}


UITouch *currentTouch = nil;

- (void) touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesBegan:touches withEvent:event];

    g_hid.mouseTransition = 1;
    g_hid.mouseDown = 1;

    CGFloat scale = UIScreen.mainScreen.scale;

    // We only track the first touch
    for (UITouch *touch in event.allTouches) {

        if (currentTouch == nil) {

            currentTouch = touch;

            CGPoint pos = [touch locationInView: self];

            g_hid.mouseX = pos.x*scale;
            g_hid.mouseY = (self.bounds.size.height - pos.y)*scale;

            break;
        }
    }
}

- (void) touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesMoved:touches withEvent:event];

    CGFloat scale = UIScreen.mainScreen.scale;

    for(UITouch *touch in event.allTouches) {

        if (touch == currentTouch) {

            CGPoint pos = [touch locationInView: self];

            g_hid.mouseX = pos.x*scale;
            g_hid.mouseY = (self.bounds.size.height - pos.y)*scale;

            break;
        }
    }

}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesEnded:touches withEvent:event];

    for(UITouch *touch in event.allTouches) {

        if (touch == currentTouch) {

            currentTouch = nil;

            g_hid.mouseTransition = 1;
            g_hid.mouseDown = 0;

            break;
        }
    }
}

@end
