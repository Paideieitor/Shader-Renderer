#pragma once

#include "engine.h"
#include <assimp/scene.h>

GLuint CreateProgramFromSource(String programSource, const char* shaderName);

u32 LoadProgram(App* app, const char* filepath, const char* programName);

Image LoadImage(const char* filename);

void FreeImage(Image image);

GLuint CreateTexture2DFromImage(Image image);

u32 LoadTexture2D(App* app, const char* filepath);

void ProcessAssimpMesh(const aiScene* scene, aiMesh* mesh, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);

void ProcessAssimpMaterial(App* app, aiMaterial* material, Material& myMaterial, String directory);

void ProcessAssimpNode(const aiScene* scene, aiNode* node, Mesh* myMesh, u32 baseMeshMaterialIndex, std::vector<u32>& submeshMaterialIndices);

u32 LoadModel(App* app, const char* filename);