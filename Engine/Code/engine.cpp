//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include "loader.h"

#include <glm/gtx/matrix_decompose.hpp>

#include <imgui.h>

GLuint FindVAO(Mesh& mesh, u32 submeshIdx, const Program& program)
{
    Submesh& submesh = mesh.submeshes[submeshIdx];

    // Try finding existing VAO
    for (u32 i = 0; i < (u32)submesh.vaos.size(); ++i)
        if (submesh.vaos[i].programHandle == program.handle)
            return submesh.vaos[i].handle;

    GLuint vaoHandle = 0;

    // Create new VAO
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

    for (u32 i = 0; i < program.vetexInputLayout.attributes.size(); ++i)
    {
        bool attributeWasLinked = false;

        for (u32 j = 0; j < submesh.vertexBufferLayout.attributes.size(); ++j)
            if (program.vetexInputLayout.attributes[i].location == submesh.vertexBufferLayout.attributes[j].location)
            {
                const u32 index = submesh.vertexBufferLayout.attributes[j].location;
                const u32 ncomp = submesh.vertexBufferLayout.attributes[j].componentCount;
                const u32 offset = submesh.vertexBufferLayout.attributes[j].offset + submesh.vertexOffset;
                const u32 stride = submesh.vertexBufferLayout.stride;
                glVertexAttribPointer(index, ncomp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)offset);
                glEnableVertexAttribArray(index);

                attributeWasLinked = true;
                break;
            }

        assert(attributeWasLinked);
    }
    glBindVertexArray(0);

    // Store new VAO in the submesh
    VAO vao = { vaoHandle, program.handle };
    submesh.vaos.push_back(vao);

    return vaoHandle;
}

bool IsPowerOf2(u32 value)
{
    return value && !(value & (value - 1));
}

u32 Align(u32 value, u32 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

Buffer CreateBuffer(u32 size, GLenum type, GLenum usage)
{
    Buffer buffer = {};
    buffer.size = size;
    buffer.type = type;

    glGenBuffers(1, &buffer.handle);
    glBindBuffer(type, buffer.handle);
    glBufferData(type, buffer.size, NULL, usage);
    glBindBuffer(type, 0);

    return buffer;
}

void BindBuffer(const Buffer& buffer)
{
    glBindBuffer(buffer.type, buffer.handle);
}

void MapBuffer(Buffer& buffer, GLenum access)
{
    glBindBuffer(buffer.type, buffer.handle);
    buffer.data = (u8*)glMapBuffer(buffer.type, access);
    buffer.head = 0;
}

void UnmapBuffer(Buffer& buffer)
{
    glUnmapBuffer(buffer.type);
    glBindBuffer(buffer.type, 0);
}

void AlignHead(Buffer& buffer, u32 alignment)
{
    ASSERT(IsPowerOf2(alignment), "The alignment must be a power of 2");
    buffer.head = Align(buffer.head, alignment);
}

void PushAlignedData(Buffer& buffer, const void* data, u32 size, u32 alignment)
{
    ASSERT(buffer.data != NULL, "The buffer must be mapped first");
    AlignHead(buffer, alignment);
    memcpy((u8*)buffer.data + buffer.head, data, size);
    buffer.head += size;
}

glm::mat4 Translate(const glm::mat4& transform, const glm::vec3& position)
{
    return glm::translate(transform, position);
}

glm::mat4 Scale(const glm::mat4& transform, const glm::vec3& scaleFactor)
{
    return glm::scale(transform, scaleFactor);
}

glm::mat4 Rotate(const glm::mat4& transform, const glm::vec3& rotation)
{
    glm::mat4 output = transform;

    output = glm::rotate(output, rotation.x, glm::vec3(1, 0, 0));
    output = glm::rotate(output, rotation.y, glm::vec3(0, 1, 0));
    output = glm::rotate(output, rotation.z, glm::vec3(0, 0, 1));

    return output;
}

void CreateModel(App* const app, u32 modelIdx, u32 programIdx, const glm::vec3& position, const glm::vec3& scaleFactor, const glm::vec3& rotation)
{
    app->entities.push_back(Entity());
    Entity& entity = app->entities.back();

    entity.modelIdx = modelIdx;

    entity.transform = Rotate(Scale(Translate(IDENTITY4, position), scaleFactor), rotation);
}

u32 BuildPlane(App* app)
{
    app->models.push_back(Model());
    Model& model = app->models.back();
    u32 modelIdx = (u32)app->models.size() - 1u;

    app->meshes.push_back(Mesh());
    Mesh& mesh = app->meshes.back();
    model.meshIdx = (u32)app->meshes.size() - 1u;

    mesh.submeshes.push_back(Submesh());
    Submesh& submesh = mesh.submeshes.back();

    // Save vertex data
    submesh.vertexOffset = 0;
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/-1, 0, -1, /*N*/0, 1, 0, /*TC*/0, 0, /*T*/0, 0, 0, /*B*/0, 0, 0 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/ 1, 0, -1, /*N*/0, 1, 0, /*TC*/1, 0, /*T*/0, 0, 0, /*B*/0, 0, 0 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/ 1, 0,  1, /*N*/0, 1, 0, /*TC*/1, 1, /*T*/0, 0, 0, /*B*/0, 0, 0 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/-1, 0,  1, /*N*/0, 1, 0, /*TC*/0, 1, /*T*/0, 0, 0, /*B*/0, 0, 0 });

    submesh.indexOffset = 0;
    submesh.indices.insert(submesh.indices.end(), { 0, 1, 2, 0, 2, 3 });

    // Save attributes to be read
    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) });
    submesh.vertexBufferLayout.stride = 6 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 2 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 3, 3, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 3 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 4, 3, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 3 * sizeof(float);

    // Create buffers
    u32 vertexBufferSize = 0;
    u32 indexBufferSize = 0;

    vertexBufferSize += submesh.vertices.size() * sizeof(float);
    indexBufferSize += submesh.indices.size() * sizeof(u32);

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    const void* verticesData = submesh.vertices.data();
    const u32   verticesSize = submesh.vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    submesh.vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = submesh.indices.data();
    const u32   indicesSize = submesh.indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    submesh.indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create Material
    app->materials.push_back(Material{});
    model.materialIdx.push_back((u32)app->materials.size() - 1u);

    Material& material = app->materials.back();
    material.name = "Plane";
    material.albedo = { 1,1,1 };
    material.albedoTextureIdx = LoadTexture2D(app, "dice.png");

    return modelIdx;
}

u32 BuildSphere(App* app)
{
    app->models.push_back(Model());
    Model& model = app->models.back();
    u32 modelIdx = (u32)app->models.size() - 1u;

    app->meshes.push_back(Mesh());
    Mesh& mesh = app->meshes.back();
    model.meshIdx = (u32)app->meshes.size() - 1u;

    mesh.submeshes.push_back(Submesh());
    Submesh& submesh = mesh.submeshes.back();

    // Save vertex data
    submesh.vertexOffset = 0;
    submesh.indexOffset = 0;

    const u32 H = 32;
    const u32 V = 16;
    struct Vertex { glm::vec3 pos; glm::vec3 norm; };
    Vertex sphere[H][V + 1];

    for (int h = 0; h < H; ++h)
    {
        for (int v = 0; v < V + 1; ++v)
        {
            float nh = float(h) / H;
            float nv = float(v) / V - 0.5f;
            float angleh = 2 * PI * nh;
            float anglev = -PI * nv;
            sphere[h][v].pos.x = sinf(angleh) * cosf(anglev);
            sphere[h][v].pos.y = -sinf(anglev);
            sphere[h][v].pos.z = cosf(angleh) * cosf(anglev);
            sphere[h][v].norm = glm::normalize(sphere[h][v].pos);

            submesh.vertices.insert(submesh.vertices.end(), { /*V*/sphere[h][v].pos.x, sphere[h][v].pos.y, sphere[h][v].pos.z, 
                                                              /*N*/sphere[h][v].norm.x, sphere[h][v].norm.y, sphere[h][v].norm.z, 
                                                              /*TC*/0, 0, /*T*/0, 0, 0, /*B*/0, 0, 0 });
        }
    }

    u32 sphereIndices[H][V][6];
    for (u32 h = 0; h < H; ++h)
    {
        for (u32 v = 0; v < V; ++v)
        {
            sphereIndices[h][v][0] = (h + 0) * (V + 1) + v;
            sphereIndices[h][v][1] = ((h + 1) % H) * (V + 1) + v;
            sphereIndices[h][v][2] = ((h + 1) % H) * (V + 1) + v + 1;
            sphereIndices[h][v][3] = (h + 0) * (V + 1) + v;
            sphereIndices[h][v][4] = ((h + 1) % H) * (V + 1) + v + 1;
            sphereIndices[h][v][5] = (h + 0) * (V + 1) + v + 1;

            submesh.indices.insert(submesh.indices.end(), { sphereIndices[h][v][0], sphereIndices[h][v][1], sphereIndices[h][v][2], 
                                                            sphereIndices[h][v][3], sphereIndices[h][v][4], sphereIndices[h][v][5] });
        }
    }

    // Save attributes to be read
    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 3, 3 * sizeof(float) });
    submesh.vertexBufferLayout.stride = 6 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 2, 2, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 2 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 3, 3, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 3 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 4, 3, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 3 * sizeof(float);

    // Create buffers
    u32 vertexBufferSize = 0;
    u32 indexBufferSize = 0;

    vertexBufferSize += submesh.vertices.size() * sizeof(float);
    indexBufferSize += submesh.indices.size() * sizeof(u32);

    glGenBuffers(1, &mesh.vertexBufferHandle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.indexBufferHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

    u32 indicesOffset = 0;
    u32 verticesOffset = 0;

    const void* verticesData = submesh.vertices.data();
    const u32   verticesSize = submesh.vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    submesh.vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = submesh.indices.data();
    const u32   indicesSize = submesh.indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    submesh.indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create Material
    app->materials.push_back(Material{});
    model.materialIdx.push_back((u32)app->materials.size() - 1u);

    Material& material = app->materials.back();
    material.name = "Sphere";
    material.albedo = { 1,1,1 };
    material.albedoTextureIdx = LoadTexture2D(app, "color_white.png");

    return modelIdx;
}

void BuildPrimitives(App* app)
{
    app->planeIdx = BuildPlane(app);
    app->sphereIdx = BuildSphere(app);
}

void CreateLight(App* const app, const Light::Type& type, const glm::vec3& color, const glm::vec3& direction, const glm::vec3& position)
{
    app->lights.push_back(Light());
    Light& light = app->lights.back();

    light.type = type;
    light.color = color;
    light.direction = direction;
    light.position = position;
}

void CreateColorAttachment(GLuint& handle, const glm::ivec2& displaySize)
{
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, displaySize.x, displaySize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Init(App* app)
{
    BuildPrimitives(app);
        
    app->mode = Mode::COLOR;
    
    app->aspectRatio = (float)app->displaySize.x / (float)app->displaySize.y;
    app->znear = 0.1f;
    app->zfar = 1000.0f;
    app->projection = glm::perspective(glm::radians(60.0f), app->aspectRatio, app->znear, app->zfar);
    
    app->cameraPosition = glm::vec3(0, 0, 20);
    app->cameraDirection = glm::normalize(glm::vec3(0) - app->cameraPosition);
    app->view = glm::lookAt(app->cameraPosition, app->cameraDirection, glm::vec3(0,1,0));
    
    // Saving openGL details
    memcpy(app->openGlVersion, glGetString(GL_VERSION), 64);
    memcpy(app->gpuName, glGetString(GL_RENDERER), 64);
    memcpy(app->openGlVendor, glGetString(GL_VENDOR), 64);
    memcpy(app->GLSLVersion, glGetString(GL_SHADING_LANGUAGE_VERSION), 64);
    GLint extensionNum;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionNum);
    for (int i = 0; i < extensionNum; ++i)
    {
        GLubyte extension[64];
        memcpy(extension, glGetStringi(GL_EXTENSIONS, GLuint(i)), 64);
        app->openGLExtensions.push_back(extension);
    }

    // Fill vertex input layout with required attributes
    GLuint programIdx = LoadProgram(app, "shaders.glsl", "TEXTURED_MESH");
    Program& texturedMeshProgram = app->programs[programIdx];
    
    // Create entities
    u32 modelIdx = LoadModel(app, "Patrick/Patrick.obj", programIdx);
    CreateModel(app, modelIdx, programIdx, glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    CreateModel(app, modelIdx, programIdx, glm::vec3(5, 0, 3), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    CreateModel(app, modelIdx, programIdx, glm::vec3(-5, 0, 3), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    CreateModel(app, modelIdx, programIdx, glm::vec3(10, 0, 6), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    CreateModel(app, modelIdx, programIdx, glm::vec3(-10, 0, 6), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));

    //CreateModel(app, app->planeIdx, programIdx, glm::vec3(0, -3.4, 0), glm::vec3(100, 100, 100), glm::vec3(0, 0, 0));
    CreateModel(app, app->sphereIdx, programIdx, glm::vec3(0, -3.4, 0), glm::vec3(10, 10, 10), glm::vec3(0, 0, 0));
    
    // Create lights
    CreateLight(app, Light::Type::DIRECTIONAL, glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), glm::vec3(1, 1, 1));
    CreateLight(app, Light::Type::DIRECTIONAL, glm::vec3(0, 0, 1), glm::vec3(-1, 1, 1), glm::vec3(1, 1, 1));
    
    // Screen
    glGenBuffers(1, &app->screen.verticesHandle);
    glBindBuffer(GL_ARRAY_BUFFER, app->screen.verticesHandle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(app->screen.vertices), app->screen.vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glGenBuffers(1, &app->screen.indicesHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->screen.indicesHandle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(app->screen.indices), app->screen.indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);   
    
    glGenVertexArrays(1, &app->screen.vao);
    glBindVertexArray(app->screen.vao);
    glBindBuffer(GL_ARRAY_BUFFER, app->screen.verticesHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->screen.indicesHandle);

    glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    app->screen.programIdx = LoadProgram(app, "shaders.glsl", "TEXTURE_TO_SCREEN");
    Program& textureToScreenProgram = app->programs[app->screen.programIdx];
    textureToScreenProgram.uTexture = glGetUniformLocation(textureToScreenProgram.handle, "uTexture");
    
    // Create Uniform Buffer
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);
    
    app->uniform = CreateConstantBuffer(app->maxUniformBufferSize);
    
    // Frame buffer
    // Color
    CreateColorAttachment(app->colorAttachmentHandle, app->displaySize);
    // Albedo
    CreateColorAttachment(app->albedoAttachmentHandle, app->displaySize);
    // Normals
    CreateColorAttachment(app->normalsAttachmentHandle, app->displaySize);
    // Positions
    CreateColorAttachment(app->positionsAttachmentHandle, app->displaySize);
    // Depth
    CreateColorAttachment(app->depthAttachmentHandle, app->displaySize);
    glGenTextures(1, &app->depthHandle);
    glBindTexture(GL_TEXTURE_2D, app->depthHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, app->displaySize.x, app->displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    app->frameBufferHandle;
    glGenFramebuffers(1, &app->frameBufferHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, app->frameBufferHandle);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, app->colorAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, app->albedoAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, app->normalsAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, app->positionsAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, app->depthAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, app->depthHandle, 0);
    
    GLenum frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (frameBufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        switch (frameBufferStatus)
        {
        case GL_FRAMEBUFFER_UNDEFINED:                     ELOG("GL_FRAMEBUFFER_UNDEFINED"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         ELOG("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: ELOG("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        ELOG("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        ELOG("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"); break;
        case GL_FRAMEBUFFER_UNSUPPORTED:                   ELOG("GL_FRAMEBUFFER_UNSUPPORTED"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:        ELOG("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"); break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:      ELOG("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"); break;
        default: ELOG("Unknown frame buffer status error!");
        }
    }
    
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
    glDrawBuffers(ARRAY_COUNT(drawBuffers), drawBuffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Depth test
    glEnable(GL_DEPTH_TEST);
}

void Gui(App* app)
{
    ImGui::Begin("Info");

    ImGui::BulletText("OpenGL version: %s", app->openGlVersion);
    ImGui::BulletText("OpenGL renderer: %s", app->gpuName);
    ImGui::BulletText("OpenGL vendor: %s", app->openGlVendor);
    ImGui::BulletText("OpenGL GLSL version: %s", app->GLSLVersion);
    if (ImGui::TreeNode("OpenGL extensions"))
    {
        for (int i = 0; i < app->openGLExtensions.size(); ++i)
            ImGui::Text("%s", app->openGLExtensions[i]);
        ImGui::TreePop();
    }
    ImGui::BulletText("FPS: %f", 1.0f / app->deltaTime);

    if (ImGui::Button("COLOR"))
        app->mode = Mode::COLOR;
    ImGui::SameLine();
    if (ImGui::Button("ALBEDO"))
        app->mode = Mode::ALBEDO;
    ImGui::SameLine();
    if (ImGui::Button("NORMALS"))
        app->mode = Mode::NORMALS;
    ImGui::SameLine();
    if (ImGui::Button("POSITIONS"))
        app->mode = Mode::POSITIONS;
    ImGui::SameLine();
    if (ImGui::Button("DEPTH"))
        app->mode = Mode::DEPTH;

    switch (app->mode)
    {
    case Mode::COLOR:
        app->currentAttachmentHandle = app->colorAttachmentHandle;
        break;
    case Mode::ALBEDO:
        app->currentAttachmentHandle = app->albedoAttachmentHandle;
        break;
    case Mode::NORMALS:
        app->currentAttachmentHandle = app->normalsAttachmentHandle;
        break;
    case Mode::POSITIONS:
        app->currentAttachmentHandle = app->positionsAttachmentHandle;
        break;
    case Mode::DEPTH:
        app->currentAttachmentHandle = app->depthAttachmentHandle;
        break;
    }

    ImGui::End();
}

void Update(App* app)
{
    //Rotate Camera
    u32 milliseconds = app->timeRunning * 1000;
    u32 spinTime = 10 * 1000;
    float alpha = 2.0f * PI * ((float)(milliseconds % spinTime) / spinTime);
    app->cameraPosition = 25.0f * glm::vec3(cos(alpha), 0, sin(alpha));

    app->cameraDirection = glm::normalize(glm::vec3(0) - app->cameraPosition);
    app->view = glm::lookAt(app->cameraPosition, app->cameraPosition + glm::normalize(app->cameraDirection), glm::vec3(0, 1, 0));

    // Set Uniform Buffer data
    MapBuffer(app->uniform, GL_WRITE_ONLY);
    
    // Set Global Parameters at the start
    PushVec3(app->uniform, app->cameraPosition);
    PushUInt(app->uniform, app->lights.size());
    
    for (u32 i = 0; i < app->lights.size(); ++i)
    {
        AlignHead(app->uniform, sizeof(glm::vec4));
    
        Light& light = app->lights[i];
        PushUInt(app->uniform, light.type);
        PushVec3(app->uniform, light.color);
        PushVec3(app->uniform, light.direction);
        PushVec3(app->uniform, light.position);
    }
    
    app->globalsSize = app->uniform.head;
    
    // Set Entities data
    for (u32 i = 0; i < app->entities.size(); ++i)
    {
        AlignHead(app->uniform, app->uniformBlockAlignment);
    
        Entity& entity = app->entities[i];
    
        entity.uniformOffset = app->uniform.head;
        PushMat4(app->uniform, entity.transform);
        PushMat4(app->uniform, app->projection * app->view * entity.transform);
        entity.uniformSize = app->uniform.head - app->entities[i].uniformOffset;
    }
    
    UnmapBuffer(app->uniform);
}

void Render(App* app)
{
    // Geometry Pass
    glBindFramebuffer(GL_FRAMEBUFFER, app->frameBufferHandle);
    
    // Set up render
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);
    
    // Pass global parameters data to shader
    glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->uniform.handle, 0, app->globalsSize);
    
    // Loop through entites and render
    for (u32 e = 0; e < app->entities.size(); ++e)
    {
        Entity& entity = app->entities[e];
    
        Model& model = app->models[entity.modelIdx];
    
        Program& program = app->programs[model.programIndex];
        glUseProgram(program.handle);
    
        Mesh& mesh = app->meshes[model.meshIdx];
    
        // Pass local parameters data to shader
        glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->uniform.handle, entity.uniformOffset, entity.uniformSize);
    
        for (u32 i = 0; i < mesh.submeshes.size(); ++i)
        {
            GLuint vao = FindVAO(mesh, i, program);
            glBindVertexArray(vao);
    
            u32 submeshMaterialIdx = model.materialIdx[i];
            Material& submeshMaterial = app->materials[submeshMaterialIdx];
    
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);
            glUniform1i(program.uTexture, 0);
    
            Submesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Lighting Pass
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);
    
    Program& textureToScreenProgram = app->programs[app->screen.programIdx];
    glUseProgram(textureToScreenProgram.handle);
    glBindVertexArray(app->screen.vao);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUniform1i(textureToScreenProgram.uTexture, 0);
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, app->currentAttachmentHandle);
    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    
    glBindVertexArray(0);
    glUseProgram(0);
}
