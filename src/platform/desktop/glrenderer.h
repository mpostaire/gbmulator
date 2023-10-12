#pragma once

#include <GL/gl.h>

typedef struct glrenderer_t glrenderer_t;

glrenderer_t *glrenderer_init(GLsizei width, GLsizei height, const GLvoid *pixels);

void glrenderer_quit(glrenderer_t *renderer);

void glrenderer_render(glrenderer_t *renderer);

void glrenderer_update_texture(glrenderer_t *renderer, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GLvoid *pixels);

void glrenderer_resize_texture(glrenderer_t *renderer, GLsizei width, GLsizei height);
