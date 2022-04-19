//
// engine.cpp : Put all your graphics stuff in this file. This is kind of the graphics module.
// In here, you should type all your OpenGL commands, and you can also type code to handle
// input platform events (e.g to move the camera or react to certain shortcuts), writing some
// graphics related GUI options, and so on.
//

#include "engine.h"
#include "loader.h"

#include <imgui.h>

const VertexV3V2 vertices[] = {
    {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0, 0.0)},
    {glm::vec3(0.5f, -0.5f, 0.0f), glm::vec2(1.0, 0.0)},
    {glm::vec3(0.5f, 0.5f, 0.0f), glm::vec2(1.0, 1.0)},
    {glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec2(0.0, 1.0)}
};

const u16 indices[] = {
    0, 1, 2,
    0, 2, 3
};

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
    app->mode = Mode_TexturedMesh;

    switch (app->mode)
    {
    case Mode_TexturedQuad:
        {
            // VBO & EBO
            glGenBuffers(1, &app->embeddedVertices);
            glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glGenBuffers(1, &app->embeddedElements);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            // VAO
            glGenVertexArrays(1, &app->vao);
            glBindVertexArray(app->vao);
            glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)12);
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
            glBindVertexArray(0);

            // Program
            app->texturedGeometryProgramIdx = LoadProgram(app, "shaders.glsl", "TEXTURED_GEOMETRY");
            Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];

            app->texturedQuadProgram_uTexture = glGetUniformLocation(texturedGeometryProgram.handle, "uTexture");

            // Texture
            app->diceTexIdx = LoadTexture2D(app, "dice.png");
            app->whiteTexIdx = LoadTexture2D(app, "color_white.png");
            app->blackTexIdx = LoadTexture2D(app, "color_black.png");
            app->normalTexIdx = LoadTexture2D(app, "color_normal.png");
            app->magentaTexIdx = LoadTexture2D(app, "color_magenta.png");
        }
        break;
    case Mode_TexturedMesh:
        {
            // Fill vertex input layout with required attributes
            app->texturedMeshProgramIdx = LoadProgram(app, "shaders.glsl", "SHOW_TEXTURED_MESH");
            Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];

            GLint attribCount = 0;
            glGetProgramiv(texturedMeshProgram.handle, GL_ACTIVE_ATTRIBUTES, &attribCount);

            for (GLint i = 0; i < attribCount; ++i)
            {
                char name[64];
                GLsizei nameLenght = 64;
                GLint size;
                GLenum type;

                glGetActiveAttrib(texturedMeshProgram.handle, i, ARRAY_COUNT(name), &nameLenght, &size, &type, name);

                GLint location = glGetAttribLocation(texturedMeshProgram.handle, name);
                texturedMeshProgram.vetexInputLayout.attributes.push_back({ (u8)location,(u8)size });
            }

            app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");

            app->patrickIdx = LoadModel(app, "Patrick/Patrick.obj");
        }
        break;
    default:;
    }
}

void Gui(App* app)
{
    ImGui::Begin("Info");

    ImGui::BulletText("OpenGL version: %s", glGetString(GL_VERSION));
    ImGui::BulletText("OpenGL renderer: %s", glGetString(GL_RENDERER));
    ImGui::BulletText("OpenGL vendor: %s", glGetString(GL_VENDOR));
    ImGui::BulletText("OpenGL GLSL version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    if (ImGui::TreeNode("OpenGL extensions"))
    {
        GLint num_extensions;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        for (int i = 0; i < num_extensions; ++i)
            ImGui::Text("%s", glGetStringi(GL_EXTENSIONS, GLuint(i)));
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
    switch (app->mode)
    {
        case Mode_TexturedQuad:
            {
                glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glViewport(0, 0, app->displaySize.x, app->displaySize.y);

                Program& texturedGeometryProgram = app->programs[app->texturedGeometryProgramIdx];
                glUseProgram(texturedGeometryProgram.handle);
                glBindVertexArray(app->vao);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glUniform1i(app->texturedQuadProgram_uTexture, 0);
                glActiveTexture(GL_TEXTURE0);
                GLuint textureHandle = app->textures[app->diceTexIdx].handle;
                glBindTexture(GL_TEXTURE_2D, textureHandle);

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

                glBindVertexArray(0);
                glUseProgram(0);
            }
            break;

        case Mode_TexturedMesh:
            {
                glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glViewport(0, 0, app->displaySize.x, app->displaySize.y);

                Program& texturedMeshProgram = app->programs[app->texturedMeshProgramIdx];
                glUseProgram(texturedMeshProgram.handle);

                Model& model = app->models[app->patrickIdx];
                Mesh& mesh = app->meshes[model.meshIdx];

                for (u32 i = 0; i < mesh.submeshes.size(); ++i)
                {
                    GLuint vao = FindVAO(mesh, i, texturedMeshProgram);
                    glBindVertexArray(vao);

                    u32 submeshMaterialIdx = model.materialIdx[i];
                    Material& submeshMaterial = app->materials[submeshMaterialIdx];

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, app->textures[submeshMaterial.albedoTextureIdx].handle);
                    glUniform1i(app->texturedMeshProgram_uTexture, 0);

                    Submesh& submesh = mesh.submeshes[i];
                    glDrawElements(GL_TRIANGLES, submesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)submesh.indexOffset);
                }
            }
            break;

        default:;
    }
}
