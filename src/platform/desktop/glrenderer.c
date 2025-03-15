#include "../../core/core.h"

#include <stdlib.h>
#include <adwaita.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include "glrenderer.h"

// Vertices coordinates
static GLfloat vertices[] = {
    //     COORDINATES   /  TexCoord (vertically flipped)
    -1.0f, -1.0f, 0.0f,    0.0f, 1.0f, // Lower left corner
    -1.0f,  1.0f, 0.0f,    0.0f, 0.0f, // Upper left corner
    1.0f,  1.0f, 0.0f,     1.0f, 0.0f, // Upper right corner
    1.0f, -1.0f, 0.0f,     1.0f, 1.0f  // Lower right corner
};

// Indices for vertices order
static GLuint indices[] = {
    0, 2, 1, // Upper triangle
    0, 3, 2 // Lower triangle
};

struct glrenderer_t {
    GLuint texture;
    GLuint VAO; // Vertex Array Object
    GLuint VBO; // Vertex Buffer Object
    GLuint EBO; // Element Buffer Object
};

static GLuint shader_program;

static GLuint create_shader_program(const char *vertex_source_path, const char *fragment_source_path) {
    GBytes *vertex_shader_data = g_resources_lookup_data(vertex_source_path, 0, NULL);
    gsize data_size = g_bytes_get_size(vertex_shader_data);
    const char *vertex_shader_source = g_bytes_get_data(vertex_shader_data, &data_size);

    GBytes *fragment_shader_data = g_resources_lookup_data(fragment_source_path, 0, NULL);
    data_size = g_bytes_get_size(fragment_shader_data);
    const char *fragment_shader_source = g_bytes_get_data(fragment_shader_data, &data_size);

    // Create Vertex Shader Object and get its reference
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    // Attach Vertex Shader source to the Vertex Shader Object
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    // Compile the Vertex Shader into machine code
    glCompileShader(vertex_shader);

    // Create Fragment Shader Object and get its reference
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    // Attach Fragment Shader source to the Fragment Shader Object
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    // Compile the Vertex Shader into machine code
    glCompileShader(fragment_shader);

    // Create Shader Program Object and get its reference
    GLuint program = glCreateProgram();
    // Attach the Vertex and Fragment Shaders to the Shader Program
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    // Wrap-up/Link all the shaders together into the Shader Program
    glLinkProgram(program);

    // Delete the now useless Vertex and Fragment Shader objects
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

static GLuint create_texture(GLsizei width, GLsizei height, const GLvoid *pixels) {
    // Create one OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);

    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);

    // Give the image to OpenGL
    void *placeholder = NULL;
    if (!pixels)
        placeholder = xcalloc(1, width * height * 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ? pixels : placeholder);

    if (placeholder)
        free(placeholder);

    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

glrenderer_t *glrenderer_init(GLsizei width, GLsizei height, const GLvoid *pixels) {
    // printf("Renderer: %s\n", glGetString(GL_RENDERER));
    // printf("OpenGL version supported %s\n", glGetString(GL_VERSION));

    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        eprintf("Error initializing GLEW! %s\n", glewGetErrorString(glewError));
        exit(EXIT_FAILURE);
    }

    // Create and load shader
    if (!shader_program) {
        shader_program = create_shader_program(
            "/io/github/mpostaire/gbmulator/src/platform/desktop/ui/default.vs.glsl",
            "/io/github/mpostaire/gbmulator/src/platform/desktop/ui/default.fs.glsl");
    }

    glrenderer_t *renderer = xcalloc(1, sizeof(*renderer));

    // Generate the VAO, VBO, and EBO with only 1 object each
    glGenVertexArrays(1, &renderer->VAO);
    glGenBuffers(1, &renderer->VBO);
    glGenBuffers(1, &renderer->EBO);

    // Make the VAO the current Vertex Array Object by binding it
    glBindVertexArray(renderer->VAO);

    // Bind the VBO specifying it's a GL_ARRAY_BUFFER
    glBindBuffer(GL_ARRAY_BUFFER, renderer->VBO);
    // Introduce the vertices into the VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Bind the EBO specifying it's a GL_ELEMENT_ARRAY_BUFFER
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->EBO);
    // Introduce the indices into the EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Configure the Vertex Attribute so that OpenGL knows how to read the VBO
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    // Enable the Vertex Attribute so that OpenGL knows to use it
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // Bind the EBO to 0 so that we don't accidentally modify it
    // MAKE SURE TO UNBIND IT AFTER UNBINDING THE VAO, as the EBO is linked in the VAO
    // This does not apply to the VBO because the VBO is already linked to the VAO during glVertexAttribPointer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    renderer->texture = create_texture(width, height, pixels);

    // Tell OpenGL which Shader Program we want to use
    glUseProgram(shader_program);

    GLuint tex0_uniform_id = glGetUniformLocation(shader_program, "tex0");
    glUniform1i(tex0_uniform_id, 0);

    // Specify the color of the background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    return renderer;
}

void glrenderer_quit(glrenderer_t *renderer) {
    glDeleteTextures(1, &renderer->texture);
    glDeleteVertexArrays(1, &renderer->VAO);
    glDeleteBuffers(1, &renderer->VBO);
    glDeleteBuffers(1, &renderer->EBO);
}

void glrenderer_render(glrenderer_t *renderer) {
    // Clean the back buffer and assign the new color to it
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    // Bind the VAO so OpenGL knows to use it
    glBindVertexArray(renderer->VAO);
    // Draw primitives, number of indices, datatype of indices, index of indices
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void glrenderer_update_texture(glrenderer_t *renderer, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GLvoid *pixels) {
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void glrenderer_resize_texture(glrenderer_t *renderer, GLsizei width, GLsizei height) {
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    // NULL as pixel data: opengl allocates texture but doesn't copy any pixel data 
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
}
