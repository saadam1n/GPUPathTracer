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
#include <algorithm>

#include <glm/gtx/matrix_transform_2d.hpp>
// Use specifically for GLTF models
//#define TINYGLTF_IMPLEMENTATION
//#include <tiny_gltf.h>

// Use for obj files
#define TINYOBJLOADER_IMPLEMENTATION 
#define TINYOBJLOADER_USE_MAPBOX_EARCUT // Robust triangulation
#include <tiny_obj_loader.h>

using namespace glm;

// vec4 alignments: 4 8 12 16 of any combination of 4 byte values
struct CompactVertex {
    // 0 - starting
    vec3 position0;
    vec3 position1;
    vec3 position2;
    // 9
    vec3 surfaceNormal; // only need one for light vertices, and I don't want to recompute this per every NEE sample, espically when we would have 3 spaces left over
    // 12
    vec2 texcoords; // Actually wait, I need 3 vec2s, not 1. I'll deal with this later
    // 14
    uint32_t matID; 
    // 15
    float cumulativeArea; 
    // 16 - we just need 4 vec4s
};



/*
matId 0 - skybox    starting at textures[0]
matId 1 - material, starting at textures[1]
matId 2 - material, starting at textures[3]
matId 3 - material, starting at textures[5]
..................
matId n - material, starting at textures[2n - 1]
*/

void InitializeTexture(const std::string& folder, Texture2D* tex, const aiMaterial* mat, aiTextureType type, aiColor3D backup) {
    tex->CreateBinding();
    if (mat->GetTextureCount(type) == 0) {
        // No bitmap available, just use the backup color
        float coldata[4]{ backup.r, backup.g, backup.b, 1.0 };
        tex->LoadData(GL_RGBA32F, GL_RGBA, GL_FLOAT, 1, 1, (void*)coldata);
        tex->SaveData(GL_FLOAT, 1, 1, (void*)coldata);
    }
    else {
        aiString apath;
        mat->GetTexture(type, 0, &apath);
        std::string path = folder + apath.C_Str();
        std::cout << path.c_str() << '\n';
        tex->LoadTexture(path.c_str());
    }
}

/*
Right now, my ray tracer only supports the UE4-PBR model
So all I care about when creating a new MaterialInstance object is being able to load the appropriate values for my albedo, my metallic, and my roughness
Now uploading these values to the shader is the same for every single material format, but reading from the file is different
For example, obj requires me to set values based on the illumination model
So this wrapper function only takes care of creating a material instance using given parameters
Now that I think about it, this is sort of like a constructor
*/
MaterialInstance CreateMatInstance(const std::string& folder, const vec3& albedoCol, const std::string& albedoTex, const vec3& emissive, float roughness, float metallic) {
    std::cout << "before " << glGetError() << " for path " << albedoTex << '\n';

    MaterialInstance material;

    Texture2D* albedo = new Texture2D;
    Texture2D* matprop = new Texture2D;

    // For now I do not cache any textures
    
    albedo->CreateBinding();
    if (!albedoTex.empty()) {
        albedo->LoadTexture(folder + albedoTex);
    } else {
        albedo->SetColor(albedoCol);
    }

    matprop->CreateBinding();
    matprop->SetColor(vec3(0.0f, roughness, 0.0f));
    
    material.albedoHandle = albedo->MakeBindless();
    material.propertiesHandle = matprop->MakeBindless();

    material.isEmissive = (emissive.x + emissive.y + emissive.z > 1e-5f);
    material.emission = emissive;

    std::cout << "after " << glGetError() << '\n';

    return material;
}

void LoadOBJ(const std::string& path, const std::string& folder, std::vector<Vertex>& vertices, std::vector<TriangleIndexData>& indices, std::vector<MaterialInstance>& gpu_materials) {

    auto create_vec2 = [](const float* ptr) -> vec2 {return vec2(ptr[0], ptr[1]); };
    auto create_vec3 = [](const float* ptr) -> vec3 {return vec3(ptr[0], ptr[1], ptr[2]); };

    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = ""; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "Errors reading file: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty()) {
        std::cout << "Warnings reading file: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();


    std::vector<uint32_t> unpadded_indices;

    //constexpr size_t reserveSize = 64 * 1024 * 1024;
    //vertices.reserve(reserveSize); // Being prepared for 64 million triangles should be good enough
    //indices.reserve(reserveSize);
    //unpadded_indices.reserve(reserveSize);

    uint32_t base_counter = 0;
    uint32_t next_material_id = 1;
    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        uint32_t current_material = 2 * next_material_id++;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v]; // Index points towards this vertex, which we will need when translating obj indices to indices in vector vertices
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                tinyobj::real_t nx = 0, ny = 1, nz = 0;

                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0) {
                    nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                }

                tinyobj::real_t tx = 0, ty = 0;

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }

                Vertex vtx;
                vtx.position = vec3(vx, vy, vz);
                vtx.normal = vec3(nx, ny, nz);
                vtx.texcoord = vec2(tx, ty);
                vtx.matId = current_material;
                vertices.push_back(vtx);

                unpadded_indices.push_back(base_counter++);
            }
            index_offset += fv;

        }
        // per-face material
        auto& mtl = materials[shapes[s].mesh.material_ids[0]];

        float beckmann_roughness = sqrt(2.0f / (mtl.shininess + 2.0f));
        float metallic = (mtl.illum == 2 ? 0.0f : 1.0f); // the different between illum 2 and 3 is taht 3 requires ray traced reflections, which most likely implies a metallic surface

        gpu_materials.push_back(CreateMatInstance(folder, create_vec3(mtl.diffuse), mtl.diffuse_texname, create_vec3(mtl.emission), beckmann_roughness, metallic));
    }

    for (uint32_t i = 0; i < unpadded_indices.size();) {
        TriangleIndexData tid;

        tid[0] = unpadded_indices[i++];
        tid[1] = unpadded_indices[i++];
        tid[2] = unpadded_indices[i++];

        indices.push_back(tid);
    }
}

void Scene::LoadScene(const std::string& path, TextureCubemap* environment) {
    textures.push_back(environment);

    std::vector<MaterialInstance> materials;
    MaterialInstance sky;
    sky.isEmissive = true;
    sky.emission = 25.0f * glm::vec3(30.0f, 26.0f, 19.0f);
    sky.albedoHandle = environment->MakeBindless();
    materials.push_back(sky);

    std::string folder = path.substr(0, path.find_last_of('/') + 1);
    std::string extension = path.substr(path.find_last_of('.') + 1);

    std::vector           <Vertex> vertices;
    std::vector<TriangleIndexData> indices;

    if (extension == "obj") {
        LoadOBJ(path, folder, vertices, indices, materials);
    }
    else {
        // Maybe worth a shot loading via assimp
        //Assimp::Importer importer;
        //const aiScene* Scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_GenUVCoords | aiProcess_FlipUVs);
        //importer.FreeScene();
        std::cout << "Unsupported file type: " << extension << '\n';
        exit(-1);
    }


    // Use our vertex index stuff to build a triangle 
    std::vector<CompactTriangle> triangles;
    for (auto& triplet : indices) {
        uint32_t i = (uint32_t)triangles.size();

        CompactTriangle triangle;

        triangle.position0 = vertices[triplet[0]].position;
        triangle.texcoord0 = vertices[triplet[0]].texcoord;

        triangle.position1 = vertices[triplet[1]].position;
        triangle.texcoord1 = vertices[triplet[1]].texcoord;

        triangle.position2 = vertices[triplet[2]].position;
        triangle.texcoord2 = vertices[triplet[2]].texcoord;

        vec3 v01 = triangle.position1 - triangle.position0;
        vec3 v02 = triangle.position2 - triangle.position0;

        triangle.normal = normalize(cross(normalize(v01), normalize(v02)));

        vec3 average_normal = (vertices[triplet[0]].normal + vertices[triplet[1]].normal + vertices[triplet[2]].normal) / 3.0f;
        if (dot(triangle.normal, average_normal) < 0.0f) {
            triangle.normal = -triangle.normal;
        }

        triangle.material = vertices[triplet[0]].matId;

        triangles.push_back(triangle);
    }

    bvh.Construct(triangles);

    struct LightTriangleInfo {
        float area;
        uint32_t index;
    };

    std::vector<LightTriangleInfo> emitters;

    for (uint32_t i = 0; i < triangles.size(); i++) {
        const auto& triangle = triangles[i];
        if (materials[triangle.material / 2].isEmissive == 1) {
            LightTriangleInfo info;

            float a = distance(triangle.position0, triangle.position2);
            float b = distance(triangle.position0, triangle.position1);
            float c = distance(triangle.position2, triangle.position1);

            float s = (a + b + c) / 2;
            info.area = sqrtf(s * (s - a) * (s - b) * (s - c));
            info.index = i;

            //std::cout << "emitter: " << i << '\n';
            //std::cout << "material " << materials[triangle.material / 2].emission.x << '\n';

            //std::cout << "IDX: " << Vertices[triplet[0]].position.x << '\n';

            emitters.push_back(info);
        }
    }

    std::cout << "Num light vertices " << emitters.size() << '\n';

    // Make syre the biggest jumps in area are first so our binary search more often converges to closer locations
    std::sort(emitters.begin(), emitters.end(), [](const LightTriangleInfo& l, const LightTriangleInfo& r) {
        return l.area < r.area;
    });
    totalLightArea = 0.0f;
    for (LightTriangleInfo& cv : emitters) {
        totalLightArea += cv.area;
        cv.area = totalLightArea;
    }

    materialsBuf.CreateBinding(BUFFER_TARGET_SHADER_STORAGE);
    materialsBuf.UploadData(materials, GL_STATIC_DRAW);

    materialVec = materials;

    vertexBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    vertexBuf.UploadData(triangles, GL_STATIC_DRAW);

    vertexTex.CreateBinding();
    vertexTex.SelectBuffer(&vertexBuf, GL_RGBA32F);

    lightBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    lightBuf.UploadData(emitters, GL_STATIC_DRAW);

    lightTex.CreateBinding();
    lightTex.SelectBuffer(&lightBuf, GL_RG32F);

    triangleVec = triangles;
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

/*

 int BaseVertex = 0;

    std::map<std::string, int> TexCache;
    uint32_t nextMatId = 1;

    for (uint32_t i = 0; i < Scene->mNumMeshes; i++) {
        const aiMesh* currMesh = Scene->mMeshes[i];

        aiColor3D albedo, emission, specular;

        int currMatID;

        aiMaterial* mat = Scene->mMaterials[Scene->mMeshes[i]->mMaterialIndex];
        bool hasTextures = (mat->GetTextureCount(aiTextureType_DIFFUSE) != 0);
        bool hasSpecular = (mat->GetTextureCount(aiTextureType_SPECULAR) != 0);

        std::string textureKey;
        if (hasTextures) {
            aiString localpath;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &localpath);
            textureKey = Folder + localpath.C_Str();
        } else {
            mat->Get(AI_MATKEY_COLOR_DIFFUSE , albedo  );
            mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission);
            mat->Get(AI_MATKEY_COLOR_SPECULAR, specular);

            std::stringstream triple;
            triple << albedo.r   << ' ' << albedo.g   << ' ' << albedo.b   << ';';
            triple << emission.r << ' ' << emission.g << ' ' << emission.b << ';';
            triple << specular.r << ' ' << specular.g << ' ' << specular.b;
            textureKey = triple.str();
        }

        auto result = TexCache.find(textureKey);
        if (result != TexCache.end()) {
            currMatID = result->second;
        }
        else {
            currMatID = 2 * nextMatId++;
            std::cout << currMatID << '\n';
            TexCache.insert({ textureKey, currMatID });

            Texture2D* currtex = new Texture2D;
            Texture2D* spectex = new Texture2D;

            InitializeTexture(Folder, currtex, mat, aiTextureType_DIFFUSE, albedo);
            InitializeTexture(Folder, spectex, mat, aiTextureType_UNKNOWN, specular); // pbr metallic roughness

            textures.push_back(currtex);
            textures.push_back(spectex);

            MaterialInstance newInstance;
            newInstance.isEmissive = 0;

            newInstance.albedoHandle = glGetTextureHandleARB(currtex->GetHandle());
            glMakeTextureHandleResidentARB(newInstance.albedoHandle);

            newInstance.propertiesHandle = glGetTextureHandleARB(spectex->GetHandle());
            glMakeTextureHandleResidentARB(newInstance.propertiesHandle);

            newInstance.emission = vec3(0.0f);
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

            aiVector3D& Position = currMesh->mVertices[j];
            aiVector3D& Normal = currMesh->mNormals[j];
            CurrentVertex.position = glm::vec3(Position.x, Position.y, Position.z);

            if (currMesh->mTextureCoords[0])
                CurrentVertex.texcoord = glm::vec2(currMesh->mTextureCoords[0][j].x, currMesh->mTextureCoords[0][j].y);
            else
                CurrentVertex.texcoord = glm::vec2(0.0f);

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


        }
        BaseVertex += currMesh->mNumVertices;
    }
*/