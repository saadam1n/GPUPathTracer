#include "SceneManager.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtx/norm.hpp>

#include "Vertex.h"
#include "Triangle.h"
#include "TriangleIndexing.h"

#include <vector>
#include <iostream>
#include <sstream>

void SceneManager::LoadScene(const char* Path) {
    std::string CXXPath = Path;
    std::string Folder = CXXPath.substr(0, CXXPath.find_last_of('/') + 1);

    Assimp::Importer Importer;

    // Turn off smooth normals for path tracing to prevent "broken" BRDFs and energy loss. 
    const aiScene* Scene = Importer.ReadFile(Path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_GenUVCoords);

    for (uint32_t MeshIndex = 0; MeshIndex < Scene->mNumMeshes; MeshIndex++) {
        const aiMesh* SceneComponent = Scene->mMeshes[MeshIndex];

        std::vector           <Vertex> Vertices;
        std::vector<TriangleIndexData> Indices ;

        Vertices.reserve(SceneComponent->mNumVertices);
        Indices.reserve (SceneComponent->mNumFaces   );

        glm::vec3 MeshColor;


        Vertices.reserve(Vertices.size() + SceneComponent->mNumVertices);
        for (uint32_t VertexIndex = 0; VertexIndex < SceneComponent->mNumVertices; VertexIndex++) {
            Vertex CurrentVertex;

            aiVector3D& Position = SceneComponent->mVertices[VertexIndex];
            aiVector3D& Normal = SceneComponent->mNormals[VertexIndex];

            CurrentVertex.Position = glm::vec3(Position.x, Position.y, Position.z);
            CurrentVertex.Normal = glm::vec3(Normal.x, Normal.y, Normal.z);

            if (SceneComponent->mTextureCoords[0]) {
                aiVector3D& TextureCoordinates = SceneComponent->mTextureCoords[0][VertexIndex];
                CurrentVertex.TextureCoordinates = glm::vec2(TextureCoordinates.x, TextureCoordinates.y);
            }

            Vertices.push_back(CurrentVertex);

            if(SceneComponent->mColors[0])
                MeshColor = glm::vec3(SceneComponent->mColors[0][VertexIndex].r, SceneComponent->mColors[0][VertexIndex].g, SceneComponent->mColors[0][VertexIndex].b);
        }

        Indices.reserve(Indices.size() + SceneComponent->mNumFaces);
        for (uint32_t FaceIndex = 0; FaceIndex < SceneComponent->mNumFaces; FaceIndex++) {
            const aiFace& Face = SceneComponent->mFaces[FaceIndex];

            TriangleIndexData CurrentIndexData;
            for (uint32_t ElementIndex = 0; ElementIndex < Face.mNumIndices; ElementIndex++) {
                CurrentIndexData[ElementIndex] = Face.mIndices[ElementIndex];
            }

            Indices.push_back(CurrentIndexData);
        }

        Mesh CurrentMesh;

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
            CurrentMesh.SetColor(MeshColor);
        }

        Meshes.push_back(CurrentMesh);
    }

    Importer.FreeScene();
}

std::vector<Mesh>::iterator SceneManager::StartMeshIterator(void) {
	return Meshes.begin();
}

std::vector<Mesh>::iterator SceneManager::StopMeshIterator (void) {
	return Meshes.end();
}