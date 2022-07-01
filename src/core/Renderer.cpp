#include "Renderer.h"
#include "OpenGL.h"

#include <stdio.h>
#include <iostream>
#include <random>
#include <ctime>
#include <glm/glm.hpp>
#include <SOIL2.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <thread>
#include <mutex>

using namespace glm;
constexpr float kExposure = 5.68f;
constexpr float kMetallic = 0.0f;

// REFERENCE CPU RENDERER PARAMS
constexpr uint32_t KNumRefSamples = 1024;// 8192;// 65536 * 2; // 32k sampling
constexpr uint32_t kNumWorkers = 7;

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
    int padding0;
    int padding1;
    vec3 throughput;
    int sampleType; // bool/index, 0 = indirect, 1 = direct

    
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
            vec3 viewDir;
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

void LoadEnvironmnet(TextureCubemap* environment, const std::string& args, VertexArray& arr) {
    std::string extension = args.substr(args.find_last_of('.') + 1);
    if (args.find_first_of("GENERATE") == 0) {
        std::stringstream parser(args);
        std::string cmd, arg0, arg1;
        parser >> cmd >> arg0 >> arg1;
        if (arg0 == "COLOR") {
            vec4 color;
            if (arg1 == "BLACK") {
                color = vec4(0.0, 0.0, 0.0, 1.0);
            }
            else if (arg1 == "WHITE") {
                color = vec4(1.0, 1.0, 1.0, 1.0);
            }
            else {
                color = vec4(1.0, 0.0, 0.0, 1.0); // RED for error
            }

            environment->CreateBinding();

            for (int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, &color.r);
                environment->GetFace(i).SaveData(GL_FLOAT, 1, 1, &color.r);
            }
        }
    }
    else if (extension == "hdr" || extension == "jpg") {
        arr.CreateBinding();
        int width, height, channels;
        float* hdrData = stbi_loadf(args.c_str(), &width, &height, &channels, SOIL_LOAD_RGBA);
        if (hdrData == nullptr) exit(-1);
        Texture2D hdrTex;
        hdrTex.CreateBinding();
        hdrTex.LoadData(GL_RGBA32F, GL_RGBA, GL_FLOAT, width, height, hdrData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        hdrTex.BindTextureUnit(0, GL_TEXTURE_2D);
        stbi_image_free(hdrData);

        ShaderRasterization equirectangularConverter;
        equirectangularConverter.CompileFiles("EquirectangularConverter.vert", "EquirectangularConverter.frag");
        equirectangularConverter.CreateBinding();
        equirectangularConverter.LoadInteger("hdrTex", 0);
        equirectangularConverter.LoadMat4x4F32("captureProjection", glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f));


        constexpr uint32_t cubemapSize = 1024;
        environment->CreateBinding();
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGB32F, cubemapSize, cubemapSize);
        glViewport(0, 0, cubemapSize, cubemapSize);

        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        mat4 captureViews[] =
        {
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
           lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        for (int i = 0; i < 6; i++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environment->GetHandle(), 0);
            equirectangularConverter.LoadMat4x4F32("captureView", captureViews[i]);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);

        for (int i = 0; i < 6; i++) {
            float* download = new float[4ULL * cubemapSize * cubemapSize];
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, GL_FLOAT, download);
            environment->GetFace(i).SaveData(GL_FLOAT, cubemapSize, cubemapSize, download);
        }
    }
    else environment->LoadTexture(args); // Load TXT file

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void Renderer::Initialize(Window* Window, const char* scenePath, const std::string& env_path) {
    bindedWindow = Window;
    viewportWidth = bindedWindow->Width;
    viewportHeight = bindedWindow->Height;
    numPixels = viewportWidth * viewportHeight;

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

    float cube[] = {
        // back face
        -1.0f, -1.0f, -1.0f, 
         1.0f,  1.0f, -1.0f, 
         1.0f, -1.0f, -1.0f,        
         1.0f,  1.0f, -1.0f, 
        -1.0f, -1.0f, -1.0f, 
        -1.0f,  1.0f, -1.0f, 
        // front face
        -1.0f, -1.0f,  1.0f, 
         1.0f, -1.0f,  1.0f, 
         1.0f,  1.0f,  1.0f, 
         1.0f,  1.0f,  1.0f, 
        -1.0f,  1.0f,  1.0f, 
        -1.0f, -1.0f,  1.0f, 
        // left face
        -1.0f,  1.0f,  1.0f, 
        -1.0f,  1.0f, -1.0f, 
        -1.0f, -1.0f, -1.0f, 
        -1.0f, -1.0f, -1.0f, 
        -1.0f, -1.0f,  1.0f, 
        -1.0f,  1.0f,  1.0f, 
        // right face
         1.0f,  1.0f,  1.0f, 
         1.0f, -1.0f, -1.0f, 
         1.0f,  1.0f, -1.0f,        
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,    
        // bottom face
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f, 
         1.0f, -1.0f,  1.0f, 
         1.0f, -1.0f,  1.0f, 
        -1.0f, -1.0f,  1.0f, 
        -1.0f, -1.0f, -1.0f, 
        // top face
        -1.0f,  1.0f, -1.0f, 
         1.0f,  1.0f , 1.0f, 
         1.0f,  1.0f, -1.0f,    
         1.0f,  1.0f,  1.0f, 
        -1.0f,  1.0f, -1.0f, 
        -1.0f,  1.0f,  1.0f,        
    };

    cubeBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    cubeBuf.UploadData(sizeof(cube), cube, GL_STATIC_DRAW);

    cubeArr.CreateBinding();
    cubeArr.CreateStream(0, 3, 3 * sizeof(float));

    present.CompileFiles("Present.vert", "Present.frag");
    iterative.CompileFile("Iterative.comp");

    TextureCubemap* environment = new TextureCubemap;
    LoadEnvironmnet(environment, env_path, cubeArr);
    scene.LoadScene(scenePath, environment);

    glViewport(0, 0, viewportWidth, viewportHeight);
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

    quadArr.CreateBinding();
    quadArr.CreateStream(0, 2, 2 * sizeof(float));

    accum.CreateBinding();
    accum.LoadData(GL_RGBA32F, GL_RGBA, GL_FLOAT, viewportWidth, viewportHeight, nullptr);

    std::default_random_engine stateGenerator;
    std::uniform_int_distribution<uint32_t> initalRange(129, UINT32_MAX);
    std::vector<uvec4> threadRandomState(numPixels);
    for (uvec4& currState : threadRandomState) {
        currState.x = initalRange(stateGenerator);
        currState.y = initalRange(stateGenerator);
        currState.z = initalRange(stateGenerator);
        currState.w = initalRange(stateGenerator);
    }
    randomState.CreateBinding(BUFFER_TARGET_SHADER_STORAGE);
    randomState.UploadData(threadRandomState, GL_DYNAMIC_DRAW);
    
    scene.materialsBuf.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 4);
    randomState.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, 5);

    accum.BindImageUnit(0, GL_RGBA32F);
    accum.BindTextureUnit(0, GL_TEXTURE_2D);
    scene.vertexTex.BindTextureUnit(1, GL_TEXTURE_BUFFER);
    scene.bvh.nodesTex.BindTextureUnit(3, GL_TEXTURE_BUFFER);
    scene.lightTex.BindTextureUnit(4, GL_TEXTURE_BUFFER);

    iterative.CreateBinding();
    iterative.LoadInteger("accum", 0);
    iterative.LoadInteger("vertexTex", 1);
    iterative.LoadInteger("indexTex", 2);
    iterative.LoadInteger("nodesTex", 3);
    iterative.LoadInteger("lightTex", 4);
    iterative.LoadFloat("totalLightArea", scene.totalLightArea);
    iterative.LoadFloat("kMetallic", kMetallic);
    iterative.LoadShaderStorageBuffer("samplers", scene.materialsBuf);
    iterative.LoadShaderStorageBuffer("randomState", randomState);

    present.CreateBinding();
    present.LoadFloat("exposure", kExposure);
    present.LoadInteger("directAccum", 0);

    frameCounter = 0;
    numSamples = 0;
}

void Renderer::CleanUp(void) {
    // I hope the scene destructor frees its resources
    iterative.Free();
    present.Free();

    accum.Free();

    quadArr.Free();
    quadBuf.Free();
}

#define MEMORY_BARRIER_RT GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT

void Renderer::RenderFrame(const Camera& camera)  {
    iterative.CreateBinding();
    iterative.LoadCamera(camera, viewportWidth, viewportHeight);
    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(MEMORY_BARRIER_RT);
    numSamples++;
}


// glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);

void Renderer::Present() {
    present.CreateBinding();
    present.LoadInteger("numSamples", numSamples);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::ResetSamples() {
    numSamples = 0;
    float clearcol[4] = { 0.0, 0.0, 0.0, 1.0 };
    glClearTexSubImage(accum.GetHandle(), 0, 0, 0, 0, viewportWidth, viewportHeight, 1, GL_RGBA, GL_FLOAT, clearcol);
}

void Renderer::SaveScreenshot(const std::string& filename) {
    if (SOIL_save_screenshot(filename.c_str(), SOIL_SAVE_TYPE_PNG, 0, 0, 1280, 720)) {
        std::cout << "File \"" << filename << "\" saved successfully\n";
    }
    else {
        std::cout << "File \"" << filename << "\" failed to save!\n";
        exit(-1);
    }
}


#define BVH_STACK_SIZE 27
bool TraverseBVH(Ray ray, HitInfo& intersection, const std::vector<CompactTriangle>& triangles, const std::vector<NodeSerialized>& nodes) {
    Ray iray;

    iray.direction = 1.0f / ray.direction;
    iray.origin = -ray.origin * iray.direction;

    NodeSerialized root = nodes.front();

    if (!root.BoundingBox.Intersect(iray, intersection))
        return false;

    bool result = false;

    int currentNode = root.ChildrenNodes[0];
    int stack[BVH_STACK_SIZE];
    int index = -1;

    while (true) {
        NodeSerialized child0 = nodes.at(currentNode);
        NodeSerialized child1 = nodes.at(currentNode + 1);

        vec2 distance0, distance1;
        bool hit0 = child0.BoundingBox.Intersect(iray, intersection, distance0);
        bool hit1 = child1.BoundingBox.Intersect(iray, intersection, distance1);

        if (hit0 && child0.Leaf.Size < 0) {
            result |= child0.Intersect(ray, intersection, triangles);
            hit0 = false;
        }

        if (hit1 && child1.Leaf.Size < 0) {
            result |= child1.Intersect(ray, intersection, triangles);
            hit1 = false;
        }

        if (hit0 && hit1) {
            if (distance0.x > distance1.x)
                std::swap(child0, child1);
            stack[++index] = child1.ChildrenNodes[0];
            currentNode = child0.ChildrenNodes[0];
        }
        else if (hit0)
            currentNode = child0.ChildrenNodes[0];
        else if (hit1)
            currentNode = child1.ChildrenNodes[0];
        else
            if (index == -1)
                break;
            else
                currentNode = stack[index--];
    }

    return result;
}

uint32_t TausStep(uint32_t& z, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t m) {
    uint b = (((z << s1) ^ z) >> s2);
    z = (((z & m) << s3) ^ b);
    return z;
}

uint32_t LCGStep(uint32_t& z, uint32_t a, uint32_t c) {
    z = a * z + c;
    return z;
}

float HybridTaus(uvec4& state) {
    return 2.3283064365387e-10f * float(
        TausStep(state.x, 13, 19, 12, 4294967294U) ^
        TausStep(state.y, 2, 25, 4, 4294967288U) ^
        TausStep(state.z, 3, 11, 17, 4294967280U) ^
        LCGStep(state.w, 1664525, 1013904223U)
    );
}

#define M_PI 3.141592653589793238462643383279f

// http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
float DistributionBeckmann(vec3 n, vec3 h, float m) {
    float noh = max(dot(n, h), 0.0f);
    float noh2 = noh * noh;
    float m2 = m * m;
    float numer = exp((noh2 - 1.0f) / (m2 * noh2));
    float denom = M_PI * m2 * noh2 * noh2;
    return numer / denom;
}

vec3 FresnelShlick(vec3 f0, vec3 n, vec3 v) {
    float x = 1.0f - max(dot(n, v), 0.0f);
    return f0 + (1.0f - f0) * (x * x * x * x * x);
}

float G1_Shlick(vec3 n, vec3 v, float k) {
    float nov = max(dot(n, v), 0.0f);
    return nov / (nov * (1.0f - k) + k);
}

float GSmith(vec3 n, vec3 v, vec3 l, float m) {
    float k = m + 1.0f;
    k *= k / 8.0f;
    return G1_Shlick(n, v, k) * G1_Shlick(n, l, k);
}

// I'm using a simple BRDF instead of a proper one to make debugging easier 
vec3 BeckmannCookTorrance(vec3 albedo, float roughness, float metallic, vec3 n, vec3 v, vec3 l) {
    return albedo / M_PI;
    if (dot(n, v) < 0.0f || dot(n, l) < 0.0f) {
        return vec3(0.0f);
    }
    // Cook torrance
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    vec3 h = normalize(v + l);
    vec3 specular = FresnelShlick(f0, h, v) * DistributionBeckmann(n, h, roughness) * GSmith(n, v, l, roughness) / max(4.0f * max(dot(n, v), 0.0f) * max(dot(n, l), 0.0f), 1e-10f); // Implicit geometric term
    vec3 diffuse = albedo / M_PI * (1.0f - metallic) * (1.0f - FresnelShlick(f0, n, l)) * (1.0f - FresnelShlick(f0, n, v)); // See pbr discussion by devsh on how to do energy conservation
    return specular + diffuse;
}

vec3 ComputeTonemapUncharted2(vec3 color) {
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    float W = 11.2f;
    float exposure = 2.0f;
    color *= exposure;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    return color;
}

void PathTraceImage(
    uint8_t* image, uint32_t x, uint32_t y, const uint32_t w, const uint32_t h, const Camera& camera,
    const std::vector<CompactTriangle>& triangles, const std::vector<NodeSerialized>& nodes,
    const std::vector<MaterialInstance>& materials, const std::vector<Texture*>& textures, uvec4 state
) {
    vec3 pixel = vec3(0.0);

    for (int i = 0; i < KNumRefSamples; i++) {
        vec2 interpolation = vec2(x + HybridTaus(state), y + HybridTaus(state)) / vec2(w, h);
        Ray ray = camera.GenRay(interpolation, HybridTaus(state), HybridTaus(state));
        vec3 throughput = vec3(1.0);

        while(true) {
            HitInfo closest;

            TraverseBVH(ray, closest, triangles, nodes);
            closest.intersection.matId /= 2; // Not needed for the CPU

            if (materials[closest.intersection.matId].isEmissive) {
                vec3 emission;
                if (closest.intersection.matId == 0) {
                    const TextureCubemap* skybox = (const TextureCubemap*)textures.front();
                    emission = skybox->Sample(ray.direction);
                }
                else
                    emission = materials[closest.intersection.matId].emission;

                pixel += throughput * emission;
                break;
            }

            ray.origin = closest.intersection.position + closest.intersection.normal * 0.001f;

            vec3 normcrs = (abs(closest.intersection.normal.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0));
            vec3 tangent = normalize(cross(normcrs, closest.intersection.normal));
            vec3 bitangent = cross(tangent, closest.intersection.normal);

            vec3 viewDir = -ray.direction;

            // I do not take advantage of cosine sampling here (yet) to sit well with specular BRDFs at grazing angles
            // https://mathworld.wolfram.com/SpherePointPicking.html
            float phi = 2 * M_PI * HybridTaus(state);
            float z = HybridTaus(state);
            float r = sqrt(1.0f - z * z);
            ray.direction = mat3(tangent, bitangent, closest.intersection.normal) * vec3(r * vec2(sin(phi), cos(phi)), z);

            const Texture2D* tex = (const Texture2D*)textures[2ULL * closest.intersection.matId - 1ULL];
            const Texture2D* mat = (const Texture2D*)textures[2ULL * closest.intersection.matId];
            float roughness = mat->Sample(closest.intersection.texcoords).x;
            throughput *= BeckmannCookTorrance(tex->Sample(closest.intersection.texcoords), roughness * roughness, kMetallic, closest.intersection.normal, viewDir, ray.direction) * 2.0f * M_PI * max(dot(closest.intersection.normal, ray.direction), 0.0f); // BRDF, we need to multiply by M_PI to account for cosine PDF


            float rr = min(max(throughput.x, max(throughput.y, throughput.z)), 1.0f);
            if (HybridTaus(state) > rr)
                break;
            throughput /= rr;
        }
    }

    pixel /= KNumRefSamples;
    //pixel = 1.0f - exp(-kExposure * pixel);
    pixel = ComputeTonemapUncharted2(kExposure * pixel);
    pixel = pow(pixel, vec3(1.0f / 2.2f));

    pixel = clamp(pixel, vec3(0.0), vec3(1.0f));
    uint64_t idx = 3ULL * (y * w + x);
    image[idx    ] = (uint8_t)(255.0f * pixel.r);
    image[idx + 1] = (uint8_t)(255.0f * pixel.g);
    image[idx + 2] = (uint8_t)(255.0f * pixel.b);
}


// Render the ground truth of the image on the CPU
void Renderer::RenderReference(const Camera& camera) {
    auto filename = std::to_string(std::time(nullptr));
    SaveScreenshot("res/screenshots/" + filename + '-' + std::to_string(numSamples) + "-RENDERED.png");

    uint64_t numPixels = (uint64_t) viewportWidth * viewportHeight;
    uint8_t* image = new uint8_t[3ULL * numPixels];

    std::vector<ivec2> pixelTasks;

    for (uint32_t y = 0; y < viewportHeight; y++) {
        for (uint32_t x = 0; x < viewportWidth; x++) {
            size_t idx = 3ULL * (y * viewportWidth + x);
            image[idx] = 0;
            image[idx+1] = 0;
            image[idx+2] = 0;
            pixelTasks.emplace_back(x, y);
        }
    }

    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> distribution(129, UINT32_MAX);

    auto start = std::time(nullptr);
    uint32_t nextTask = 0;
    std::mutex taskMutex;
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < kNumWorkers; i++) {
        workers.emplace_back(
            [](
            uint32_t& nextTask, std::mutex& taskMutex, uint8_t* image, const std::vector<ivec2>& pixelTasks, uint32_t w, uint32_t h, const Camera& camera,
                const std::vector<CompactTriangle>& triangles, const std::vector<NodeSerialized>& nodes,
                const std::vector<MaterialInstance>& materials, const std::vector<Texture*>& textures,
                std::default_random_engine& generator, std::uniform_int_distribution<uint32_t>& distribution
            ) {
                while (true) {
                    taskMutex.lock();
                    uint32_t currentTask = nextTask++;
                    uvec4 state;
                    state.x = distribution(generator);
                    state.y = distribution(generator);
                    state.z = distribution(generator);
                    state.w = distribution(generator);
                    taskMutex.unlock();

                    if (currentTask >= pixelTasks.size())
                        return;

                    PathTraceImage(image, pixelTasks[currentTask].x, pixelTasks[currentTask].y, w, h, camera, triangles, nodes, materials, textures, state);
                }
            },
            std::ref(nextTask), std::ref(taskMutex), image, std::ref(pixelTasks), viewportWidth, viewportHeight, std::ref(camera), std::ref(scene.triangleVec), std::ref(scene.bvh.nodesVec), std::ref(scene.materialVec), std::ref(scene.textures), std::ref(generator), std::ref(distribution)
        );
    }

    bool renderInProgress = true;
    std::thread progressUpdateThread([&renderInProgress](time_t start, uint32_t& nextTask, size_t pixelTasksSize) {
            while (renderInProgress) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "Rendering " << 100.0f * nextTask / pixelTasksSize << "% complete\tTime remaining: " << (std::time(nullptr) - start) * ((float)pixelTasksSize - nextTask) / nextTask << " seconds\n";
            }
        }, start, std::ref(nextTask), pixelTasks.size()
    );

    Texture2D pixels;
    pixels.CreateBinding();
    pixels.LoadData(GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, viewportWidth, viewportHeight, image);
    pixels.BindTextureUnit(15, GL_TEXTURE_2D);

    //glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB, viewportWidth, viewportHeight);

    ShaderRasterization imagePresent;
    imagePresent.CompileFiles("extra/Image.vert", "extra/Image.frag");
    imagePresent.CreateBinding();
    imagePresent.LoadInteger("image", 15);

    while (nextTask < pixelTasks.size()) {
        glClear(GL_COLOR_BUFFER_BIT);
        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewportWidth, viewportHeight, GL_RGB, GL_UNSIGNED_BYTE, image);
        pixels.LoadData(GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, viewportWidth, viewportHeight, image);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        bindedWindow->Update();
    }

    imagePresent.Free();
    pixels.Free();

    for (std::thread& worker : workers)
        worker.join();

    renderInProgress = false;
    progressUpdateThread.join();
    auto deltaT = std::time(nullptr) - start;

    uint8_t* flipped = new uint8_t[3ULL * numPixels];
    // Flip the image now
    for (uint32_t y = 0; y < viewportHeight; y++) {
        for (uint32_t x = 0; x < viewportWidth; x++) {
            size_t cidx = 3ULL * (y * viewportWidth + x);
            size_t fidx = 3ULL * ((uint64_t)(viewportHeight - y - 1) * viewportWidth + x); // Flip the y
            flipped[cidx] = image[fidx];
            flipped[cidx + 1] = image[fidx + 1];
            flipped[cidx + 2] = image[fidx + 2];
        }
    }

    SOIL_save_image(("res/screenshots/" + filename + '-' + std::to_string(deltaT) + "-REFERENCE.png").c_str(), SOIL_SAVE_TYPE_PNG, viewportWidth, viewportHeight, 3, flipped);

    delete[] image;
    delete[] flipped;

    std::cout << "Pssst? You still there? Rendering completed in " << deltaT << " seconds\n";
}