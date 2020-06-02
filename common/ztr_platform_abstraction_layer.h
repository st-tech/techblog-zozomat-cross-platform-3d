//
// See LICENSE.txt for this sample’s licensing information.
//
// ztr_platform_abstraction_layer.h
// ZOZO Technologies Cross Platform Renderer Example
//

#ifndef FOOT_RENDERER_H
#define FOOT_RENDERER_H


// MARK: Platform specific OpenGl includes

#if __ANDROID__
    #include <GLES3/gl3.h>

#elif __EMSCRIPTEN__
    #include <GLES3/gl3.h>

#elif __APPLE__
    #include "TargetConditionals.h"

    #if TARGET_OS_IPHONE
        #import <OpenGLES/ES3/gl.h>
        #import <OpenGLES/ES3/glext.h>

    #elif TARGET_IPHONE_SIMULATOR
        #import <OpenGLES/ES3/gl.h>
        #import <OpenGLES/ES3/glext.h>

    #elif TARGET_OS_OSX
        #import <OpenGL/OpenGL.h>
        #import <OpenGL/gl3.h>

    #else
        // Unsupported platform
#endif

#else
    // Unsupported architecture

#endif

#ifdef __cplusplus
extern "C" {
#endif


// MARK: Platform abstraction structs

typedef struct ztr_mem_t
{
    int mem[4];

} ztr_mem_t;

typedef struct ztr_hid_t
{
    float mouseX;
    float mouseY;

    int mouseDown;
    int mouseTransition;
    int doubleTap;

    int pinchZoomActive;
    int pinchZoomTransition;
    float pinchZoomScale;

} ztr_hid_t;

typedef struct ztr_file_t
{
    int handle;
    unsigned int dataSize;
    void *data;

} ztr_file_t;


// MARK: Platform call functions

#define PLATFORM_OPEN_FILE(name) ztr_file_t name(const char *fileName)
typedef PLATFORM_OPEN_FILE(platform_open_file);

// MARK: Platform call API

typedef struct ztr_platform_api_t
{
    platform_open_file *openFile;

} ztr_platform_api_t;


// MARK: Platform callback functions

#define ZTR_INIT(name) void name(ztr_platform_api_t *platform)
ZTR_INIT(ztrInit);

#define ZTR_DRAW(name) void name(ztr_mem_t *mem, ztr_hid_t hid)
ZTR_DRAW(ztrDraw);

#define ZTR_LOAD(name) void name(ztr_platform_api_t *platform, char *leftPath, char *rightPath)
ZTR_LOAD(ztrLoad);

#define ZTR_RESIZE(name) void name(ztr_platform_api_t *platform, int w, int h)
ZTR_RESIZE(ztrResize);

#define ZTR_FREE(name) void name(void)
ZTR_FREE(ztrFree);


#ifdef __cplusplus
}
#endif

#endif
