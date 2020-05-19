//
//  ztr_platform_independent_layer.cpp
//  ZOZO Technologies Cross Platform Renderer Example
//

// MARK: Includes

#import "ztr_platform_abstraction_layer.h"

// MARK: Standard library includes

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <float.h>

// MARK: Single header library includes

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

// MARK: Constants

#define MAX_SHADERS (1 << 4)
#define MAX_MESHES (1 << 6)

#define CAM_PITCH_MIN 15.f
#define CAM_PITCH_MAX 88.f
#define CAM_LOOKAT_CENTER (HMM_Vec3 (0.f, 0.45f, 0.f))
#define CAM_LOOKAT_UP (HMM_Vec3 (0.f, 1.f, 0.f))

#define CAM_FOV 45.f
#define CAM_NEAR 0.5f
#define CAM_FAR 5.f

#define CAM_ORTH_SCALE_DEFAULT 0.7f
#define CAM_ORTH_SCALE_MIN 0.4f
#define CAM_ORTH_SCALE_MAX 0.75f

#define CAM_RADIUS_DEFAULT 2.2f
#define CAM_PITCH_DEFAULT 48.f
#define CAM_YAW_DEFAULT 0.f
#define CAM_YAW_BASE 44.f
#define CAM_YAW_ANIM_AMOUNT_DEFAULT 300.f

#define CAM_DECELLERATION_FACTOR 0.97f
#define CAM_STOP_THRESHOLD 0.05f

#define SCENE_ANIMATION_STEP 0.007f

// TODO: Make this platform independent.
#define MOUSE_SENSITIVITY 0.3f

// MARK: Structs

struct shader_t
{
    GLuint program;
    GLenum elementType;
};

struct vertex_t
{
    hmm_vec3 position;
    hmm_vec3 normal;
};

struct tvertex_t
{
    hmm_vec3 position;
    hmm_vec3 normal;
    hmm_vec2 uv;
};

struct rec3_t
{
    hmm_vec3 min;
    hmm_vec3 max;
};

struct texture_t
{
    GLuint id;
};

struct mesh_t
{
    vertex_t *vertices;
    unsigned int verticesCount;

    GLushort *indices;
    unsigned int indicesCount;

    vertex_t *textures;
    unsigned int texturesCount;

    // NOTE: Rendering buffers.
    GLuint VAO, VBO, EBO;

    hmm_mat4 S, R, T;
    hmm_mat4 model;
    shader_t *shader;
};

struct camera_t
{
    hmm_vec3 pos;
    float radius;
    float orthScale;
    float orthScaleDiff;

    float pitch;
    float yaw;

    float yawAnimEnd;
    float yawAnimAmount;
    float pitchAnimEnd;
    float pitchAnimAmount;

};

// This represents _internal_ input.
struct mouse_t
{
    hmm_vec2 last;
    hmm_vec2 offset;
};

struct scene_t
{
    camera_t camera;

    // TODO: State enum
    int ready;
    int animatingIntroFade;
    int animatingResetCamera;

    // Animation position
    float animT = 0.f;
    float animStep = 0.007f;

    // Shaders
    shader_t shaders[MAX_SHADERS];
    unsigned int shaderCount = 0;

    // Meshes
    mesh_t meshes[MAX_MESHES];
    int meshCount = 0;

    shader_t *objectShader;


    // TODO: Separate "from platform" struct?
    mouse_t mouse;
    hmm_vec2 screenDims;

    // Frame timing
    double currentTime = 0;
    double lastTime = 0;
};


// MARK: Enums

enum shading_version_t
{
    ShadingLanguageVersion_None,
    ShadingLanguageVersion_GLES300,
    ShadingLanguageVersion_GLES310,
    ShadingLanguageVersion_GLES320,
    ShadingLanguageVersion_GL410,
};


// MARK: Globals

static scene_t g_scene;

ztr_platform_api_t *g_platform;


// MARK: Utility Functions

inline void
InitCam (camera_t *cam)
{
    cam->pos = HMM_Vec3 (0.f, 0.f, 0.f);
    cam->radius = CAM_RADIUS_DEFAULT;
    cam->orthScale = CAM_ORTH_SCALE_DEFAULT;;

    cam->pitch = CAM_PITCH_DEFAULT;
    cam->yaw = CAM_YAW_DEFAULT;

    cam->yawAnimEnd = CAM_YAW_BASE;
    cam->yawAnimAmount = CAM_YAW_ANIM_AMOUNT_DEFAULT;

    cam->pitchAnimEnd = CAM_PITCH_DEFAULT;
    cam->pitchAnimAmount = CAM_PITCH_DEFAULT;
}

inline void
InitScene (scene_t *scene)
{
    scene->animT = 0.f;
    scene->animStep = SCENE_ANIMATION_STEP;
    scene->animatingIntroFade = 0;
}

inline void
InitMouse (mouse_t *mouse)
{
    mouse->last.X = 400;
    mouse->last.Y = 300;
    mouse->offset.X = 0.f;
    mouse->offset.Y = 0.f;
}

#ifdef _DEBUG
    #define GL_CHECK_ERROR() glCheckError()
#else
    #define GL_CHECK_ERROR()
#endif

inline void
glCheckError (void)
{
    GLenum error = glGetError ();
    if (error != GL_NO_ERROR)
    {
        printf ("GL error detected: 0x%04x\n", error);
    }
}

inline float
Clamp (float x, float min, float max)
{
    float res = x;
    if (res > max)
    {
        res = max;
    }
    else if (res < min)
    {
        res = min;
    }
    return (res);
}


// TODO: Could use fixed co-efficients for speed here.
// NOTE: t should be 0 to 1, returns curve value from B to C.
inline float
cubicHermite (float A, float B, float C, float D, float t)
{
    float a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    float b = A - (5.0*B)/2.0 + 2.0*C - D/2.0;
    float c = -A/2.0 + C/2.0;
    float d = B;

    float x = a*t*t*t + b*t*t + c*t + d;

    return (x);
}

// NOTE: Bezier allows for steeper curves than Hermite...
inline float
cubicBezier (float u)
{
    hmm_vec3 p0 = HMM_Vec3 (0.f, 0.f, 0.f);
    hmm_vec3 p1 = HMM_Vec3 (0.f, 1.f, 0.f);
    hmm_vec3 p2 = HMM_Vec3 (0.f, 1.f, 0.f);
    hmm_vec3 p3 = HMM_Vec3 (1.f, 1.f, 0.f);

    // float xu = pow(1 - u, 3)*p0.X + 3*u*pow(1 - u, 2)*p1.X + 3*pow(u, 2)*(1 - u)*p2.X + pow(u, 3)*p3.X;
    float yu = pow(1 - u, 3)*p0.Y + 3*u*pow(1 - u, 2)*p1.Y + 3*pow(u, 2)*(1 - u)*p2.Y + pow(u, 3)*p3.Y;

    return (yu);
}

inline void
UpdateCamPos (camera_t *cam)
{
    // TODO: Draw this vector for debugging.
    // TODO: Sort out cam->pitch flipping issue...
    hmm_vec3 dir;

    dir.X = HMM_CosF (HMM_ToRadians (cam->yaw)) * HMM_CosF(HMM_ToRadians (cam->pitch));
    dir.Y = HMM_SinF (HMM_ToRadians (cam->pitch));
    dir.Z = HMM_SinF (HMM_ToRadians (cam->yaw)) * HMM_CosF(HMM_ToRadians (cam->pitch));

    cam->pos = HMM_NormalizeVec3 (dir)*cam->radius;
}

GLuint
LoadShaders (shading_version_t shadingVersion, char *vertexFileName, char *fragmentFileName)
{
    // NOTE: Create vertex and fragment shaders.
    GLuint vertexShaderID = glCreateShader (GL_VERTEX_SHADER);
    GLuint fragmentShaderID = glCreateShader (GL_FRAGMENT_SHADER);

    ztr_file_t vertexShaderFile = g_platform->openResourceFile (vertexFileName);
    if (vertexShaderFile.data == NULL)
        assert (!"Could not load vertex shader.\n");

    ztr_file_t fragmentShaderFile = g_platform->openResourceFile (fragmentFileName);
    if (fragmentShaderFile.data == NULL)
        assert (!"Could not load fragment shader.\n");

    char *versionString = 0;

    switch (shadingVersion)
    {
        case ShadingLanguageVersion_None:
            {
                assert (!"Invalid shading language version.");
            } break;

        case ShadingLanguageVersion_GLES300:
            {
                versionString = (char *) "#version 300 es\n";
            } break;

        case ShadingLanguageVersion_GLES310:
            {
                versionString = (char *) "#version 310 es\n";
            } break;

        case ShadingLanguageVersion_GLES320:
            {
                versionString = (char *) "#version 320 es\n";
            } break;

        case ShadingLanguageVersion_GL410:
            {
                versionString = (char *) "#version 410\n";
            } break;
    }

    int versionStringSize = (int) strlen (versionString);

    // TODO: Error check mallocs.
    int vertexShaderSourceSize = vertexShaderFile.dataSize + versionStringSize;
    char *vertexShaderSource = (char *) malloc (vertexShaderSourceSize + 1);
    memcpy ((void *) vertexShaderSource, (void *) versionString, versionStringSize);
    memcpy ((void *) (vertexShaderSource + versionStringSize), vertexShaderFile.data, vertexShaderFile.dataSize);
    vertexShaderSource[vertexShaderSourceSize] = '\0';

    int fragmentShaderSourceSize = fragmentShaderFile.dataSize + versionStringSize;
    char *fragmentShaderSource = (char *) malloc (fragmentShaderSourceSize + 1);
    memcpy ((void *) fragmentShaderSource, (void *) versionString, versionStringSize);
    memcpy ((void *) (fragmentShaderSource + versionStringSize), fragmentShaderFile.data, fragmentShaderFile.dataSize);
    fragmentShaderSource[fragmentShaderSourceSize] = '\0';

    GLint Result = GL_FALSE;
    int infoLogLength;

    // Compile Vertex Shader.
    printf ("Compiling shader : %s\n", vertexFileName);
    glShaderSource (vertexShaderID, 1, (const GLchar **) &vertexShaderSource, NULL);
    glCompileShader (vertexShaderID);

    // Check Vertex Shader.
    glGetShaderiv (vertexShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv (vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0 ){
        std::vector<char> vertexShaderErrorMessage (infoLogLength + 1);
        glGetShaderInfoLog (vertexShaderID, infoLogLength, NULL, &vertexShaderErrorMessage[0]);
        printf ("%s\n", &vertexShaderErrorMessage[0]);
    }

    // Compile Fragment Shader.
    printf ("Compiling shader : %s\n", fragmentFileName);
    glShaderSource (fragmentShaderID, 1, (const GLchar **) &fragmentShaderSource, NULL);
    glCompileShader (fragmentShaderID);

    // Check Fragment Shader.
    glGetShaderiv (fragmentShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv (fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
        std::vector<char> fragmentShaderErrorMessage (infoLogLength + 1);
        glGetShaderInfoLog (fragmentShaderID, infoLogLength, NULL, &fragmentShaderErrorMessage[0]);
        printf ("%s\n", &fragmentShaderErrorMessage[0]);
    }

    // Link the program.
    printf ("Linking program\n");
    GLuint programID = glCreateProgram ();
    glAttachShader (programID, vertexShaderID);
    glAttachShader (programID, fragmentShaderID);
    glLinkProgram (programID);

    // Check the program
    glGetProgramiv (programID, GL_LINK_STATUS, &Result);
    glGetProgramiv (programID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
        std::vector<char> programErrorMessage (infoLogLength + 1);
        glGetProgramInfoLog (programID, infoLogLength, NULL, &programErrorMessage[0]);
        printf ("%s\n", &programErrorMessage[0]);
    }

    glDetachShader (programID, vertexShaderID);
    glDetachShader (programID, fragmentShaderID);
    glDeleteShader (vertexShaderID);
    glDeleteShader (fragmentShaderID);

    free ((void *) vertexShaderSource);
    free ((void *) fragmentShaderSource);

    return programID;
}

static mesh_t *
loadObj (const char *fileName)
{
    ztr_file_t file = g_platform->openResourceFile (fileName);

    mesh_t *result = NULL;

    if (file.data != NULL)
    {
        tinyobj_attrib_t attrib;
        tinyobj_shape_t* shapes = NULL;
        size_t numShapes;
        tinyobj_material_t* materials = NULL;
        size_t numMaterials;

        unsigned int flags = 0;

        int parseResult = tinyobj_parse_obj (&attrib, &shapes, &numShapes, &materials,
                &numMaterials, (const char *) file.data, file.dataSize, flags);

        if ((parseResult == TINYOBJ_ERROR_EMPTY) ||
            (parseResult == TINYOBJ_ERROR_INVALID_PARAMETER) ||
            (parseResult == TINYOBJ_ERROR_FILE_OPERATION))
        {
            printf ("A Tiny obj error occured.\n");
        }

        assert (g_scene.meshCount < MAX_MESHES);
        mesh_t *mesh = g_scene.meshes + g_scene.meshCount++;

        mesh->indicesCount = 0;
        mesh->indices = (GLushort *) malloc (sizeof (GLushort)*attrib.num_faces);

        mesh->verticesCount = 0;
        mesh->vertices = (vertex_t *) malloc (sizeof (vertex_t)*attrib.num_faces);

        // For now, we iterate every face and copy to a new vertex.
        // To remove dupes, maybe we can find a lib that does this for us.
        // Could prob do this with a hash map quite easily.
        for (int i=0 ; i<attrib.num_faces ;  i++)
        {
            tinyobj_vertex_index_t *sourceFace = attrib.faces + i;

            unsigned int destIndex = mesh->verticesCount;
            vertex_t *destVertex = mesh->vertices + destIndex;

            float *vertStart = attrib.vertices + sourceFace->v_idx*3;
            destVertex->position = HMM_Vec3 (vertStart[0], vertStart[1], vertStart[2]);

            if (sourceFace->vn_idx != TINYOBJ_INVALID_INDEX)
            {
                float *normStart = attrib.normals + sourceFace->vn_idx*3;
                destVertex->normal = HMM_Vec3 (normStart[0], normStart[1], normStart[2]);
            }

#if 0
            if (sourceFace->vt_idx != TINYOBJ_INVALID_INDEX)
            {
                float *uvStart = attrib.texcoords + sourceFace->vt_idx*2;
                destVertex->uv = HMM_Vec2 (uvStart[0], uvStart[1]);
            }
#endif

            mesh->indices[i] = (GLushort)i;
            mesh->verticesCount++;
            mesh->indicesCount++;
        }

        glGenVertexArrays (1, &mesh->VAO);
        glGenBuffers (1, &mesh->VBO);
        glGenBuffers (1, &mesh->EBO);

        glBindVertexArray (mesh->VAO);

        glBindBuffer (GL_ARRAY_BUFFER, mesh->VBO);
        glBufferData (GL_ARRAY_BUFFER, mesh->verticesCount*sizeof (vertex_t), mesh->vertices, GL_STATIC_DRAW);
        //glBindBuffer (GL_ARRAY_BUFFER, 0);

        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
        glBufferData (GL_ELEMENT_ARRAY_BUFFER, mesh->indicesCount*sizeof (GLushort), mesh->indices, GL_STATIC_DRAW);
        //glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

        // Vertex Positions.
        glEnableVertexAttribArray (0);
        glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (vertex_t), (GLvoid*)offsetof (vertex_t, position));

        // Vertex Normals.
        glEnableVertexAttribArray (1);
        glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, sizeof (vertex_t), (GLvoid*)offsetof (vertex_t, normal));

        // Vertex Texture Coords.
        // glEnableVertexAttribArray (2);
        // glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof (vertex_t), (GLvoid*)offsetof (vertex_t, uv));

        // Unbind the VAO, not the EBO, otherwise our VAO will lose it's EBO.
        glBindVertexArray (0);

        result = mesh;

        // TODO: IMPLEMENT FOR DEMO
        // g_platform->closeAndFreeResourceFile(file);
    }

    return (result);
}

inline shading_version_t
findShadingVersion (char *glShadingVersionString)
{
    shading_version_t result = ShadingLanguageVersion_None;

    if (strstr (glShadingVersionString, "3.00") != NULL)
    {
        result = ShadingLanguageVersion_GLES300;
    }
    else if (strstr (glShadingVersionString, "3.10") != NULL)
    {
        result = ShadingLanguageVersion_GLES310;
    }
    else if (strstr (glShadingVersionString, "3.20") != NULL)
    {
        result = ShadingLanguageVersion_GLES320;
    }
    else if (strstr (glShadingVersionString, "4.10") != NULL)
    {
        result = ShadingLanguageVersion_GL410;
    }
    else
    {
        assert (!"Unsupported shading language.");
    }

    return (result);
}

// MARK: Platform abstracted functions

ZTR_INIT (ztrInit)
{
    glGetString (GL_VERSION);

    g_platform = platform;

    g_scene.shaderCount = 0;

    GLint major, minor;
    glGetIntegerv (GL_MAJOR_VERSION, &major);
    glGetIntegerv (GL_MINOR_VERSION, &minor);
    printf ("GL version major: %d minor: %d\n", major, minor);

    char *shadingVersionString = (char *)glGetString (GL_SHADING_LANGUAGE_VERSION);
    printf ("GL shading language version: %s\n", shadingVersionString);
    shading_version_t shadingVersion = findShadingVersion (shadingVersionString);

    assert (shadingVersion != ShadingLanguageVersion_None);

    GLint m_viewport[4];
    glGetIntegerv (GL_VIEWPORT, m_viewport);

    // Uncomment for wireframe debugging.
    // glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
    glEnable (GL_CULL_FACE);

    // Enable transparency.
    glEnable (GL_BLEND);

    // Expects pre multiplied alpha values.
    glBlendEquationSeparate (GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate (GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // TODO: Does nothing by itself. Need to use FBOs for MSAA.
    // glEnable (GL_MULTISAMPLE);

    assert (g_scene.shaderCount < MAX_SHADERS);
    g_scene.objectShader = g_scene.shaders + g_scene.shaderCount++;
    g_scene.objectShader->program = LoadShaders (shadingVersion, (char *) "shaders/object_vert.glsl", (char *) "shaders/object_frag.glsl");
    g_scene.objectShader->elementType = GL_TRIANGLES;

    glUseProgram (g_scene.objectShader->program);

    // Set the shader colors only once.
    // Don’t forget to ’use’ the corresponding shader program first (to set the uniform)
    GLint objectColorLoc = glGetUniformLocation (g_scene.objectShader->program, "objectColor");
    glUniform3f (objectColorLoc, 255.f/255.99f, 174.f/255.99f, 82.f/255.99f);
    GL_CHECK_ERROR ();

    // Also set light’s color (white).
    GLint lightColorLoc = glGetUniformLocation (g_scene.objectShader->program, "lightColor");
    glUniform3f (lightColorLoc, 1.0f, 1.0f, 1.0f);
    GL_CHECK_ERROR ();

    hmm_vec3 lightPos = HMM_Vec3 (0.2,0,0.3);
    GLint lightPosLoc = glGetUniformLocation (g_scene.objectShader->program, "lightPos");
    glUniform3f (lightPosLoc, lightPos.X, lightPos.Y, lightPos.Z);
    GL_CHECK_ERROR ();

    // MARK: Init all structs
    InitScene (&g_scene);
    InitCam (&g_scene.camera);
    InitMouse (&g_scene.mouse);

    // MARK: Load Stanford Bunny

    mesh_t *bunnyMesh = loadObj ("bunny_vn.obj");
    float S = 1.f;
    bunnyMesh->S = HMM_Scale (HMM_Vec3 (S, S, S));
    bunnyMesh->R = HMM_Rotate (0.f, HMM_Vec3 (1,0,0));
    bunnyMesh->T = HMM_Translate (HMM_Vec3 (0,0,0));
    bunnyMesh->shader = g_scene.objectShader;

    g_scene.ready = 1;
    g_scene.animatingIntroFade = 1;

}

ZTR_FREE (ztrFree)
{
    if (g_scene.ready)
    {
        // TODO: Allocate a memory pool and reset to zero.
        for (int i=0 ; i<g_scene.meshCount ; i++)
        {
            mesh_t *mesh = g_scene.meshes + i;

            if (mesh->indices)
            {
                free (mesh->indices);
                mesh->indices = NULL;
            }
            if (mesh->vertices)
            {
                free (mesh->vertices);
                mesh->vertices = NULL;
            }
        }

        g_scene.meshCount = 0;
        // g_scene.shaderCount = 0;
        g_scene.ready = 0;
    }
}

ZTR_RESIZE (ztrResize)
{
    // TODO: Sync these writes.
    g_scene.screenDims.X = w;
    g_scene.screenDims.Y = h;

    glViewport (0, 0, w, h);
}

ZTR_DRAW (ztrDraw)
{
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);

    glClearColor (1.f, 1.f, 1.f, 1.f);

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // TODO: Make this atomic.
    if (g_scene.ready)
    {
        camera_t *cam = &g_scene.camera;

        mouse_t *mouse = &g_scene.mouse;

        float animCurve = 1.f;
        float fadeInAnimCurve = 1.f;
        float labelPosAnimCurve = 1.f;
        float fadeMeshOpacity = 0.f;

        // NOTE: Check first to disable hid input while load animating.
        if (g_scene.animatingIntroFade)
        {
            if (g_scene.animT < 1.f)
            {
                animCurve = cubicBezier (g_scene.animT);
                fadeInAnimCurve = animCurve;
                labelPosAnimCurve = animCurve;

                cam->yaw = cam->yawAnimEnd + cam->yawAnimAmount*(1.f - animCurve);
                cam->pitch = cam->pitchAnimEnd + cam->pitchAnimAmount*(1.f - animCurve);

                fadeMeshOpacity = 1.f - animCurve;

                g_scene.animT += g_scene.animStep;
            }
            else
            {
                // NOTE: To prevent mouse "jumping" if down while animating.
                if (hid.mouseDown == 1)
                {
                    mouse->last.X = hid.mouseX;
                    mouse->last.Y = hid.mouseY;
                }

                g_scene.animatingIntroFade = 0;
                g_scene.animT = 0.f;
            }

            UpdateCamPos (cam);
        }
        else if (hid.doubleTap == 1)
        {
            mouse->last.X = hid.mouseX;
            mouse->last.Y = hid.mouseY;

            mouse->offset.X = 0;
            mouse->offset.Y = 0; // reversed since y-coordinates range from bottom to top

            g_scene.animatingIntroFade = false;

            g_scene.animatingResetCamera = 1;
            g_scene.animT = 0.f;

            // NOTE: To ensure we always rotate in the correct direction.
            float yawZeroed = cam->yaw - (CAM_YAW_BASE + 180);
            float yawWhole = (((int) (yawZeroed/360.f))*360.f);
            float yawFraction = (yawZeroed - yawWhole);
            yawFraction += (yawFraction < 0.f ? 360.f : 0.f);
            float yawAngle = yawFraction - 180.f;

            cam->yawAnimEnd = yawWhole + CAM_YAW_BASE;
            cam->yawAnimAmount = yawAngle;

            cam->pitchAnimEnd = CAM_PITCH_DEFAULT;
            cam->pitchAnimAmount = cam->pitch - cam->pitchAnimEnd;

            cam->orthScaleDiff = CAM_ORTH_SCALE_MAX - cam->orthScale;
        }
        else if (hid.pinchZoomTransition == 1)
        {
            if (hid.pinchZoomActive)
            {
                g_scene.animatingIntroFade = false;
                g_scene.animatingResetCamera = false;

                mouse->offset.X = 0;
                mouse->offset.Y = 0;
            }
            else
            {
                cam->orthScale = cam->orthScale*(1.f/hid.pinchZoomScale);
                cam->orthScale = Clamp (cam->orthScale, CAM_ORTH_SCALE_MIN, CAM_ORTH_SCALE_MAX);

            }

        }
        else if (hid.pinchZoomActive == 1)
        {
            cam->orthScale = cam->orthScale*(1.f/hid.pinchZoomScale);
            cam->orthScale = Clamp (cam->orthScale, CAM_ORTH_SCALE_MIN, CAM_ORTH_SCALE_MAX);

        }
        else if (hid.mouseDown == 1)
        {
            if (hid.mouseTransition == 1)
            {
                mouse->last.X = hid.mouseX;
                mouse->last.Y = hid.mouseY;

                g_scene.animatingIntroFade = false;
                g_scene.animatingResetCamera = false;
            }

            // Reversed since y-coordinates range from bottom to top.
            mouse->offset.X = hid.mouseX - mouse->last.X;
            mouse->offset.Y = mouse->last.Y - hid.mouseY;

            mouse->last.X = hid.mouseX;
            mouse->last.Y = hid.mouseY;

            mouse->offset.X *= MOUSE_SENSITIVITY;
            mouse->offset.Y *= MOUSE_SENSITIVITY;

            cam->yaw += mouse->offset.X;
            cam->pitch += mouse->offset.Y;

            if (cam->pitch < CAM_PITCH_MIN)
            {
                cam->pitch = CAM_PITCH_MIN;
                mouse->offset.Y = 0.f;
            }
            else if (cam->pitch > CAM_PITCH_MAX)
            {
                cam->pitch = CAM_PITCH_MAX;
                mouse->offset.Y = 0.f;
            }

            UpdateCamPos (cam);
        }

        else if (g_scene.animatingResetCamera)
        {
            if (g_scene.animT < 1.f)
            {
                animCurve = cubicBezier (g_scene.animT);
                fadeInAnimCurve = 1.f;
                labelPosAnimCurve = 1.f;

                cam->yaw = cam->yawAnimEnd + cam->yawAnimAmount*(1.f - animCurve);
                cam->pitch = cam->pitchAnimEnd + cam->pitchAnimAmount*(1.f - animCurve);

                cam->orthScale = CAM_ORTH_SCALE_MAX - cam->orthScaleDiff*(1.f - animCurve);

                g_scene.animT += g_scene.animStep;
            }
            else
            {
                g_scene.animatingResetCamera = 0;
                g_scene.animT = 0.f;
            }

            UpdateCamPos (cam);
        }
        else
        {
            cam->yaw += mouse->offset.X;
            cam->pitch += mouse->offset.Y;

            if (cam->pitch < CAM_PITCH_MIN)
            {
                cam->pitch = CAM_PITCH_MIN;
                mouse->offset.Y = 0.f;
            }
            else if (cam->pitch > CAM_PITCH_MAX)
            {
                cam->pitch = CAM_PITCH_MAX;
                mouse->offset.Y = 0.f;
            }

            UpdateCamPos (cam);

            mouse->offset.X*=CAM_DECELLERATION_FACTOR;
            mouse->offset.Y*=CAM_DECELLERATION_FACTOR;

            if (abs (mouse->offset.X) < CAM_STOP_THRESHOLD)
            {
                mouse->offset.X = 0.f;
            }
            if (abs (mouse->offset.Y) < CAM_STOP_THRESHOLD)
            {
                mouse->offset.Y = 0.f;
            }
        }

        g_scene.currentTime = g_scene.lastTime + 1.f/60.f;
        // float deltaTime = (float) (g_scene.currentTime - g_scene.lastTime);
        g_scene.lastTime = g_scene.currentTime;

        // glfwGetFramebufferSize(window, &g_scene.screenDims.X, &g_scene.screenDims.Y);

        // Projection/view matrices.
        float ratio = (float) g_scene.screenDims.Y/(float) g_scene.screenDims.X;
        float orth = cam->orthScale;
        hmm_mat4 projection = HMM_Orthographic (-orth, orth, -ratio*orth, ratio*orth, CAM_NEAR, CAM_FAR);
        hmm_mat4 view = HMM_LookAt (cam->pos, CAM_LOOKAT_CENTER, CAM_LOOKAT_UP);

        glUseProgram (g_scene.objectShader->program);
        GL_CHECK_ERROR ();

        hmm_vec3 lightPos = HMM_Vec3 (1,1,2);
        GLint lightPosLoc = glGetUniformLocation (g_scene.objectShader->program, "lightPos");
        //glUniform3f (lightPosLoc, cam->pos[0], cam->pos[1], cam->pos[2]);
        glUniform3f (lightPosLoc, lightPos[0], lightPos[1], lightPos[2]);
        GL_CHECK_ERROR ();

        for (int i=0 ; i<g_scene.shaderCount; i++)
        {
            shader_t *shader = g_scene.shaders + i;

            // TODO: Should only have to do this once per shader.
            // Or better still, use the same shader.
            glUseProgram(shader->program);

            GLuint viewMatrixLoc = glGetUniformLocation (shader->program, "view");
            glUniformMatrix4fv (viewMatrixLoc, 1, GL_FALSE, &view.Elements[0][0]);
            GL_CHECK_ERROR ();

            GLuint projectionMatrixLoc = glGetUniformLocation (shader->program, "projection");
            glUniformMatrix4fv (projectionMatrixLoc, 1, GL_FALSE, &projection.Elements[0][0]);
            GL_CHECK_ERROR ();
        }

        for (int i=0 ; i<g_scene.meshCount; i++)
        {
            mesh_t *mesh = g_scene.meshes + i;

            shader_t *shader = mesh->shader;

            glUseProgram (shader->program);

            // NOTE: Don't have to do every frame, everything in our scene is static anyway.
            mesh->model = mesh->T*mesh->R*mesh->S;

            GLuint rotateMatrixLoc = glGetUniformLocation (shader->program, "rotate");
            glUniformMatrix4fv (rotateMatrixLoc, 1, GL_FALSE, &mesh->R.Elements[0][0]);

            // Might be better to just pass S R T and figure out model on shader.
            // No, there is a better way to do this so that normals don't get affected by scale etc.
            GLuint modelMatrixLoc = glGetUniformLocation (shader->program, "model");
            glUniformMatrix4fv (modelMatrixLoc, 1, GL_FALSE, &mesh->model.Elements[0][0]);

            glBindVertexArray (mesh->VAO);
            GL_CHECK_ERROR ();

            glDrawElements (shader->elementType, mesh->indicesCount, GL_UNSIGNED_SHORT, 0);
            GL_CHECK_ERROR ();

            glBindVertexArray (0);
            GL_CHECK_ERROR ();
        }
    }
}
