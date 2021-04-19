#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtx/norm.hpp>

#include "Vertex.h"
#include "Triangle.h"
#include "TriangleIndexing.h"

#include <vector>

template<typename T>
size_t GetVectorSizeBytes(const std::vector<T>& Vector) {
    return Vector.size() * sizeof(T);
}

void Mesh::LoadMesh(const char* File) {
    //BoundingBox.Max = glm::vec3(-FLT_MAX);
    //BoundingBox.Min = glm::vec3( FLT_MAX);

    Assimp::Importer Importer;

    const aiScene* Scene = Importer.ReadFile(File, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    // TODO: memory reservation

    std::vector          <Vertex> Vertices;
    std::vector<TriangleIndexData> Indices;

    for (uint32_t MeshIndex = 0; MeshIndex < Scene->mNumMeshes; MeshIndex++) {
        const aiMesh* Mesh = Scene->mMeshes[MeshIndex];
        for (uint32_t VertexIndex = 0; VertexIndex < Mesh->mNumVertices; VertexIndex++) {
            Vertex CurrentVertex;

            CurrentVertex.Position = glm::vec3(Mesh->mVertices[VertexIndex].x, Mesh->mVertices[VertexIndex].y, Mesh->mVertices[VertexIndex].z);
            CurrentVertex.Normal = glm::vec3(Mesh->mNormals[VertexIndex].x, Mesh->mNormals[VertexIndex].y, Mesh->mNormals[VertexIndex].z);
            if(Mesh->mTextureCoords[0])
                CurrentVertex.TextureCoordinates = glm::vec2(Mesh->mTextureCoords[0][VertexIndex].x, Mesh->mTextureCoords[0][VertexIndex].y);

            Vertices.push_back(CurrentVertex);

            //BoundingBox.Max = glm::max(BoundingBox.Max, CurrentVertex.Position);
            //BoundingBox.Min = glm::min(BoundingBox.Min, CurrentVertex.Position);
        }
        for (uint32_t FaceIndex = 0; FaceIndex < Mesh->mNumFaces; FaceIndex++) {
            const aiFace& Face = Mesh->mFaces[FaceIndex];

            TriangleIndexData CurrentIndexData;
            for (uint32_t ElementIndex = 0; ElementIndex < Face.mNumIndices; ElementIndex++) {
                CurrentIndexData[ElementIndex] = Face.mIndices[ElementIndex];
            }

            Indices.push_back(CurrentIndexData);
        }
    }

    Importer.FreeScene();

    VertexBuffer.CreateBinding(BUFFER_TARGET_ARRAY);
    VertexBuffer.UploadData(GetVectorSizeBytes(Vertices), Vertices.data());

    ElementBuffer.CreateBinding(BUFFER_TARGET_ARRAY);
    ElementBuffer.UploadData(GetVectorSizeBytes(Indices), Indices.data());

    BufferTexture.Vertices.CreateBinding();
    BufferTexture.Vertices.SelectBuffer(&VertexBuffer, GL_RGBA32F);

    BufferTexture.Indices.CreateBinding();
    BufferTexture.Indices.SelectBuffer(&ElementBuffer, GL_RGB32UI);

    BVH.ConstructAccelerationStructure(Vertices, Indices);
}

void Mesh::LoadTexture(const char* Path) {
    Material.Diffuse.CreateBinding();
    Material.Diffuse.LoadTexture(Path);
}