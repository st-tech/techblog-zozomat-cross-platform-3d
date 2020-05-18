//
//  ViewController.m
//  ZOZO Technologies Cross Platform Renderer Example
//

#include <sys/mman.h>

#import "ViewController.h"
#import "foot_renderer.h"

@implementation ViewController

static ztr_platform_api_t g_platform;
static ztr_hid_t g_hid;

static CVReturn MyDisplayLinkCallback(CVDisplayLinkRef displayLink,
									  const CVTimeStamp* now,
									  const CVTimeStamp* outputTime,
									  CVOptionFlags flagsIn,
									  CVOptionFlags* flagsOut,
									  void* displayLinkContext)
{
    ViewController *glView = (__bridge ViewController*)displayLinkContext;

    @autoreleasepool {
        [glView drawView];
    }
	return kCVReturnSuccess;
}

- (void) awakeFromNib
{
    NSOpenGLPixelFormatAttribute attrs[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFADepthSize, 24,

		// Must specify the 3.2 Core Profile to use OpenGL 3.2
//#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3
		NSOpenGLPFAOpenGLProfile,
		NSOpenGLProfileVersion3_2Core,
//#endif
		0
	};

	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

	if (!pixelFormat)
	{
		NSLog(@"No OpenGL pixel format");
	}

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3 && defined(DEBUG)
	CGLEnable([context CGLContextObj], kCGLCECrashOnRemovedFunctions);
#endif

    [self setPixelFormat:pixelFormat];
    [self setOpenGLContext:context];

#if SUPPORT_RETINA_RESOLUTION
    [self setWantsBestResolutionOpenGLSurface:YES];
#endif

    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);

    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                                options:options
                                                                  owner:self
                                                               userInfo:nil];
    [self addTrackingArea:trackingArea];

    [[self window] setAcceptsMouseMovedEvents:YES];
}

// TODO: Move this to a common function for both OSX and iOS (Cocoa common).
PLATFORM_GET_RESOURCE_PATH(getResourcePath)
{
    char *result = 0;

    NSArray *components = [[NSString stringWithUTF8String:fileName] componentsSeparatedByString:@"."];
    if(components.count == 2)
    {
        NSString *fileNameBase = [NSString stringWithFormat:@"res/%@", components[0]];
        NSURL *filePathUrl = [[NSBundle mainBundle] URLForResource:fileNameBase withExtension:components[1]];
        const char *filePathCString = [filePathUrl.path cStringUsingEncoding: NSASCIIStringEncoding];

        assert (filePathCString != NULL);

        result = (char *) filePathCString;
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
        off_t SeekOffset = lseek(result.handle, 0, SEEK_END);
        if(SeekOffset > 0)
        {
            lseek(result.handle, 0, SEEK_SET);
            result.dataSize = (unsigned int) SeekOffset;
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

- (void) prepareOpenGL
{
	[super prepareOpenGL];
	[[self openGLContext] makeCurrentContext];

	// Synchronize buffer swaps with vertical refresh rate
	GLint swapInt = 1;
	[[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
	CVDisplayLinkSetOutputCallback(displayLink, &MyDisplayLinkCallback, (__bridge void*)self);
	CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
	CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
	CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);

	CVDisplayLinkStart(displayLink);

	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(windowWillClose:)
												 name:NSWindowWillCloseNotification
											   object:[self window]];

    g_platform.openResourceFile = openResourceFile;
    g_platform.getResourcePath = getResourcePath;

    ztrInit(&g_platform);
}

- (void) windowWillClose:(NSNotification*)notification
{
    CVDisplayLinkStop(displayLink);
}


- (void)renewGState
{
	[[self window] disableScreenUpdatesUntilFlush];
	[super renewGState];
}

- (void) drawRect: (NSRect) theRect
{
	[self drawView];
}

- (void) drawView
{
	[[self openGLContext] makeCurrentContext];

	CGLLockContext([[self openGLContext] CGLContextObj]);

    ztrDraw(0, g_hid);

    g_hid.mouseTransition = 0;

	CGLFlushDrawable([[self openGLContext] CGLContextObj]);
	CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void) reshape
{
    [super reshape];

    NSRect viewDims = [self bounds];
    ztrResize (0, (int) viewDims.size.width, (int) viewDims.size.height);
}


- (void)mouseUp:(NSEvent *)event
{
    if (event.type == NSEventTypeLeftMouseUp)
    {
        g_hid.mouseTransition = 1;
        g_hid.mouseDown = 0;
    }
}

- (void)mouseDown:(NSEvent *)event
{
    if (event.type == NSEventTypeLeftMouseDown)
    {
        g_hid.mouseTransition = 1;
        g_hid.mouseDown = 1;
    }
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint pos = [self convertPoint:[event locationInWindow] fromView:nil];

    g_hid.mouseX = pos.x;
    g_hid.mouseY = pos.y;
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint pos = [self convertPoint:[event locationInWindow] fromView:nil];

    g_hid.mouseX = pos.x;
    g_hid.mouseY = pos.y;
}

- (void) dealloc
{
	CVDisplayLinkStop(displayLink);
	CVDisplayLinkRelease(displayLink);

    ztrFree ();
}

@end
