#include "Renderer.h"
#include "OpenGL.h"

#include <stdio.h>
#include <iostream>
#include <glm/glm.hpp>

using namespace glm;

/*
When we trace a ray, we don't actually care about the ray, we care about the path
There are two things a path is always associated with:
1. The pixel it belongs to, so we can update the framebuffer after we are done rendering the current frames
2. Throughput (energy transmitted from one end of the path to the other, needed for any basic path tracing)
3. Accumulated energy (for NEE)
When we generate a new ray, we only care about two things:
1. Origin
2. And direction
When we query the intersection result of a ray, we discard any information about it as the intersection result is what is worth to us now:
1. The position of the vertex
2. Normal vector
3. Texture coordinate
4. material ID

Memory wise this is a big screw-up but we programmers are gonna have to suck it up and deal with it. At least I hope the newer GPUs have loads of memory laying around
Either way, the base part (the pixel and energy information) align nicely to 32 bytes (ivec2-vec3-vec3 combo, woohoo!)
This allows us to start over again for the ray and vertex information
The ray unforunately only goes up to vec3-vec3 (24 byes) so we have to add 8 bytes of padding
That's not the worst bit. The standard information about hte vertex (pos, norm, and tex coords) round nicely to 32 bytes as a vec3-vec3-vec2 combo. 
But we need to add a material ID, which is four bytes, which forces us to add another 16 bytes for raeding/write reasons and general performance.
So if we add that up, 32+32+16 = 64+16=80 BYTES FOR JUST A SINGLE RAY
Wow. Now let's do some math, 1920 * 1080 * 80, or 165888000 bytes. Thankfully this is just 158.203125 MB, far short of what even older GPUs require
Even my weak and old GTX 750 Ti that is laying around somewhere which was my first graphics card had 2 GB of memory, ample for our task
In fact, your office PC's GT 210 has 1 GB of VRAM, good enough for 158 MB

Btw, 80/16 is 5, so 5 vec4s for each ray

Now, let's code
*/
struct RayInfo {
    ivec2 pixel;
    vec3 throughput;
    vec3 accumulated;
    
    union {
        struct {
            // read from shader as vec4 to make my life easier, and that memory would be wasted anyway
            vec4 origin;
            vec4 pos;
        };
        struct {
            vec3 position;
            vec3 normal;
            vec2 texcoord;
            uint32_t matID;
            uint32_t padding[3];
        };
    };

};

void DebugMessageCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* msg, const void* data)
{
    char* _source;
    char* _type;
    char* _severity;

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        _source = "API";
        break;

    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        _source = "WINDOW SYSTEM";
        break;

    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        _source = "SHADER COMPILER";
        break;

    case GL_DEBUG_SOURCE_THIRD_PARTY:
        _source = "THIRD PARTY";
        break;

    case GL_DEBUG_SOURCE_APPLICATION:
        _source = "APPLICATION";
        break;

    case GL_DEBUG_SOURCE_OTHER:
        _source = "UNKNOWN";
        break;

    default:
        _source = "UNKNOWN";
        break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        _type = "ERROR";
        break;

    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        _type = "DEPRECATED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        _type = "UDEFINED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_PORTABILITY:
        _type = "PORTABILITY";
        break;

    case GL_DEBUG_TYPE_PERFORMANCE:
        _type = "PERFORMANCE";
        break;

    case GL_DEBUG_TYPE_OTHER:
        _type = "OTHER";
        break;

    case GL_DEBUG_TYPE_MARKER:
        _type = "MARKER";
        break;

    default:
        _type = "UNKNOWN";
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        _severity = "HIGH";
        break;

    case GL_DEBUG_SEVERITY_MEDIUM:
        _severity = "MEDIUM";
        break;

    case GL_DEBUG_SEVERITY_LOW:
        _severity = "LOW";
        break;

    case GL_DEBUG_SEVERITY_NOTIFICATION:
        // Sorry, but I don't care and I also don't want you cluterring my log
        return;
        _severity = "NOTIFICATION";
        break;

    default:
        _severity = "UNKNOWN";
        break;
    }

    printf("%d: %s of %s severity, raised from %s: %s\n",
        id, _type, _severity, _source, msg);
}

void Renderer::Initialize(Window* Window, const char* scenePath) {
    bindedWindow = Window;
    viewportWidth = bindedWindow->Width;
    viewportHeight = bindedWindow->Height;

    static bool OpenGLLoaded = false;
    if (!OpenGLLoaded) {
        glewInit();
        OpenGLLoaded = true;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glClearStencil(0xFF);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(DebugMessageCallback, NULL);

    scene.LoadScene(scenePath);

    float quad[] = {
    -1.0f,  1.0f,
    -1.0f, -1.0f,
     1.0f, -1.0f,

    -1.0f,  1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f
    };

    quadBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    quadBuf.UploadData(sizeof(quad), quad, GL_STATIC_DRAW);

    screenQuad.CreateBinding();
    screenQuad.CreateStream(0, 2, 2 * sizeof(float));
    //

    presentShader.CompileFiles("present/Present.vert", "present/Present.frag");

    colorTexture.CreateBinding();
    colorTexture.LoadData(GL_RGBA16F, GL_RGBA, GL_FLOAT, viewportWidth, viewportHeight, nullptr);

    rayBuffer.CreateBinding(BUFFER_TARGET_SHADER_STORAGE);
    rayBuffer.UploadData(sizeof(RayInfo) * viewportWidth * viewportHeight, nullptr, GL_DYNAMIC_DRAW);

    rayCounter.CreateBinding(BUFFER_TARGET_ATOMIC_COUNTER);
    rayCounter.UploadData(sizeof(int) * 4, nullptr, GL_DYNAMIC_DRAW);
    rayCounter.CreateBlockBinding(BUFFER_TARGET_ATOMIC_COUNTER, 0);

    rayBuffer.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 1);
    scene.vertexBuf.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 2);
    scene.indexBuf.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 3);
    scene.bvh.nodes.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 4);
    scene.bvh.leaves.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 5);
    scene.textureHandlesBuf.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 6);
    

    genRays.CompileFile("kernel/raygen/FiniteAperture.comp");
    closestHit.CompileFile("kernel/intersect/Closest.comp");
    shadow.CompileFile("kernel/intersect/Shadow.comp");

    colorTexture.BindImageUnit(0, GL_RGBA16F);
    colorTexture.BindTextureUnit(0, GL_TEXTURE_2D);

    genRays.CreateBinding();
    genRays.LoadShaderStorageBuffer("RayBuffer", rayBuffer);
    genRays.LoadAtomicBuffer(0, rayCounter);

    closestHit.CreateBinding();

    closestHit.LoadShaderStorageBuffer("vertexBuf", scene.vertexBuf);
    closestHit.LoadShaderStorageBuffer("indexBuf", scene.indexBuf);
    closestHit.LoadShaderStorageBuffer("nodes", scene.bvh.nodes);
    closestHit.LoadShaderStorageBuffer("leaves", scene.bvh.leaves);
    closestHit.LoadShaderStorageBuffer("RayBuffer", rayBuffer);
    closestHit.LoadAtomicBuffer(0, rayCounter);

    shadow.CreateBinding();
    shadow.LoadInteger("ColorOutput", 0);
    shadow.LoadShaderStorageBuffer("Samplers", scene.textureHandlesBuf);
    shadow.LoadShaderStorageBuffer("vertexBuf", scene.vertexBuf);
    shadow.LoadShaderStorageBuffer("indexBuf", scene.indexBuf);
    shadow.LoadShaderStorageBuffer("nodes", scene.bvh.nodes);
    shadow.LoadShaderStorageBuffer("leaves", scene.bvh.leaves);
    shadow.LoadShaderStorageBuffer("RayBuffer", rayBuffer);
    shadow.LoadAtomicBuffer(0, rayCounter);

    shadow.LoadVector3F32("lightPos", glm::vec3(0.0, 400.0, 0.0));
    shadow.LoadVector3F32("lightCol", glm::vec3(1.2, 1.25, 1.3) * 300000.0f);
    shadow.LoadVector3F32("ambient", glm::vec3(0.1));

    presentShader.CreateBinding();
    presentShader.LoadFloat("exposure", 0.66);
    presentShader.LoadInteger("ColorTexture", 0);


    frameCounter = 0;
}

void Renderer::CleanUp(void) {
    // I hope the scene destructor frees its resources
    genRays.Free();
    closestHit.Free();
    presentShader.Free();

    colorTexture.Free();

    screenQuad.Free();
    quadBuf.Free();
}

void Renderer::RenderFrame(const Camera& camera)  {

    // Begin by clearing our atomic counters
    int empty[4] = { 0, 0, 0, 0 };
    glBufferSubData(BUFFER_TARGET_ATOMIC_COUNTER, 0, sizeof(empty), empty);

    genRays.CreateBinding();
    genRays.LoadCamera("Camera", camera);

    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_ALL_BARRIER_BITS);


    closestHit.CreateBinding();
    closestHit.LoadInteger("Frame", frameCounter++);

    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    shadow.CreateBinding();
    //shadow.LoadInteger("Frame", frameCounter++);

    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

void Renderer::Present() {
    presentShader.CreateBinding();
    glDrawArrays(GL_TRIANGLES, 0, 6);
}