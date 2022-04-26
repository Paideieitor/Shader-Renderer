//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include "loader.h"

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

void Init(App* app)
{
    app->mode = Mode::TexturedMesh;

    app->aspectRatio = (float)app->displaySize.x / (float)app->displaySize.y;
    app->znear = 0.1f;
    app->zfar = 1000.0f;
    app->projection = glm::perspective(glm::radians(60.0f), app->aspectRatio, app->znear, app->zfar);
    app->view = glm::lookAt(glm::vec3(0,0,2), glm::vec3(0,0,0), glm::vec3(0,1,0));

    app->world = Transform(glm::vec3(2.5f, 1.5f, -2.0f), glm::vec3(0.45f)).GetTransform();
    app->worldViewProjection = app->projection * app->view * app->world;

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
     GLuint programIdx = LoadProgram(app, "shaders.glsl", "SHOW_TEXTURED_MESH");
     Program& texturedMeshProgram = app->programs[programIdx];

     LoadModel(app, "Patrick/Patrick.obj", programIdx);

     glEnable(GL_DEPTH_TEST);

     // Get Uniform Buffer data
     glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
     glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAlignment);

     // Create Uniform Buffers
     glGenBuffers(1, &app->bufferHandle);
     glBindBuffer(GL_UNIFORM_BUFFER, app->bufferHandle);
     glBufferData(GL_UNIFORM_BUFFER, app->maxUniformBufferSize, NULL, GL_STREAM_DRAW);
     glBindBuffer(GL_UNIFORM_BUFFER, 0);
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
    ImGui::BulletText("FPS: %f", 1.0f/app->deltaTime);

    ImGui::End();
}

void Update(App* app)
{
    // You can handle app->input keyboard/mouse here
}

void Render(App* app)
{
    // Set up render
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, app->displaySize.x, app->displaySize.y);

    // Set Buffer handle
    glBindBuffer(GL_UNIFORM_BUFFER, app->bufferHandle);
    u8* bufferData = (u8*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
    u32 bufferHead = 0;

    memcpy(bufferData + bufferHead, glm::value_ptr(app->world), sizeof(glm::mat4));
    bufferHead += sizeof(glm::mat4);

    memcpy(bufferData + bufferHead, glm::value_ptr(app->worldViewProjection), sizeof(glm::mat4));
    bufferHead += sizeof(glm::mat4);

    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Loop through meshes and render
    for (u32 m = 0; m < app->models.size(); ++m)
    {
        Model& model = app->models[m];

        Program& program = app->programs[model.programIndex];
        glUseProgram(program.handle);

        Mesh& mesh = app->meshes[model.meshIdx];

        // Pass world position data to shader
        //bind biffer range TODO

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
}
