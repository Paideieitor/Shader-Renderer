//
// engine.h: This file contains the types and functions relative to the engine.
//

#pragma once

#include "platform.h"
#include <glad/glad.h>

struct Image
{
    void* pixels;
    glm::ivec2 size;
    i32 nchannels;
    i32 stride;
};

struct VertexV3V2
{
    glm::vec3 pos;
    glm::vec2 uv;
};

struct VertexBufferAttribute
{
    u8 location;
    u8 componentCount;
    u8 offset;
};

struct VertexBufferLayout
{
    std::vector<VertexBufferAttribute> attributes;
    u8 stride;
};

struct VertexShaderAttribute
{
    u8 location;
    u8 componentCount;
};

struct VertexShaderLayout
{
    std::vector<VertexShaderAttribute> attributes;
};

struct VAO
{
    GLuint handle;
    GLuint programHandle;
};

struct Texture
{
    GLuint handle;
    std::string filepath;
};

struct Material
{
    std::string name;
    glm::vec3 albedo;
    glm::vec3 emissive;
    f32 smoothness;
    u32 albedoTextureIdx;
    u32 emissiveTextureIdx;
    u32 specularTextureIdx;
    u32 normalsTextureIdx;
    u32 bumpTextureIdx;
};

struct Submesh
{
    VertexBufferLayout vertexBufferLayout;
    std::vector<float> vertices;
    std::vector<u32> indices;
    u32 vertexOffset;
    u32 indexOffset;

    std::vector<VAO> vaos;
};

struct Mesh
{
    std::vector<Submesh> submeshes;
    GLuint vertexBufferHandle;
    GLuint indexBufferHandle;
};

struct Model
{
    u32 meshIdx;
    std::vector<u32> materialIdx;

    GLuint programIndex;
};

struct Program
{
    GLuint handle;
    GLuint uTexture;
    std::string filepath;
    std::string programName;
    u64 lastWriteTimestamp; // What is this for?

    VertexShaderLayout vetexInputLayout;
};

enum class Mode
{
    TexturedMesh
};

struct App
{
    // Loop
    f32  deltaTime;
    bool isRunning;
    
    // Input
    Input input;

    // Graphics
    GLubyte gpuName[64];
    GLubyte openGlVersion[64];
    GLubyte openGlVendor[64];
    GLubyte GLSLVersion[64];
    std::vector<GLubyte*> openGLExtensions;

    glm::ivec2 displaySize;

    // Render data
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Model> models;
    std::vector<Program> programs;

    // Transforms
    float aspectRatio;
    float znear;
    float zfar;
    glm::mat4 projection;
    glm::mat4 view;

    // Mode
    Mode mode;
};

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);
