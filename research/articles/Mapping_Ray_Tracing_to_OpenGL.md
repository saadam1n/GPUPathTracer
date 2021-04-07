# Mapping the Ray Tracing Equations to OpenGL

by Saad Amin, 4/7/2021

The goal of this (incomplete) article is to show the method of mapping the ray tracing equations to OpenGL. In it's current state, it does not go into much detail besides finding the intersections, although it should one day have a basic description of implementing resursive ray tracing in OpenGL, with triangles. Prerequisites: basic understanding of ray tracing and the OpenGL pipeline.

## The Ray Tracing Algorithm, on the CPU

Before we start writing OpenGL code, we need to first understand how it works on the CPU so we don't complicate ourselves too much on the start. Let's review a basic ray-tracing algorithm that outputs a different color depending if it hit something:

```cpp
// Ideally we would loop Y first to prevent cache misses, but that's outside the scope of this article
for(uint32_t X = 0; X < Width; X++) {
    for(uint32_t Y = 0; Y < Height; Y++){
        glm::ivec2 Pixel = glm::ivec2(X, Y);

        Ray PrimaryRay = GenerateCameraRay(Pixel);

        bool Hit = false;

        for(const Mesh& CurrentMesh : Scene) {

            if(Hit){
                break;
            }

            for(const Triangle& CurrentTriangle){
                if(PrimaryRay.Intersect(CurrentTriangle)){
                    Hit = true;
                    break;
                }
            }

        }

        if(Hit){
            RecordColor(OutputImage, Pixel, White);
        } else{
            RecordColor(OutputImage, Pixel, Black);
        }

    }
}

```

In summary, we loop through each pixel, and for each pixel, we generate a ray from the camera into the scene. Then, we loop through each mesh in the scene, and for each mesh, we loop through it's triangles. For each triangle, we check for an intersection, and if there is an intersection, we output the color white to the screen. If there is none, we output black.

## The Ray Tracing Algorithm, on the GPU

If we look closer, we will notice that each pixel can be treated seperately from each other, which makes ray tracing a good canidate for parellelism. Most things that can be parellelized can be ported over to the GPU easily.

First of all, we have to find a way to run the intersection code for each pixel on the GPU. Since the code runs for the entire screen, a simple solution would be to draw a fullscreen quad and run the intersection code in the fragment shader. We could output the color by using `OutputImage` as a render target in a framebuffer. However, there are more modern ways to do this. Compute shaders, which were introduces in OpenGL 4.3, have been commonly used for this purpose. A compute shader is a single stage shader that is designed to do compute work on the GPU. To output the color, we can use something called an image object in GLSL, which allows for (arbritary) reads and writes to a texture from the shader. I won't go into the specifics of compute shaders, but there are many resources online about them if you want to read further (TODO: add a further reading section)

Now we have chosen a method of doing the ray tracing code on the GPU, we have to send the triangles to the GPU for ray tracing. Some of the apparent solutions would have been uniform variables and buffers, however, we run into size limites quickly and cannot handle an arbritary number of triangles. Next, we can put our data in textures, namely a buffer texture, and then read the data back from the shader. This could work, especially if you are stuck with OpenGL 3.3 and do not have access to the fancy new features like compute shaders and our next solution to sending triangle data to the shader, which is shader storage buffer objects (SSBOs for short). A SSBO is a way for a shader to read and write to a buffer object directly. For example, we can do something like this:

```glsl
layout(std430) readonly buffer VertexBuffer {
    Vertex Vertices[];
};

```

This allows us to have a much more easier time reading and writing triangle data. Not to mention, it also works with at least 128 MB of triangle data (although most implementations allow up to the full VRAM)

Next, we need to have our intersection algorithm. I used the Möller-Trumbore algorithm, since it's both fast and accurate.
