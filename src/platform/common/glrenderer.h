#pragma once

#include <GLES3/gl3.h>

typedef struct glrenderer_t glrenderer_t;

typedef enum {
    GLRENDERER_OBJ_ID_A,
    GLRENDERER_OBJ_ID_B,
    GLRENDERER_OBJ_ID_SELECT,
    GLRENDERER_OBJ_ID_START,
    GLRENDERER_OBJ_ID_DPAD_RIGHT,
    GLRENDERER_OBJ_ID_DPAD_LEFT,
    GLRENDERER_OBJ_ID_DPAD_UP,
    GLRENDERER_OBJ_ID_DPAD_DOWN,
    GLRENDERER_OBJ_ID_R,
    GLRENDERER_OBJ_ID_L,
    GLRENDERER_OBJ_ID_LINK,
    GLRENDERER_OBJ_ID_DPAD_CENTER,
    GLRENDERER_OBJ_ID_DPAD_UP_RIGHT,
    GLRENDERER_OBJ_ID_DPAD_UP_LEFT,
    GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT,
    GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT,
    GLRENDERER_OBJ_ID_SCREEN // must be the last element of this enum
} glrenderer_obj_id_t;

glrenderer_t *glrenderer_init(GLsizei screen_w, GLsizei screen_h, uint32_t visible_btns_mask);

void glrenderer_quit(glrenderer_t *renderer);

void glrenderer_render(glrenderer_t *renderer);

void glrenderer_update_screen(glrenderer_t *renderer, const GLvoid *pixels);

void glrenderer_resize_screen(glrenderer_t *renderer, GLsizei width, GLsizei height);

void glrenderer_resize_viewport(glrenderer_t *renderer, GLsizei width, GLsizei height);

glrenderer_obj_id_t glrenderer_get_obj_at_coord(glrenderer_t *renderer, uint32_t x, uint32_t y);

void glrenderer_set_obj_tint(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat tint);

void glrenderer_set_obj_alpha(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat alpha);

void glrenderer_set_show_buttons(glrenderer_t *renderer, uint32_t visible_btns_mask);
