#pragma once

#include <GL/gl.h>

void glrenderer_init(GLsizei width, GLsizei height, const GLvoid *pixels);

void glrenderer_render(void);

void glrenderer_update_screen_texture(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GLvoid *pixels);
