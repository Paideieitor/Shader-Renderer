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

u32 BuildScreen(App* app)
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
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/-1, -1, 0, /*TC*/0, 0 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/ 1, -1, 0, /*TC*/1, 0 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/ 1,  1, 0, /*TC*/1, 1 });
    submesh.vertices.insert(submesh.vertices.end(), { /*V*/-1,  1, 0, /*TC*/0, 1 });

    submesh.indexOffset = 0;
    submesh.indices.insert(submesh.indices.end(), { 0, 1, 2, 0, 2, 3 });

    // Save attributes to be read
    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 0, 3, 0 });
    submesh.vertexBufferLayout.stride = 3 * sizeof(float);

    submesh.vertexBufferLayout.attributes.push_back(VertexBufferAttribute{ 1, 2, submesh.vertexBufferLayout.stride });
    submesh.vertexBufferLayout.stride += 2 * sizeof(float);

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

    const void* verticesData = mesh.submeshes[0].vertices.data();
    const u32   verticesSize = mesh.submeshes[0].vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    mesh.submeshes[0].vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = mesh.submeshes[0].indices.data();
    const u32   indicesSize = mesh.submeshes[0].indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    mesh.submeshes[0].indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return modelIdx;
}

void InsertVectexData(std::vector<float>& vertices, const glm::vec3& pos, const glm::vec3& normal, const glm::vec2& texCoords, const glm::vec3& tangent, const glm::vec3& bitangent)
{
    vertices.insert(vertices.end(), { /*V*/pos.x, pos.y, pos.z, /*N*/normal.x, normal.y, normal.z, /*TC*/texCoords.x, texCoords.y, /*T*/tangent.x, tangent.y, tangent.z, /*B*/bitangent.x, bitangent.y, bitangent.z });
}

void GetTangentSpace(glm::vec3& tangent, glm::vec3& bitangent, const glm::vec3& pos1, const glm::vec3& pos2, const glm::vec3& pos3, const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3)
{
    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;

    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

    bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
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

    // positions
    glm::vec3 pos1(-1.0, 0.0, 1.0);
    glm::vec3 pos2(-1.0, 0.0, -1.0);
    glm::vec3 pos3(1.0, 0.0, -1.0);
    glm::vec3 pos4(1.0, 0.0, 1.0);
    // texture coordinates
    glm::vec2 uv1(0.0, 1.0);
    glm::vec2 uv2(0.0, 0.0);
    glm::vec2 uv3(1.0, 0.0);
    glm::vec2 uv4(1.0, 1.0);
    // normal vector
    glm::vec3 nm(0.0, 1.0, 0.0);

    // tangent space
    glm::vec3 tangent1;
    glm::vec3 bitangent1;
    GetTangentSpace(tangent1, bitangent1, pos1, pos2, pos3, uv1, uv2, uv3);

    glm::vec3 tangent2;
    glm::vec3 bitangent2;
    GetTangentSpace(tangent2, bitangent2, pos1, pos3, pos4, uv1, uv3, uv4);

    submesh.vertexOffset = 0;
    InsertVectexData(submesh.vertices, pos1, nm, uv1, tangent1, bitangent1);
    InsertVectexData(submesh.vertices, pos2, nm, uv2, tangent1, bitangent1);
    InsertVectexData(submesh.vertices, pos3, nm, uv3, tangent1, bitangent1);

    InsertVectexData(submesh.vertices, pos1, nm, uv1, tangent2, bitangent2);
    InsertVectexData(submesh.vertices, pos3, nm, uv3, tangent2, bitangent2);
    InsertVectexData(submesh.vertices, pos4, nm, uv4, tangent2, bitangent2);

    submesh.indexOffset = 0;
    submesh.indices.insert(submesh.indices.end(), { 0, 2, 1, 3, 5, 4 });

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

    const void* verticesData = mesh.submeshes[0].vertices.data();
    const u32   verticesSize = mesh.submeshes[0].vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    mesh.submeshes[0].vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = mesh.submeshes[0].indices.data();
    const u32   indicesSize = mesh.submeshes[0].indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    mesh.submeshes[0].indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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

    const u32 H = 64;
    const u32 V = 32;
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

    const void* verticesData = mesh.submeshes[0].vertices.data();
    const u32   verticesSize = mesh.submeshes[0].vertices.size() * sizeof(float);
    glBufferSubData(GL_ARRAY_BUFFER, verticesOffset, verticesSize, verticesData);
    mesh.submeshes[0].vertexOffset = verticesOffset;
    verticesOffset += verticesSize;

    const void* indicesData = mesh.submeshes[0].indices.data();
    const u32   indicesSize = mesh.submeshes[0].indices.size() * sizeof(u32);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, indicesOffset, indicesSize, indicesData);
    mesh.submeshes[0].indexOffset = indicesOffset;
    indicesOffset += indicesSize;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return modelIdx;
}

void BuildPrimitives(App* app)
{
    app->screenIdx = BuildScreen(app);
    app->planeIdx = BuildPlane(app);
    app->sphereIdx = BuildSphere(app);
}

u32 CreateEntity(App* const app, u32 modelIdx, u32 programIdx, const glm::vec3& position, const glm::vec3& scaleFactor, const glm::vec3& rotation)
{
    app->entities.push_back(Entity());
    Entity& entity = app->entities.back();

    entity.modelIdx = modelIdx;
    entity.programIdx = programIdx;

    entity.transform = Rotate(Scale(Translate(IDENTITY4, position), scaleFactor), (rotation / 360.0f) * 2.0f * PI);
    entity.position = position;
    entity.scale = scaleFactor;
    entity.rotation = rotation;

    return app->entities.size() - 1u;
}

u32 CreateLight(App* const app, const Light::Type& type, const glm::vec3& color, const glm::vec3& direction, const glm::vec3& position, f32 range)
{
    app->lights.push_back(Light());
    Light& light = app->lights.back();

    light.type = type;
    switch (light.type)
    {
    case Light::Type::DIRECTIONAL:
        light.programIdx = app->directionalProgramIdx;
        break;
    case Light::Type::POINT:
        light.programIdx = app->pointProgramIdx;
        break;
    }
    light.color = color;
    light.direction = direction;
    light.center = position;
    light.range = range;

    light.transform = Scale(Translate(IDENTITY4, position), glm::vec3(range));
    
    return app->lights.size() - 1u;
}

void CreateColorAttachment(GLuint& handle, const glm::ivec2& displaySize)
{
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, displaySize.x, displaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SetLightProgramTextureLocations(App* app, u32 programIdx)
{
    Program& program = app->programs[programIdx];

    program.albedoLocation = glGetUniformLocation(program.handle, "uAlbedo");
    program.normalsLocation = glGetUniformLocation(program.handle, "uNormals");
    program.positionLocation = glGetUniformLocation(program.handle, "uPosition");
    program.depthLocation = glGetUniformLocation(program.handle, "uDepth");
}

void Init(App* app)
{
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
        GLubyte* extension = new GLubyte[64];
        memcpy(extension, glGetStringi(GL_EXTENSIONS, GLuint(i)), 64);
        app->openGLExtensions.push_back(extension);
    }

    // Fill vertex input layout with required attributes
    app->texturedMeshProgramIdx = LoadProgram(app, "Assets/Shaders/shaders.glsl", "TEXTURED_MESH");
    Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];

    texturedMeshProgram.albedoLocation = glGetUniformLocation(texturedMeshProgram.handle, "uAlbedo");
    texturedMeshProgram.normalsLocation = glGetUniformLocation(texturedMeshProgram.handle, "uNormal");
    texturedMeshProgram.depthLocation = glGetUniformLocation(texturedMeshProgram.handle, "uRelief");
    
    // Create entities
    app->defaultTextureIdx = LoadTexture2D(app, "Assets/Textures/color_white.png");

    BuildPrimitives(app);
    
    //app->patrickIdx = LoadModel(app, "Assets/Models/Patrick/Patrick.obj");
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(0, 0, 0), glm::vec3(0.5f), glm::vec3(0, 0, 0));
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(0, 0, -20), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(5, 0, 3), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(-5, 0, 3), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(10, 0, 6), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    //CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(-10, 0, 6), glm::vec3(1, 1, 1), glm::vec3(0, 0, 0));
    //
    //CreateEntity(app, app->sphereIdx, app->texturedMeshProgramIdx, glm::vec3(0, 3.2, 0), glm::vec3(1.4f), glm::vec3(220, 234, 43));

    // Normals Stuff

    //app->materials.emplace_back(Material());
    //Material& material = app->materials.back();
    //material.albedoTextureIdx = LoadTexture2D(app, "Assets/Textures/brickwall.jpg");
    //material.normalsTextureIdx = LoadTexture2D(app, "Assets/Textures/brickwall_normal.jpg");
    //
    //u32 brickwallIdx = CreateEntity(app, app->planeIdx, app->texturedMeshProgramIdx, glm::vec3(0, -3.4, 0), glm::vec3(20), glm::vec3(0, 0, 0));
    //app->models[app->entities[brickwallIdx].modelIdx].materialIdx.emplace_back(app->materials.size() - 1u);

    // Relief Stuff

    app->materials.emplace_back(Material());
    Material& material = app->materials.back();
    material.albedoTextureIdx = LoadTexture2D(app, "Assets/Textures/diffuse.png");
    material.normalsTextureIdx = LoadTexture2D(app, "Assets/Textures/normal.png");
    material.bumpTextureIdx = LoadTexture2D(app, "Assets/Textures/displacement.png");
    
    u32 reliefwallIdx = CreateEntity(app, app->planeIdx, app->texturedMeshProgramIdx, glm::vec3(0, 0, 0), glm::vec3(2), glm::vec3(90, 0, 0));
    app->models[app->entities[reliefwallIdx].modelIdx].materialIdx.emplace_back(app->materials.size() - 1u);
    
    // Deferred Shading
    app->directionalProgramIdx = LoadProgram(app, "Assets/Shaders/shaders.glsl", "DIRECTIONAL_LIGHT");
    SetLightProgramTextureLocations(app, app->directionalProgramIdx);
    app->pointProgramIdx = LoadProgram(app, "Assets/Shaders/shaders.glsl", "POINT_LIGHT");
    SetLightProgramTextureLocations(app, app->pointProgramIdx);

    // Create lights
    CreateLight(app, Light::Type::DIRECTIONAL, glm::vec3(0.3f, 0.3f, 0.3f), glm::vec3(1, 1, 1), glm::vec3(1, 1, 1), 0);
    CreateLight(app, Light::Type::DIRECTIONAL, glm::vec3(0, 0, 0.5f), glm::vec3(-1, 1, 1), glm::vec3(1, 1, 1), 0);
    
    srand(0);
    for (i32 i = 0; i < LIGHT_AMOUNT; ++i)
    {
        i32 x = (rand() % 50) - 25;
        i32 y = 0;
        i32 z = (rand() % 50) - 25;
        f32 g = (f32)(rand() % 100) / 100.0f;
        f32 b = (f32)(rand() % 100) / 100.0f;
        f32 r = (f32)(rand() % 100) / 100.0f;
        i32 s = (rand() % 5) + 5;
    
        CreateLight(app, Light::Type::POINT, glm::vec3(r, g, b), glm::vec3(0), glm::vec3(x, y, z), (f32)s);
    }

    // Screen Shader
    app->toScreenProgramIdx = LoadProgram(app, "Assets/Shaders/shaders.glsl", "TO_SCREEN");
    Program& toScreenProgram = app->programs[app->toScreenProgramIdx];
    toScreenProgram.albedoLocation = glGetUniformLocation(toScreenProgram.handle, "uColor");
    
    // Create Uniform Buffer
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);
    
    app->uniform = CreateConstantBuffer(app->maxUniformBufferSize);
    
    // Frame buffer
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
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, app->albedoAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, app->normalsAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, app->positionsAttachmentHandle, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, app->depthAttachmentHandle, 0);
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
    
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(ARRAY_COUNT(drawBuffers), drawBuffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Depth test
    glEnable(GL_BLEND);
}

void Gui(App* app)
{
    ImGui::Begin("Inspector");

    if (ImGui::CollapsingHeader("Info"))
    {
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
        ImGui::Separator();
    }

    ImGui::BulletText("FPS: %f", 1.0f / app->deltaTime);

    ImGui::Text("Display Mode:");
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
    ImGui::Checkbox("Use Normal Mapping", &app->useNormalMap);
    ImGui::Checkbox("Use Relief Mapping", &app->useReliefMap);
    ImGui::Separator();

    ImGui::Checkbox("Moving Lights", &app->movingLights);
    ImGui::Text("Camera:");
    ImGui::Checkbox("Free Camera", &app->freeCam);

    if (!app->freeCam)
    {
        ImGui::SliderAngle("Rotation##camera", &app->alpha);
        ImGui::SliderFloat("Distance##camera", &app->camDist, 1.0f, 100.0f);
        ImGui::SliderFloat("Height##camera", &app->camHeight, -50.0f, 50.0f);
    }
    else
    {
        ImGui::DragFloat3("Position##camera", (float*)&app->cameraPosition);
        ImGui::DragFloat2("Rotation##camera", (float*)&app->cameraRotation);
        ImGui::SliderFloat("Speed##camera", &app->camSpeed, 0.1f, 500.0f);
        ImGui::SliderFloat("Turn Speed##camera", &app->camTurnSpeed, 0.1f, 500.0f);
    }

    switch (app->mode)
    {
    case Mode::COLOR:
        app->currentAttachmentHandle = app->albedoAttachmentHandle;
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

    ImGui::Separator();
    if (app->selectedEntity + app->selectedLight > -2)
        if (ImGui::CollapsingHeader("Selected", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Delete"))
            {
                if (app->selectedEntity > -1)
                    app->entities.erase(app->entities.begin() + app->selectedEntity);
                else
                    app->lights.erase(app->lights.begin() + app->selectedLight);

                app->selectedEntity = -1;
                app->selectedLight = -1;
            }
            else
            {
                char name[64];
                if (app->selectedEntity > -1)
                {
                    sprintf(name, "Entity %d", app->selectedEntity);
                    ImGui::Text(name);

                    Entity& entity = app->entities[app->selectedEntity];

                    const char* items[] = { "PLANE", "SPHERE", "PATRICK" };
                    u32 currentModel = -1;
                    if (entity.modelIdx == app->planeIdx)
                        currentModel = 0;
                    else if (entity.modelIdx == app->sphereIdx)
                        currentModel = 1;
                    else if (entity.modelIdx == app->patrickIdx)
                        currentModel = 2;

                    if (ImGui::BeginCombo("Model", items[currentModel]))
                    {
                        for (u32 i = 0; i < 3u; ++i)
                        {
                            const bool is_selected = (currentModel == i);
                            if (ImGui::Selectable(items[i], is_selected))
                            {
                                if (std::string(items[i]) == "PLANE")
                                    entity.modelIdx = app->planeIdx;
                                else if (std::string(items[i]) == "SPHERE")
                                    entity.modelIdx = app->sphereIdx;
                                else if (std::string(items[i]) == "PATRICK")
                                    entity.modelIdx = app->patrickIdx;
                            }

                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (ImGui::DragFloat3("Position", (float*)&entity.position))
                        entity.transform = Rotate(Scale(Translate(IDENTITY4, entity.position), entity.scale), (entity.rotation / 360.0f) * 2.0f * PI);
                    if (ImGui::DragFloat3("Scale", (float*)&entity.scale))
                        entity.transform = Rotate(Scale(Translate(IDENTITY4, entity.position), entity.scale), (entity.rotation / 360.0f) * 2.0f * PI);
                    if (ImGui::DragFloat3("Rotation", (float*)&entity.rotation))
                        entity.transform = Rotate(Scale(Translate(IDENTITY4, entity.position), entity.scale), (entity.rotation / 360.0f) * 2.0f * PI);
                }
                else
                {
                    Light& light = app->lights[app->selectedLight];
                    switch (light.type)
                    {
                    case Light::Type::DIRECTIONAL:
                    {
                        const char* type = "Directional";
                        sprintf(name, "%s Light %d", type, app->selectedLight);
                    }
                    break;
                    case Light::Type::POINT:
                    {
                        const char* type = "Point";
                        sprintf(name, "%s Light %d", type, app->selectedLight);
                    }
                    break;
                    }
                    ImGui::Text(name);

                    switch (light.type)
                    {
                    case Light::Type::DIRECTIONAL:
                    {
                        ImGui::DragFloat3("Direction", (float*)&light.direction);
                    }
                    break;
                    case Light::Type::POINT:
                    {
                        if (!app->movingLights)
                            if (ImGui::DragFloat3("Center##point", (float*)&light.center))
                                light.transform = Scale(Translate(IDENTITY4, light.center), glm::vec3(light.range));
                        if (ImGui::DragFloat("Range", &light.range))
                            light.transform = Scale(Translate(IDENTITY4, light.center), glm::vec3(light.range));
                    }
                    break;
                    }

                    ImGui::Separator();
                    ImGui::ColorPicker3("Color", (float*)(&light.color), ImGuiColorEditFlags_Float);
                }
            }
            ImGui::Separator();
        }

    if (ImGui::CollapsingHeader("Danger Zone"))
    {
        if (ImGui::Button("Delete All Entities"))
        {
            app->entities.clear();
            app->selectedEntity = -1;
        }

        if (ImGui::Button("Delete All Lights"))
        {
            app->lights.clear();
            app->selectedLight = -1;
        }
    }

    ImGui::End();

    ImGui::Begin("Scene");

    if (ImGui::Button("Create Entity"))
        CreateEntity(app, app->patrickIdx, app->texturedMeshProgramIdx, glm::vec3(0), glm::vec3(1), glm::vec3(0));
    if (ImGui::Button("Create Directional Light"))
        CreateLight(app, Light::Type::DIRECTIONAL, glm::vec3(1), glm::vec3(1), glm::vec3(0), 0);
    ImGui::SameLine();
    if (ImGui::Button("Create Point Light"))
        CreateLight(app, Light::Type::POINT, glm::vec3(1), glm::vec3(0), glm::vec3(0), 10);

    if (ImGui::TreeNode("Entities"))
    {
        for (u32 i = 0u; i < app->entities.size(); ++i)
        {
            char name[64];
            sprintf(name, "Entity %d", i);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
            if (app->selectedEntity == i)
                flags |= ImGuiTreeNodeFlags_Selected;
            if (ImGui::TreeNodeEx(name, flags))
                if (ImGui::IsItemClicked())
                {
                    app->selectedEntity = i;
                    app->selectedLight = -1;
                }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Lights"))
    {
        for (u32 i = 0u; i < app->lights.size(); ++i)
        {
            char name[64];
            switch (app->lights[i].type)
            {
            case Light::Type::DIRECTIONAL:
            {
                const char* type = "Directional";
                sprintf(name, "%s %d", type, i);
            }
                break;
            case Light::Type::POINT:
            {
                const char* type = "Point";
                sprintf(name, "%s %d", type, i);
            }
                break;
            }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
            if (app->selectedLight == i)
                flags |= ImGuiTreeNodeFlags_Selected;
            if (ImGui::TreeNodeEx(name, flags))
                if (ImGui::IsItemClicked())
                {
                    app->selectedEntity = -1;
                    app->selectedLight = i;
                }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

void Update(App* app)
{
    //Camera

    if (!app->freeCam)
    {
        app->cameraPosition = app->camDist * glm::vec3(cos(app->alpha), app->camHeight, sin(app->alpha));

        app->cameraDirection = glm::vec3(0) - app->cameraPosition;
    }
    else
    {
        glm::vec2 r = (app->cameraRotation / 360.0f) * 2.0f * PI;
        app->cameraDirection = glm::normalize(glm::vec3(Rotate(IDENTITY4, glm::vec3(0, -r.x, r.y)) * glm::vec4(glm::vec3(1, 0, 0), 0.0)));

        if (app->input.keys[K_W] == BUTTON_PRESSED)
        {
            if (app->input.keys[K_SPACE] == BUTTON_PRESSED)
            {
                app->cameraRotation.y += app->camTurnSpeed * app->deltaTime;

                glm::vec2 r = (app->cameraRotation / 360.0f) * 2.0f * PI;
                app->cameraDirection = glm::normalize(glm::vec3(Rotate(IDENTITY4, glm::vec3(0, -r.x, r.y)) * glm::vec4(glm::vec3(1, 0, 0), 0.0)));
            }
            else
                app->cameraPosition += app->cameraDirection * app->camSpeed * app->deltaTime;
        }
        if (app->input.keys[K_S] == BUTTON_PRESSED)
        {
            if (app->input.keys[K_SPACE] == BUTTON_PRESSED)
            {
                app->cameraRotation.y -= app->camTurnSpeed * app->deltaTime;

                glm::vec2 r = (app->cameraRotation / 360.0f) * 2.0f * PI;
                app->cameraDirection = glm::normalize(glm::vec3(Rotate(IDENTITY4, glm::vec3(0, -r.x, r.y)) * glm::vec4(glm::vec3(1, 0, 0), 0.0)));
            }
            else
                app->cameraPosition -= app->cameraDirection * app->camSpeed * app->deltaTime;
        }
        if (app->input.keys[K_A] == BUTTON_PRESSED)
        {
            app->cameraRotation.x -= app->camTurnSpeed * app->deltaTime;

            glm::vec2 r = (app->cameraRotation / 360.0f) * 2.0f * PI;
            app->cameraDirection = glm::normalize(glm::vec3(Rotate(IDENTITY4, glm::vec3(0, -r.x, r.y)) * glm::vec4(glm::vec3(1, 0, 0), 0.0)));
        }
        if (app->input.keys[K_D] == BUTTON_PRESSED)
        {
            app->cameraRotation.x += app->camTurnSpeed * app->deltaTime;

            glm::vec2 r = (app->cameraRotation / 360.0f) * 2.0f * PI;
            app->cameraDirection = glm::normalize(glm::vec3(Rotate(IDENTITY4, glm::vec3(0, -r.x, r.y)) * glm::vec4(glm::vec3(1, 0, 0), 0.0)));
        }
    }
    app->view = glm::lookAt(app->cameraPosition, app->cameraPosition + glm::normalize(app->cameraDirection), glm::vec3(0, 1, 0));

    if (app->movingLights)
        for (u32 i = 0u; i < app->lights.size(); ++i)
        {
            srand(i);
            Light& light = app->lights[i];
            if (light.type == Light::Type::DIRECTIONAL)
                continue;
            i32 direction =  (rand() % 3) - 1;

            if (direction != 0)
            {
                u32 spinTime = (rand() % 50000) + 9000;
                f32 distance = glm::length(light.center);

                u32 milliseconds = (u32)(app->timeRunning * 1000.0f) + (rand() % 1000);
                float alpha = 2.0f * PI * ((float)(milliseconds % spinTime) / spinTime) * direction;

                app->lights[i].center = distance * glm::vec3(cos(alpha), light.center.y, sin(alpha));
                app->lights[i].transform = Scale(Translate(IDENTITY4, app->lights[i].center), glm::vec3(app->lights[i].range));
            }
        }

    // Set Uniform Buffer data
    MapBuffer(app->uniform, GL_WRITE_ONLY);
    
    // Set Global Parameters at the start
    PushVec3(app->uniform, app->cameraPosition);
    PushVec3(app->uniform, glm::vec3(app->displaySize.x, app->displaySize.y, app->aspectRatio));
    PushFloat(app->uniform, app->znear);
    PushFloat(app->uniform, app->zfar);
    
    app->globalsSize = app->uniform.head;
    
    // Set Entities data
    for (u32 i = 0; i < app->entities.size(); ++i)
    {
        AlignHead(app->uniform, app->uniformBlockAlignment);
    
        Entity& entity = app->entities[i];
    
        entity.uniformOffset = app->uniform.head;
        PushMat4(app->uniform, entity.transform);
        PushMat4(app->uniform, app->projection * app->view * entity.transform);

        u32 hasNormalMapping = 0u;
        if (app->useNormalMap)
            for (u32 m = 0u; m < app->models[entity.modelIdx].materialIdx.size(); ++m)
                if (app->materials[app->models[entity.modelIdx].materialIdx[m]].normalsTextureIdx > 0)
                {
                    hasNormalMapping = 1u;
                    break;
                }
        PushUInt(app->uniform, hasNormalMapping);

        u32 hasReliefMapping = 0u;
        if (app->useReliefMap)
            for (u32 m = 0u; m < app->models[entity.modelIdx].materialIdx.size(); ++m)
                if (app->materials[app->models[entity.modelIdx].materialIdx[m]].bumpTextureIdx > 0)
                {
                    hasReliefMapping = 1u;
                    break;
                }
        PushUInt(app->uniform, hasReliefMapping);
        entity.uniformSize = app->uniform.head - entity.uniformOffset;
    }

    // Set Lights data
    for (u32 i = 0; i < app->lights.size(); ++i)
    {
        AlignHead(app->uniform, app->uniformBlockAlignment);

        Light& light = app->lights[i];

        light.uniformOffset = app->uniform.head;
        PushVec3(app->uniform, light.color);
        
        switch (light.type)
        {
        case Light::Type::DIRECTIONAL:
            PushVec3(app->uniform, glm::normalize(light.direction));
            break;
        case Light::Type::POINT:
            PushVec3(app->uniform, light.center);
            PushFloat(app->uniform, light.range);
            PushMat4(app->uniform, light.transform);
            PushMat4(app->uniform, app->projection * app->view * light.transform);
            break;
        }

        light.uniformSize = app->uniform.head - light.uniformOffset;
    }
    //ELOG("Max: %d , Head: %d", app->maxUniformBufferSize, app->uniform.head);
    UnmapBuffer(app->uniform);
}

void Render(App* app)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Pass global parameters data to shader
    glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->uniform.handle, 0, app->globalsSize);

    // Geometry Pass
    glBindFramebuffer(GL_FRAMEBUFFER, app->frameBufferHandle);
    
    // Set up render
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Loop through entites and render
    for (u32 e = 0; e < app->entities.size(); ++e)
    {
        Entity& entity = app->entities[e];
    
        Model& model = app->models[entity.modelIdx];
    
        Program& program = app->programs[entity.programIdx];
        glUseProgram(program.handle);

        glUniform1i(program.albedoLocation, 0);
        glUniform1i(program.normalsLocation, 1);
        glUniform1i(program.depthLocation, 2);
    
        Mesh& mesh = app->meshes[model.meshIdx];
    
        // Pass local parameters data to shader
        glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->uniform.handle, entity.uniformOffset, entity.uniformSize);
    
        for (u32 i = 0; i < mesh.submeshes.size(); ++i)
        {
            GLuint vao = FindVAO(mesh, i, program);
            glBindVertexArray(vao);
    
            GLuint albedoHandle = app->textures[app->defaultTextureIdx].handle;
            GLuint normalHandle = 0u;
            GLuint reliefHandle = 0u;
            if (model.materialIdx.size() > 0u)
            {
                u32 submeshMaterialIdx = model.materialIdx[i];
                Material& submeshMaterial = app->materials[submeshMaterialIdx];
                albedoHandle = app->textures[submeshMaterial.albedoTextureIdx].handle;
                normalHandle = app->textures[submeshMaterial.normalsTextureIdx].handle;
                reliefHandle = app->textures[submeshMaterial.bumpTextureIdx].handle;
            }
    
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, albedoHandle);
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, normalHandle);
            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, reliefHandle);

            Submesh& submesh = mesh.submeshes[i];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Lighting Pass
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    if (app->mode == Mode::COLOR)
        for (u32 i = 0; i < app->lights.size(); ++i)
        {
            Light& light = app->lights[i];

            Program& program = app->programs[light.programIdx];
            glUseProgram(program.handle);

            glUniform1i(program.albedoLocation, 0);
            glUniform1i(program.normalsLocation, 1);
            glUniform1i(program.positionLocation, 2);
            glUniform1i(program.depthLocation, 3);

            glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), app->uniform.handle, light.uniformOffset, light.uniformSize);

            u32 modelIdx = 0;
            switch (light.type)
            {
            case Light::Type::DIRECTIONAL:
                modelIdx = app->screenIdx;
                break;
            case Light::Type::POINT:
                modelIdx = app->sphereIdx;
                break;
            }

            Mesh& mesh = app->meshes[app->models[modelIdx].meshIdx];
            GLuint vao = FindVAO(mesh, 0, program);
            glBindVertexArray(vao);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, app->albedoAttachmentHandle);
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, app->normalsAttachmentHandle);
            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, app->positionsAttachmentHandle);
            glActiveTexture(GL_TEXTURE0 + 3);
            glBindTexture(GL_TEXTURE_2D, app->depthAttachmentHandle);

            Submesh& submesh = mesh.submeshes[0];
            glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);

            glBindVertexArray(0);
            glUseProgram(0);
        }
    else
    {
        Program& program = app->programs[app->toScreenProgramIdx];
        glUseProgram(program.handle);

        glUniform1i(program.albedoLocation, 0);

        Mesh& mesh = app->meshes[app->models[app->screenIdx].meshIdx];
        GLuint vao = FindVAO(mesh, 0, program);
        glBindVertexArray(vao);

        glActiveTexture(GL_TEXTURE0);
        switch (app->mode)
        {
        case Mode::ALBEDO:
            glBindTexture(GL_TEXTURE_2D, app->albedoAttachmentHandle);
            break;
        case Mode::NORMALS:
            glBindTexture(GL_TEXTURE_2D, app->normalsAttachmentHandle);
            break;
        case Mode::POSITIONS:
            glBindTexture(GL_TEXTURE_2D, app->positionsAttachmentHandle);
            break;
        case Mode::DEPTH:
            glBindTexture(GL_TEXTURE_2D, app->depthAttachmentHandle);
            break;
        }

        Submesh& submesh = mesh.submeshes[0];
        glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);

        glBindVertexArray(0);
        glUseProgram(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}
