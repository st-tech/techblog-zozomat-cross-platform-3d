//
//  ZTRGLView.m
//  ZOZO Technologies Cross Platform Renderer Example
//

#import "ZTRGLView.h"
#import "foot_renderer.h"

#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>

#import <CoreText/CoreText.h>

#include <sys/mman.h>

@interface ZTRGLView ()
{
    EAGLContext* _context;
    NSInteger _animationFrameInterval;
    CADisplayLink* _displayLink;

    GLuint _colorRenderbuffer;
    GLuint _depthRenderbuffer;
    GLuint _defaultFBOName;
}
@end

@implementation ZTRGLView

static ztr_hid_t g_hid;
static ztr_platform_api_t g_platform;

+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

// TODO: Move this to a common function for both OSX and iOS (Cocoa common).
PLATFORM_GET_RESOURCE_PATH(getResourcePath)
{
    char *result = 0;

    NSArray *components = [[NSString stringWithUTF8String:fileName] componentsSeparatedByString:@"."];
    if(components.count == 2)
    {
        NSBundle *bundle = [NSBundle bundleForClass:[ZTRGLView class]];
        NSString *fileNameBase = [NSString stringWithFormat:@"res/%@", components[0]];
        NSURL *filePathUrl = [bundle URLForResource:fileNameBase withExtension:components[1]];
        char *filePathCString = (char *) [filePathUrl.path cStringUsingEncoding: NSASCIIStringEncoding];

        assert (filePathCString != NULL);

        result = filePathCString;
    }

    return (result);
}

PLATFORM_OPEN_RESOURCE_FILE(openResourceFile)
{
    ztr_file_t result = {};

    // TODO: Use NS functions here.
    result.handle = open(filePath, O_RDONLY,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(result.handle >= 0)
    {
        int SeekOffset = lseek(result.handle, 0, SEEK_END);
        if(SeekOffset > 0)
        {
            lseek(result.handle, 0, SEEK_SET);
            result.dataSize = SeekOffset;
            // TODO: What is the FreeBSD version of mmap???
            result.data = mmap(
                    result.data,
                    result.dataSize,
                    PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANON,
                    -1, 0);

            ssize_t BytesRead = read(result.handle, result.data, result.dataSize);
            if(BytesRead == -1)
            {
                // TODO: Error handling/logging.
                // TODO: Check the errno here.
                if(result.data)
                {
                    // TODO: Proper Error Handling.
                    int munmapResult = munmap (result.data, result.dataSize);
                    assert (munmapResult == 0);
                    memset((void *)&result, 0, sizeof(result));
                }
            }
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
    eaglLayer.opaque = TRUE;

    eaglLayer.contentsScale = screenScale;
    eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                    [NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking, kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];

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

    g_platform.getResourcePath = getResourcePath;
    g_platform.openResourceFile = openResourceFile;

    ztrInit(&g_platform);
    ztrResize (0, backingWidth, backingHeight);

    _animating = FALSE;
    _animationFrameInterval = 1;
    _displayLink = nil;

    [self startAnimation];
}

- (void) doubleTapAction:(NSIndexPath *)indexPath
{
    g_hid.doubleTap = 1;

    // NOTE: Hack to ensure we disable normal input.
    g_hid.mouseTransition = 0;
    g_hid.mouseDown = 0;

    currentTouch = nil;
}

- (void) pinchZoomAction:(UIPinchGestureRecognizer *)recognizer
{
    // NSLog(@"pinchZoomAction: %f", recognizer.scale);

    switch (recognizer.state)
    {
        case UIGestureRecognizerStateBegan:
            {
                // NSLog(@"pinchZoomAction: UIGestureRecognizerStateBegan");

                g_hid.pinchZoomActive = 1;
                g_hid.pinchZoomTransition = 1;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        case UIGestureRecognizerStateChanged:
            {
                // NSLog(@"pinchZoomAction: UIGestureRecognizerStateChanged");

                g_hid.pinchZoomActive = 1;
                g_hid.pinchZoomTransition = 0;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        case UIGestureRecognizerStateEnded:
            {
                // NSLog(@"pinchZoomAction: UIGestureRecognizerStateEnded");

                g_hid.pinchZoomActive = 0;
                g_hid.pinchZoomTransition = 1;
                g_hid.pinchZoomScale = recognizer.scale;

            } break;

        default:
            break;
    }

    // NOTE: Hack to ensure we disable normal input.
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
    // TODO: Invoke a resize function here.

    [self drawView:nil];
}

- (NSInteger) animationFrameInterval
{
	return _animationFrameInterval;
}

- (void) setAnimationFrameInterval:(NSInteger)frameInterval
{
	// Frame interval defines how many display frames must pass between each time the
	// display link fires. The display link will only fire 30 times a second when the
	// frame internal is two on a display that refreshes 60 times a second. The default
	// frame interval setting of one will fire 60 times a second when the display refreshes
	// at 60 times a second. A frame interval setting of less than one results in undefined
	// behavior.
	if (frameInterval >= 1)
	{
		_animationFrameInterval = frameInterval;

		if (_animating)
		{
			[self stopAnimation];
			[self startAnimation];
		}
	}
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

		_animating = TRUE;
	}
}

- (void) stopAnimation
{
	if (_animating)
	{
        [_displayLink invalidate];
        _displayLink = nil;
		_animating = FALSE;
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

    // We only track the first touch.
    for (UITouch *touch in event.allTouches) {

        if (currentTouch == nil) {

            currentTouch = touch;

            CGPoint pos = [touch locationInView: self];

            g_hid.mouseX = pos.x;
            g_hid.mouseY = self.bounds.size.height - pos.y;

            break;
        }
    }
}

- (void) touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [super touchesMoved:touches withEvent:event];

    for(UITouch *touch in event.allTouches) {

        if (touch == currentTouch) {

            CGPoint pos = [touch locationInView: self];

            g_hid.mouseX = pos.x;
            g_hid.mouseY = self.bounds.size.height - pos.y;

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
