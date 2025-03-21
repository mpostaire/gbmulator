#include <stdlib.h>
#include <stdio.h>
#include <GLES3/gl3.h>

#include "glrenderer.h"

static const char *vertex_shader_source =
    "#version 300 es\n"
    "in vec2 position;\n"
    "in vec2 fixed_tex_coord;\n"
    "out vec2 tex_coord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    tex_coord = fixed_tex_coord;\n"
    "}";

static const char fragment_shader_source[] = {
    // TODO use embed or better alternative
    // #embed "default.fs.glsl"
    "#version 300 es\n"
    "\n"
    "precision mediump float;\n"
    "\n"
    "// Inputs the texture coordinates from the Vertex Shader\n"
    "in vec2 tex_coord;\n"
    "\n"
    "// Outputs colors in RGBA\n"
    "out vec4 color;\n"
    "\n"
    "// Gets the Texture Unit from the main function\n"
    "uniform sampler2D tex0;\n"
    "\n"
    "void main() {\n"
    "    color = texture(tex0, tex_coord);\n"
    "}"
};

struct glrenderer_t {
    GLuint texture;
    GLuint VAO; // Vertex Array Object
    GLuint VBO; // Vertex Buffer Object
};

static GLuint shader_program;

static GLuint compile_shader(GLenum type, const char* source) {
    // Create Vertex Shader Object and get its reference
    GLuint shader = glCreateShader(type);
    // Attach Vertex Shader source to the Vertex Shader Object
    glShaderSource(shader, 1, &source, NULL);
    // Compile the Vertex Shader into machine code
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        printf("%s\n", info_log);
        return 0;
    }

    return shader;
}

static GLuint create_shader_program(const char *vertex_shader_source, const char *fragment_shader_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    if (vertex_shader == 0)
        return 0;

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (vertex_shader == 0)
        return 0;

    // Create Shader Program Object and get its reference
    GLuint program = glCreateProgram();
    // Attach the Vertex and Fragment Shaders to the Shader Program
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    // Wrap-up/Link all the shaders together into the Shader Program
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(program, sizeof(info_log), NULL, info_log);
        printf("%s\n", info_log);
        return 0;
    }

    // Delete the now useless Vertex and Fragment Shader objects
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

static GLuint create_texture(GLsizei width, GLsizei height, const GLvoid *pixels) {
    // Create one OpenGL texture
    GLuint texture_id;
    glGenTextures(1, &texture_id);

    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Give the image to OpenGL
    void *placeholder = NULL;
    if (!pixels)
        placeholder = calloc(1, width * height * 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ? pixels : placeholder);

    if (placeholder)
        free(placeholder);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture_id;
}

glrenderer_t *glrenderer_init(GLsizei width, GLsizei height, const GLvoid *pixels) {
    // Create and load shader
    if (!shader_program)
        shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);

    glrenderer_t *renderer = calloc(1, sizeof(*renderer));
    if (!renderer)
        return NULL;

    glGenVertexArrays(1, &renderer->VAO);
    glGenBuffers(1, &renderer->VBO);

    glBindVertexArray(renderer->VAO);

    // Render a fullscreen quad
    static GLfloat vertices[] = {
        -1.0f, -1.0f,    0.0f, 1.0f,
         1.0f, -1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,    0.0f, 0.0f,
         1.0f,  1.0f,    1.0f, 0.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, renderer->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint position_location = glGetAttribLocation(shader_program, "position");
    glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(*vertices), (GLvoid *)0);
    glEnableVertexAttribArray(position_location);

    GLint fixed_tex_coord_location = glGetAttribLocation(shader_program, "fixed_tex_coord");
    glVertexAttribPointer(fixed_tex_coord_location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(*vertices), (GLvoid *) (2 * sizeof(*vertices)));
    glEnableVertexAttribArray(fixed_tex_coord_location);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    renderer->texture = create_texture(width, height, pixels);

    // Tell OpenGL which Shader Program we want to use
    glUseProgram(shader_program);

    GLint tex0_uniform_id = glGetUniformLocation(shader_program, "tex0");
    glUniform1i(tex0_uniform_id, 0);

    GLint color_correction_uniform_id = glGetUniformLocation(shader_program, GLRENDERER_CONF_UNIFORM_COLOR_CORRECTION);
    glUniform1ui(color_correction_uniform_id, COLOR_CORRECTION_CGB);

    // Specify the color of the background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    return renderer;
}

void glrenderer_quit(glrenderer_t *renderer) {
    glDeleteTextures(1, &renderer->texture);
    glDeleteVertexArrays(1, &renderer->VAO);
    glDeleteBuffers(1, &renderer->VBO);
}

void glrenderer_render(glrenderer_t *renderer) {
    // Clean the back buffer and assign the new color to it
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    glBindVertexArray(renderer->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->VBO);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
