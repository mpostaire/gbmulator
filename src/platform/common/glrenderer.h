#pragma once

#include <GLES3/gl3.h>

#define GLRENDERER_CONF_UNIFORM_COLOR_CORRECTION "color_correction"

typedef struct glrenderer_t glrenderer_t;

typedef enum {
    COLOR_CORRECTION_NONE,
    COLOR_CORRECTION_CGB
} glrenderer_color_correction_t;

glrenderer_t *glrenderer_init(GLsizei width, GLsizei height, const GLvoid *pixels);

void glrenderer_quit(glrenderer_t *renderer);

void glrenderer_render(glrenderer_t *renderer);

void glrenderer_update_texture(glrenderer_t *renderer, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GLvoid *pixels);

void glrenderer_resize_texture(glrenderer_t *renderer, GLsizei width, GLsizei height);

void glrenderer_set_conf_uniform(glrenderer_t *renderer, const GLchar *uniform, GLuint value);
