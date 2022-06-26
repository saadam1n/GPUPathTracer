#include "Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtx/norm.hpp>

#include "../math/Vertex.h"
#include "../math/Triangle.h"
#include "../math/TriangleIndexing.h"

#include <vector>
#include <iostream>
#include <sstream>
#include <map>

#include <SOIL2.h>
#include <cmath>

#include <glm/gtx/matrix_transform_2d.hpp>

using namespace glm;

void Scene::LoadScene(const std::string& path, TextureCubemap* environment) {
    totalLightArea = 0;
    std::vector<MaterialInstance> materialInstances;
    
    textures.push_back(environment);

    MaterialInstance sky;
    sky.isEmissive = true;
    sky.emission = glm::vec3(1.0);
    sky.samplerHandle = glGetTextureHandleARB(environment->GetHandle());
    glMakeTextureHandleResidentARB(sky.samplerHandle);
    materialInstances.push_back(sky);

    std::string Folder = path.substr(0, path.find_last_of('/') + 1);

    Assimp::Importer importer;

    // Turn off smooth normals for path tracing to prevent "broken" BRDFs and energy loss.
    const aiScene* Scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenUVCoords);

    
    // Third order of business: load vertices into a megabuffer and transform texcoords to location on atlas

    std::vector           <Vertex> Vertices;
    std::vector<TriangleIndexData> Indices;

    /*
    Short explanation of why we need base vertex:

    Typically, any graphics program, like my ray tracer, expects that the zeroth index points to the first vertex of the object
    However, when we stuff many meshes into one buffer, index zero for an object might actually mean index zero for another object
    So we need to keep tract of what actually us the zzeroth index and add that to each vertex to make this work
    */
    int BaseVertex = 0;

    auto PosMod = [](float x, float y) {
        float md = std::fmod(x, y);
        if (md < 0) {
            md += y;
        }
        return md;
    };

    std::map<std::string, int> TexCache;
    std::vector<Triangle> lightTriangles;

    for (uint32_t i = 0; i < Scene->mNumMeshes; i++) {
        const aiMesh* currMesh = Scene->mMeshes[i];

        aiColor3D albedo, emission;

        int currMatID;

        aiMaterial* mat = Scene->mMaterials[Scene->mMeshes[i]->mMaterialIndex];
        bool hasTextures = (mat->GetTextureCount(aiTextureType_DIFFUSE) != 0);
        
        std::string textureKey;
        if (hasTextures) {
            aiString localpath;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &localpath);

            textureKey = Folder + localpath.C_Str();
        } else {
            mat->Get(AI_MATKEY_COLOR_DIFFUSE , albedo  );
            mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission);

            std::stringstream triple;
            triple << albedo.r   << ' ' << albedo.g     << ' ' << albedo.b   << ';';
            triple << emission.r << ' ' << emission.g   << ' ' << emission.b;
            textureKey = triple.str();
        }

        auto result = TexCache.find(textureKey);
        if (result != TexCache.end()) {
            currMatID = result->second;
        }
        else {
            currMatID = 2 * (int)textures.size();
            TexCache.insert({ textureKey, currMatID });

            Texture2D* currtex = new Texture2D;
            currtex->CreateBinding();

            if (hasTextures) {
                currtex->LoadTexture(textureKey.c_str());
            }
            else {
                float coldata[4]{ albedo.r, albedo.g, albedo.b, 1.0 };
                currtex->LoadData(GL_RGBA32F, GL_RGBA, GL_FLOAT, 1, 1, (void*)coldata);
                currtex->SaveData(GL_FLOAT, 1, 1, (void*)coldata);
            }

            textures.push_back(currtex);

            MaterialInstance newInstance;
            newInstance.isEmissive = 0;

            newInstance.samplerHandle = glGetTextureHandleARB(currtex->GetHandle());
            glMakeTextureHandleResidentARB(newInstance.samplerHandle);

            if (!hasTextures) {
                if (emission.r + emission.g + emission.b > 0.001f) {
                    newInstance.isEmissive = 1;
                    newInstance.emission = vec3(emission.r, emission.g, emission.b);
                }
            }

            materialInstances.push_back(newInstance);
        }

        for (uint32_t j = 0; j < currMesh->mNumVertices; j++) {
            Vertex CurrentVertex;

            aiVector3D& Position = Scene->mRootNode->mTransformation * currMesh->mVertices[j];
            aiVector3D& Normal = currMesh->mNormals[j];

            CurrentVertex.position = glm::vec3(Position.x, Position.y, Position.z);
            CurrentVertex.normal = glm::vec3(Normal.x, Normal.y, Normal.z);

            if (currMesh->mTextureCoords[0])
                CurrentVertex.texcoords = glm::vec2(currMesh->mTextureCoords[0][j].x, currMesh->mTextureCoords[0][j].y);
            else
                CurrentVertex.texcoords = glm::vec2(0.0f);

            CurrentVertex.matId = currMatID;

            Vertices.push_back(CurrentVertex);
        }

        for (uint32_t j = 0; j < currMesh->mNumFaces; j++) {
            const aiFace& Face = currMesh->mFaces[j];

            TriangleIndexData CurrentIndexData;
            for (uint32_t k = 0; k < Face.mNumIndices; k++) {
                CurrentIndexData[k] = BaseVertex + Face.mIndices[k];
            }

            Indices.push_back(CurrentIndexData);

            if (materialInstances[currMatID / 2].isEmissive == 1) {
                Triangle lightTriangle;
                lightTriangle.Vertices[0] = Vertices[CurrentIndexData[0]];
                lightTriangle.Vertices[1] = Vertices[CurrentIndexData[1]];
                lightTriangle.Vertices[2] = Vertices[CurrentIndexData[2]];

                float a = distance(lightTriangle.Vertices[0].position, lightTriangle.Vertices[1].position);
                float b = distance(lightTriangle.Vertices[0].position, lightTriangle.Vertices[2].position);
                float c = distance(lightTriangle.Vertices[2].position, lightTriangle.Vertices[1].position);

                float s = (a + b + c) / 2;
                totalLightArea += sqrtf(s * (s - a) * (s - b) * (s - c));
                lightTriangle.Vertices[0].area = totalLightArea;
                lightTriangle.Vertices[1].area = totalLightArea;
                lightTriangle.Vertices[2].area = totalLightArea;
                lightTriangles.push_back(lightTriangle);
            }
        }

        

        BaseVertex += currMesh->mNumVertices;
    }

    // As soon as we are done loading we should ignore it
    importer.FreeScene();

    bvh.ConstructAccelerationStructure(Vertices, Indices);

    materialsBuf.CreateBinding(BUFFER_TARGET_SHADER_STORAGE);
    materialsBuf.UploadData(materialInstances, GL_STATIC_DRAW);

    materialVec = materialInstances;

    vertexBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    vertexBuf.UploadData(Vertices, GL_STATIC_DRAW);

    vertexTex.CreateBinding();
    vertexTex.SelectBuffer(&vertexBuf, GL_RGBA32F);

    indexBuf.CreateBinding(BUFFER_TARGET_SHADER_STORAGE);
    indexBuf.UploadData(Indices, GL_STATIC_DRAW);

    indexTex.CreateBinding();
    indexTex.SelectBuffer(&indexBuf, GL_RGBA32UI);

    lightBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    lightBuf.UploadData(lightTriangles, GL_STATIC_DRAW);

    lightTex.CreateBinding();
    lightTex.SelectBuffer(&lightBuf, GL_RGBA32F);

    vertexVec = Vertices;
    indexVec = Indices;
}

/*
std::string CXXPath = Path;
    std::string Folder = CXXPath.substr(0, CXXPath.find_last_of('/') + 1);

    Assimp::Importer Importer;

    // Turn off smooth normals for path tracing to prevent "broken" BRDFs and energy loss.
    const aiScene* Scene = Importer.ReadFile(Path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenUVCoords);
    Meshes.resize(Scene->mNumMeshes);

    for (uint32_t MeshIndex = 0; MeshIndex < Scene->mNumMeshes; MeshIndex++) {
        const aiMesh* SceneComponent = Scene->mMeshes[MeshIndex];

        std::vector           <Vertex> Vertices;
        std::vector<TriangleIndexData> Indices ;

        Vertices.reserve(SceneComponent->mNumVertices);
        Indices.reserve (SceneComponent->mNumFaces   );

        for (uint32_t VertexIndex = 0; VertexIndex < SceneComponent->mNumVertices; VertexIndex++) {
            Vertex CurrentVertex;

            aiVector3D& Position = SceneComponent->mVertices[VertexIndex];
            aiVector3D& Normal = SceneComponent->mNormals[VertexIndex];

            CurrentVertex.Position = glm::vec3(Position.x, Position.y, Position.z);
            CurrentVertex.Normal = glm::vec3(Normal.x, Normal.y, Normal.z);

            if (SceneComponent->mTextureCoords[0])
                CurrentVertex.TextureCoordinates = glm::vec2(SceneComponent->mTextureCoords[0][VertexIndex].x, SceneComponent->mTextureCoords[0][VertexIndex].y);
            else
                CurrentVertex.TextureCoordinates = glm::vec2(0.0f);

            Vertices.push_back(CurrentVertex);
        }

        for (uint32_t FaceIndex = 0; FaceIndex < SceneComponent->mNumFaces; FaceIndex++) {
            const aiFace& Face = SceneComponent->mFaces[FaceIndex];

            TriangleIndexData CurrentIndexData;
            for (uint32_t ElementIndex = 0; ElementIndex < Face.mNumIndices; ElementIndex++) {
                CurrentIndexData[ElementIndex] = Face.mIndices[ElementIndex];
            }

            Indices.push_back(CurrentIndexData);
        }

        Mesh& CurrentMesh = Meshes[MeshIndex];

        CurrentMesh.LoadMesh(Vertices, Indices);

        // Load texture

        aiMaterial* MaterialData = Scene->mMaterials[SceneComponent->mMaterialIndex];

        if (MaterialData->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString DiffuseTex;
            MaterialData->GetTexture(aiTextureType_DIFFUSE, 0, &DiffuseTex);

            std::ostringstream StringBuilder;
            StringBuilder << Folder << DiffuseTex.C_Str();
            CurrentMesh.LoadTexture(StringBuilder.str().c_str());
        } else {
            aiColor3D Diffuse;
            MaterialData->Get(AI_MATKEY_COLOR_DIFFUSE, Diffuse);
            CurrentMesh.SetColor(glm::vec3(Diffuse.r, Diffuse.g, Diffuse.b));
        }
    }

    Importer.FreeScene();
*/

/*
// First order of business: organize the texture atlas

    // The first thing we will need to do is to get the rectpack library set up

    // Avoid being overly verbose when we don't need it
    using namespace rectpack2D;

    // See example on the rectpack2D repo https://github.com/TeamHypersomnia/rectpack2D/blob/master/example/main.cpp
    using SpacesType = empty_spaces<false>;
    using RectType = output_rect_t<SpacesType>;

    // Now at this point in the example the program begins to create the rectangles
    std::vector<RectType> Dims; // Store the dimensions for our textures
    std::vector<unsigned char*> Images; // Store the actual data for each images
    std::map<std::string, int> TexCache; // Store indicies into TextureDim for each file path
    std::vector<int> AssociatedTex; // Store the indicies into TextureDim for each mesh

    Dims.reserve(Scene->mNumMeshes);
    Images.reserve(Scene->mNumMeshes);
    AssociatedTex.reserve(Scene->mNumMeshes);

    for (int i = 0; i < Scene->mNumMeshes; i++) {
        aiMaterial* mat = Scene->mMaterials[Scene->mMeshes[i]->mMaterialIndex];
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString localpath;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &localpath);

            std::string location = Folder + localpath.C_Str();

            // Now we check if this texture already exists in our registry
            auto result = TexCache.find(location);
            if (result == TexCache.end()) {
                // New texture
                int j = Dims.size(); // This will becomes its index

                // Load from disk
                int width, height, channels;
                Images.push_back(SOIL_load_image(location.c_str(), &width, &height, &channels, SOIL_LOAD_RGBA));

                // Create new size entry
                Dims.push_back(rect_xywh{ 0, 0, width, height });

                // Create new texture cache entry
                TexCache.insert({ location, j });

                // And finally tell this mesh index to use this texture
                AssociatedTex.push_back(j);
            }
            else {
                // We that makes our job a lot easier, we just associate this mesh with the found texture
                AssociatedTex.push_back(result->second);
            }
        }
        else {
            // Now if we don't have a texture BUT a color, we simply just create a 1x1 texture
            // But when time comes to free the memory, how will we tell if it needs to be free by SOIL or us via delete?
            // We simply set the data to nullptr and set the path in the registry as the RGB color
            aiColor3D diffcol;
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffcol);

            std::stringstream triple;
            triple << diffcol.r << ' ' << diffcol.g << ' ' << diffcol.b;

            auto result = TexCache.find(triple.str());
            if (result == TexCache.end()) {
                int j = Dims.size();

                Images.push_back(nullptr);
                Dims.push_back(rect_xywh(0, 0, 1, 1));
                TexCache.insert({ triple.str(), j });
                AssociatedTex.push_back(j);
            }
            else {
                AssociatedTex.push_back(result->second);
            }

        }
    }

    auto ReportSuccesful = [](RectType&) {
        return callback_result::CONTINUE_PACKING;
    };

    auto ReportUnsuccesful = [](RectType&) {
        return callback_result::ABORT_PACKING;
    };

    auto AtlasDims = find_best_packing_dont_sort<SpacesType>(Dims, make_finder_input(16384, -4, ReportSuccesful, ReportUnsuccesful, flipping_option::DISABLED));

    // Second order of business: load the texture atlas

    // Create the # of pixels times 4 channels per pixel
    unsigned char* AtlasData = new unsigned char[AtlasDims.area() * 4];

    for (int i = 0; i < Dims.size(); i++) {
        // If Images[i] is not nullptr we simply map the data to the atlas
        // I assume that data is given column by column then row by row
        // So the correct way to index is y * width + x
        // TODO: fix the caching here

        auto CalcIdx = [](int width, int x, int y) {
            return 4 * (y * width + x);
        };

        auto WriteCol = [](unsigned char* ptr, int idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0) {
            ptr[idx  ] = r;
            ptr[idx+1] = g;
            ptr[idx+2] = b;
            ptr[idx+3] = a;
        };

        if (Images[i]) {
            // Simply copy it over and free data

            auto& currdims = Dims[i];
            for (int y = 0; y < currdims.h; y++) {
                for (int x = 0; x < currdims.w; x++) {
                    int ReadIdx = CalcIdx(currdims.w, x, y);
                    WriteCol(AtlasData, CalcIdx(AtlasDims.w, x + currdims.x, y + currdims.y), Images[i][ReadIdx], Images[i][ReadIdx+1], Images[i][ReadIdx+2], Images[i][ReadIdx+3]);
                }
            }

            SOIL_free_image_data(Images[i]);
        }
        else {
            // Find and prase
            for (const auto& p : TexCache) {
                if (p.second == i) {
                    float r, g, b;
                    std::istringstream parser(p.first);
                    parser >> r >> g >> b;
                    // Write and break
                    WriteCol(AtlasData, CalcIdx(AtlasDims.w, Dims[i].x, Dims[i].y), (unsigned char) r * 255, (unsigned char) g * 255, (unsigned char) b * 255);
                    std::cout << "Writing to " << Dims[i].x << ' ' << Dims[i].y << '\n';
                    break;
                }
            }
        }
    }

    // Upload and delete
    Atlas.CreateBinding();
    Atlas.LoadData(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, AtlasDims.w, AtlasDims.h, AtlasData);
    delete[] AtlasData;

*/