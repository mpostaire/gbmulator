#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "glrenderer.h"
#include "bmp.h"

#define VERTEX_INDICES_OBJ_STRIDE 5
#define N_VERTEX_PER_OBJ          24

static const char *vertex_shader_source =
    "#version 300 es\n"
    "in vec2 position;\n"
    "in vec2 tex_coord;\n"
    "in float tint;\n"
    "in float alpha;\n"
    "out vec2 v_tex_coord;\n"
    "out float v_tint;\n"
    "out float v_alpha;\n"
    "uniform mat4 proj;\n"
    "void main() {\n"
    "  gl_Position = proj * vec4(position, 0.0, 1.0);\n"
    "  v_tex_coord = tex_coord;\n"
    "  v_tint = tint;\n"
    "  v_alpha = alpha;\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 v_tex_coord;\n"
    "in float v_tint;\n"
    "in float v_alpha;\n"
    "out vec4 color;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "    color = texture(tex, v_tex_coord);\n"
    "    color = vec4(color.rgb * v_tint, color.a * v_alpha);\n"
    "}\n";

struct glrenderer_t {
    GLuint  screen_tex;
    GLsizei screen_tex_w;
    GLsizei screen_tex_h;

    GLuint vao; // Vertex Array Object
    GLuint vbo; // Vertex Buffer Object
    GLuint ebo; // Element Buffer Object

    GLint u_tex;
    GLint u_proj;

    GLuint  btn_atlas_tex;
    GLsizei btn_atlas_tex_w;
    GLsizei btn_atlas_tex_h;

    GLsizei viewport_w;
    GLsizei viewport_h;

    rect_t  btn_coords[GLRENDERER_OBJ_ID_SCREEN];
    bool    show_buttons;
    GLfloat tints[GLRENDERER_OBJ_ID_SCREEN + 1];
    GLfloat alphas[GLRENDERER_OBJ_ID_SCREEN + 1];

    GLfloat clear_r;
    GLfloat clear_g;
    GLfloat clear_b;
};

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} rect_t;

static GLuint shader_program             = 0;
static size_t shader_program_ref_counter = 0;

static rect_t btn_atlas_regions[] = {
    [GLRENDERER_OBJ_ID_A]               = { .x = 48, .y = 16, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_B]               = { .x = 48, .y = 32, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_SELECT]          = { .x = 32, .y = 48, .w = 32, .h = 8  },
    [GLRENDERER_OBJ_ID_START]           = { .x = 32, .y = 56, .w = 32, .h = 8  },
    [GLRENDERER_OBJ_ID_DPAD_RIGHT]      = { .x = 32, .y = 16, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_LEFT]       = { .x = 0,  .y = 16, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_UP]         = { .x = 16, .y = 0,  .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_DOWN]       = { .x = 16, .y = 32, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_R]               = { .x = 0,  .y = 48, .w = 32, .h = 8  },
    [GLRENDERER_OBJ_ID_L]               = { .x = 0,  .y = 56, .w = 32, .h = 8  },
    [GLRENDERER_OBJ_ID_LINK]            = { .x = 48, .y = 0,  .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_CENTER]     = { .x = 16, .y = 16, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_UP_RIGHT]   = { .x = 32, .y = 0,  .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_UP_LEFT]    = { .x = 0,  .y = 0,  .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT] = { .x = 32, .y = 32, .w = 16, .h = 16 },
    [GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT]  = { .x = 0,  .y = 32, .w = 16, .h = 16 }
};

static GLushort vertex_indices[(GLRENDERER_OBJ_ID_SCREEN + 1) * VERTEX_INDICES_OBJ_STRIDE] = {};

static GLuint compile_shader(GLenum type, const char *source) {
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
    if (fragment_shader == 0)
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
    GLuint texture_id;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    void *placeholder = NULL;
    if (!pixels)
        placeholder = calloc(1, width * height * 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ? pixels : placeholder);

    if (placeholder)
        free(placeholder);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture_id;
}

static void create_buffers(glrenderer_t *renderer) {
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, N_VERTEX_PER_OBJ * sizeof(GLfloat) * (GLRENDERER_OBJ_ID_SCREEN + 1), NULL, GL_STATIC_DRAW);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    GLushort j = 0;
    for (size_t i = 0; i < sizeof(vertex_indices) / sizeof(*vertex_indices);) {
        vertex_indices[i++] = j++;
        vertex_indices[i++] = j++;
        vertex_indices[i++] = j++;
        vertex_indices[i++] = j++;
        vertex_indices[i++] = 0xFFFF; // primitive restart index
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vertex_indices), vertex_indices, GL_STATIC_DRAW);

    GLsizei vertex_stride = 6 * sizeof(GLfloat);

    GLint position_loc = glGetAttribLocation(shader_program, "position");
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) 0);
    glEnableVertexAttribArray(position_loc);

    GLint tex_coord_loc = glGetAttribLocation(shader_program, "tex_coord");
    glVertexAttribPointer(tex_coord_loc, 2, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(tex_coord_loc);

    GLint tint_loc = glGetAttribLocation(shader_program, "tint");
    glVertexAttribPointer(tint_loc, 1, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (4 * sizeof(GLfloat)));
    glEnableVertexAttribArray(tint_loc);

    GLint alpha_loc = glGetAttribLocation(shader_program, "alpha");
    glVertexAttribPointer(alpha_loc, 1, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(alpha_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void create_buttons(glrenderer_t *renderer) {
    if (glIsTexture(renderer->btn_atlas_tex))
        return;

    // clang-format off
    static uint8_t atlas_data[] = {
        #embed "atlas.bmp"
    };
    // clang-format on

    bmp_image_t *atlas_bmp = bmp_decode(atlas_data, sizeof(atlas_data));
    if (atlas_bmp) {
        renderer->btn_atlas_tex   = create_texture(atlas_bmp->w, atlas_bmp->h, atlas_bmp->data);
        renderer->btn_atlas_tex_w = atlas_bmp->w;
        renderer->btn_atlas_tex_h = atlas_bmp->h;

        free(atlas_bmp);
    } else {
        renderer->show_buttons = false;
        printf("[ERROR] Couldn't load btn texture atlas\n");
    }
}

// TODO try printer renderer to check it doesn't break it
glrenderer_t *glrenderer_init(GLsizei screen_w, GLsizei screen_h, bool show_buttons) {
    static bool is_first_init = true;
    if (is_first_init) {
        printf("Renderer: %s\n", glGetString(GL_VERSION));
        is_first_init = false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    if (!shader_program)
        shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);
    shader_program_ref_counter++;

    glrenderer_t *renderer = calloc(1, sizeof(*renderer));
    if (!renderer)
        return NULL;

    renderer->show_buttons = show_buttons;

    create_buffers(renderer);

    renderer->screen_tex   = create_texture(screen_w, screen_h, NULL);
    renderer->screen_tex_w = screen_w;
    renderer->screen_tex_h = screen_h;

    if (renderer->show_buttons)
        create_buttons(renderer);

    glUseProgram(shader_program);

    renderer->u_tex = glGetUniformLocation(shader_program, "tex");
    glUniform1i(renderer->u_tex, 0);

    renderer->u_proj = glGetUniformLocation(shader_program, "proj");

    for (glrenderer_obj_id_t obj_id = 0; obj_id <= GLRENDERER_OBJ_ID_SCREEN; obj_id++) {
        renderer->tints[obj_id]  = 1.0f;
        renderer->alphas[obj_id] = 1.0f;
    }

    glrenderer_resize_viewport(renderer, screen_w, screen_h);

    if (renderer->show_buttons) {
        renderer->clear_r = 0.0f;
        renderer->clear_g = 0.0f;
        renderer->clear_b = 0.0f;
    } else {
        renderer->clear_r = 1.0f;
        renderer->clear_g = 1.0f;
        renderer->clear_b = 1.0f;
    }

    return renderer;
}

void glrenderer_quit(glrenderer_t *renderer) {
    if (!renderer)
        return;

    glDeleteTextures(1, &renderer->screen_tex);
    if (renderer->show_buttons)
        glDeleteTextures(1, &renderer->btn_atlas_tex);
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteBuffers(1, &renderer->ebo);

    if (shader_program_ref_counter == 1)
        glDeleteProgram(shader_program);
    shader_program_ref_counter--;
}

void glrenderer_render(glrenderer_t *renderer) {
    if (!renderer)
        return;

    glClearColor(renderer->clear_r, renderer->clear_g, renderer->clear_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Call to glUseProgram useless as only one program is ever used in the lifespan of a glrenderer_t instance
    // and it was already called int glrenderer_init()

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    glBindTexture(GL_TEXTURE_2D, renderer->screen_tex);
    glDrawElements(GL_TRIANGLE_STRIP, VERTEX_INDICES_OBJ_STRIDE, GL_UNSIGNED_SHORT, (GLvoid *) (GLRENDERER_OBJ_ID_SCREEN * VERTEX_INDICES_OBJ_STRIDE * sizeof(*vertex_indices)));

    if (renderer->show_buttons) {
        glBindTexture(GL_TEXTURE_2D, renderer->btn_atlas_tex);
        glDrawElements(GL_TRIANGLE_STRIP, (sizeof(vertex_indices) / sizeof(*vertex_indices)) - VERTEX_INDICES_OBJ_STRIDE, GL_UNSIGNED_SHORT, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void glrenderer_update_screen(glrenderer_t *renderer, const GLvoid *pixels) {
    if (!renderer)
        return;

    glBindTexture(GL_TEXTURE_2D, renderer->screen_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, renderer->screen_tex_w, renderer->screen_tex_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void glrenderer_resize_screen(glrenderer_t *renderer, GLsizei width, GLsizei height) {
    if (!renderer || (width == renderer->screen_tex_w && height == renderer->screen_tex_h))
        return;

    glBindTexture(GL_TEXTURE_2D, renderer->screen_tex);
    // NULL as pixel data: opengl allocates texture but doesn't copy any pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    renderer->screen_tex_w = width;
    renderer->screen_tex_h = height;
}

static inline void update_orthographic_proj(glrenderer_t *renderer, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far) {
    // clang-format off
    GLfloat proj_data[] = {
        2.0f / (right - left),            0.0f,                             0.0f,                         0.0f,
        0.0f,                             2.0f / (top - bottom),            0.0f,                         0.0f,
        0.0f,                             0.0f,                             -2.0f / (far - near),         0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f
    };
    // clang-format on

    glUniformMatrix4fv(renderer->u_proj, 1, GL_FALSE, proj_data);
}

static inline void update_vertices(glrenderer_t *renderer, GLint obj_id, rect_t *coords) {
    GLfloat u0, v0;
    GLfloat u1, v1;

    GLfloat tint  = renderer->tints[obj_id];
    GLfloat alpha = renderer->alphas[obj_id];

    if (obj_id == GLRENDERER_OBJ_ID_SCREEN) {
        u0 = 0.0f;
        u1 = 1.0f;
        v0 = 0.0f;
        v1 = 1.0f;
    } else {
        u0 = btn_atlas_regions[obj_id].x / (GLfloat) renderer->btn_atlas_tex_w;
        v0 = btn_atlas_regions[obj_id].y / (GLfloat) renderer->btn_atlas_tex_h;
        u1 = (btn_atlas_regions[obj_id].x + btn_atlas_regions[obj_id].w) / (GLfloat) renderer->btn_atlas_tex_w;
        v1 = (btn_atlas_regions[obj_id].y + btn_atlas_regions[obj_id].h) / (GLfloat) renderer->btn_atlas_tex_h;
    }

    // clang-format off
    GLfloat vertices[N_VERTEX_PER_OBJ] = {
        //        x,                      y,            u,  v,  tint, alpha
        coords->x,              coords->y + coords->h,  u0, v1, tint, alpha, // bottom-left
        coords->x + coords->w,  coords->y + coords->h,  u1, v1, tint, alpha, // bottom-right
        coords->x,              coords->y,              u0, v0, tint, alpha, // top-left
        coords->x + coords->w,  coords->y,              u1, v0, tint, alpha  // top-right
    };
    // clang-format on

    GLintptr offset = obj_id * sizeof(vertices);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

glrenderer_obj_id_t glrenderer_get_obj_at_coord(glrenderer_t *renderer, uint32_t x, uint32_t y) {
    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_SCREEN; obj_id++) {
        if (x < renderer->btn_coords[obj_id].x || x > renderer->btn_coords[obj_id].x + renderer->btn_coords[obj_id].w || y < renderer->btn_coords[obj_id].y || y > renderer->btn_coords[obj_id].y + renderer->btn_coords[obj_id].h)
            continue;
        return obj_id;
    }

    return GLRENDERER_OBJ_ID_SCREEN;
}

void glrenderer_resize_viewport(glrenderer_t *renderer, GLsizei width, GLsizei height) {
    if (!renderer)
        return;

    glViewport(0, 0, width, height);

    renderer->viewport_w = width;
    renderer->viewport_h = height;

    update_orthographic_proj(renderer, 0, renderer->viewport_w, renderer->viewport_h, 0, -1, 1);

    // fit screen_tex inside viewport while keeping aspect ratio
    GLfloat image_ratio    = (GLfloat) renderer->screen_tex_w / (GLfloat) renderer->screen_tex_h;
    GLfloat viewport_ratio = (GLfloat) renderer->viewport_w / (GLfloat) renderer->viewport_h;

    rect_t screen_coords;

    if (image_ratio > viewport_ratio) {
        screen_coords.w = renderer->viewport_w;
        screen_coords.h = screen_coords.w / image_ratio;
    } else {
        screen_coords.h = renderer->viewport_h;
        screen_coords.w = screen_coords.h * image_ratio;
    }

    screen_coords.x = (renderer->viewport_w - screen_coords.w) / 2.0;
    if (renderer->show_buttons)
        screen_coords.y = 0;
    else
        screen_coords.y = (renderer->viewport_h - screen_coords.h) / 2.0;

    update_vertices(renderer, GLRENDERER_OBJ_ID_SCREEN, &screen_coords);

    if (renderer->show_buttons) {
        GLfloat btn_scale = screen_coords.w / (GLfloat) renderer->screen_tex_w;

        renderer->btn_coords[GLRENDERER_OBJ_ID_SELECT] = (rect_t) {
            .x = 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].w * btn_scale),
            .y = renderer->viewport_h - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].h * btn_scale
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_START] = (rect_t) {
            .x = renderer->viewport_w - 3.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_START].w * btn_scale),
            .y = renderer->viewport_h - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_START].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_START].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_START].h * btn_scale
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_R] = (rect_t) {
            .x = renderer->viewport_w - 1.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_R].w * btn_scale),
            .y = (renderer->viewport_h - screen_coords.h) + (btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_R].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_R].h * btn_scale
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_L] = (rect_t) {
            .x = 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_L].w * btn_scale),
            .y = (renderer->viewport_h - screen_coords.h) + (btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_L].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale
        };

        rect_t dpad_rect = {
            .x = 1.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].w * btn_scale),
            .y = renderer->viewport_h - 4.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].h * btn_scale
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP_LEFT] = (rect_t) {
            .x = dpad_rect.x,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_LEFT] = (rect_t) {
            .x = dpad_rect.x,
            .y = dpad_rect.y + renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP_LEFT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_LEFT].x + dpad_rect.w,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_LEFT].y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x + dpad_rect.w,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_DOWN] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].y + renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP_RIGHT] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP].x + renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP].w,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT] = (rect_t) {
            .x = dpad_rect.x,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].y + renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x + dpad_rect.w,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].y + renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_A] = (rect_t) {
            .x = renderer->viewport_w - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_A].w * btn_scale),
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_UP].y + 0.8f * btn_atlas_regions[GLRENDERER_OBJ_ID_A].w,
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_A].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_A].h * btn_scale
        };
        renderer->btn_coords[GLRENDERER_OBJ_ID_B] = (rect_t) {
            .x = renderer->btn_coords[GLRENDERER_OBJ_ID_A].x - btn_atlas_regions[GLRENDERER_OBJ_ID_B].w * btn_scale,
            .y = renderer->btn_coords[GLRENDERER_OBJ_ID_DPAD_DOWN].y - 0.8f * btn_atlas_regions[GLRENDERER_OBJ_ID_B].w,
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_B].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_B].h * btn_scale
        };

        renderer->btn_coords[GLRENDERER_OBJ_ID_LINK] = (rect_t) {
            .x = 0.5f * renderer->viewport_w - 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].w * btn_scale),
            .y = (renderer->viewport_h - screen_coords.h) + 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].h * btn_scale
        };

        for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_SCREEN; obj_id++)
            update_vertices(renderer, obj_id, &renderer->btn_coords[obj_id]);
    }
}

void glrenderer_set_obj_tint(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat tint) {
    if (!renderer || obj_id < 0 || obj_id > GLRENDERER_OBJ_ID_SCREEN || tint < 0.0f || tint > 1.0f)
        return;

    renderer->tints[obj_id] = tint;
    update_vertices(renderer, obj_id, &renderer->btn_coords[obj_id]);
}

void glrenderer_set_obj_alpha(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat alpha) {
    if (!renderer || obj_id < 0 || obj_id > GLRENDERER_OBJ_ID_SCREEN || alpha < 0.0f || alpha > 1.0f)
        return;

    renderer->alphas[obj_id] = alpha;
    update_vertices(renderer, obj_id, &renderer->btn_coords[obj_id]);
}

void glrenderer_set_show_buttons(glrenderer_t *renderer, bool show_buttons) {
    if (!renderer)
        return;

    renderer->show_buttons = show_buttons;

    if (renderer->show_buttons)
        create_buttons(renderer);

    if (renderer->show_buttons) {
        renderer->clear_r = 0.0f;
        renderer->clear_g = 0.0f;
        renderer->clear_b = 0.0f;
    } else {
        renderer->clear_r = 1.0f;
        renderer->clear_g = 1.0f;
        renderer->clear_b = 1.0f;
    }

    glrenderer_resize_viewport(renderer, renderer->viewport_w, renderer->viewport_h);
}
