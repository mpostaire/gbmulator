#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLES3/gl3.h>

#include "glrenderer.h"
#include "bmp.h"

#define VERTEX_INDICES_OBJ_STRIDE 5
#define N_VERTEX_PER_OBJ          24
#define PIXBUF_COUNT              2

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

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} rect_t;

struct glrenderer_t {
    GLuint  screen_tex;
    GLsizei screen_tex_w;
    GLsizei screen_tex_h;

    uint8_t *pixbufs[PIXBUF_COUNT];
    uint8_t  writing_pixbuf;
    uint8_t  rendering_pixbuf;

    GLuint vao; // Vertex Array Object
    GLuint vbo; // Vertex Buffer Object
    GLuint ebo; // Element Buffer Object

    GLuint shader_program;

    GLint u_tex;
    GLint u_proj;

    GLuint  btn_atlas_tex;
    GLsizei btn_atlas_tex_w;
    GLsizei btn_atlas_tex_h;

    GLsizei viewport_w;
    GLsizei viewport_h;

    GLushort vertex_indices[GLRENDERER_OBJ_ID_END * VERTEX_INDICES_OBJ_STRIDE];
    rect_t   obj_coords[GLRENDERER_OBJ_ID_END];
    uint32_t visible_btns_mask;
    GLfloat  tints[GLRENDERER_OBJ_ID_END];
    GLfloat  alphas[GLRENDERER_OBJ_ID_END];

    GLfloat clear_r;
    GLfloat clear_g;
    GLfloat clear_b;

    bool     resize_screen_tex_requested;
    uint32_t resize_pixbuf_requests;
    bool     resize_viewport_requested;
    uint32_t update_obj_requests;
};

static const rect_t btn_atlas_regions[] = {
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture_id;
}

static void create_buffers(glrenderer_t *renderer) {
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, N_VERTEX_PER_OBJ * sizeof(GLfloat) * GLRENDERER_OBJ_ID_END, NULL, GL_STATIC_DRAW);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    GLushort j = 0;
    for (size_t i = 0; i < sizeof(renderer->vertex_indices) / sizeof(*renderer->vertex_indices);) {
        renderer->vertex_indices[i++] = j++;
        renderer->vertex_indices[i++] = j++;
        renderer->vertex_indices[i++] = j++;
        renderer->vertex_indices[i++] = j++;
        renderer->vertex_indices[i++] = 0xFFFF; // primitive restart index
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(renderer->vertex_indices), renderer->vertex_indices, GL_STATIC_DRAW);

    GLsizei vertex_stride = 6 * sizeof(GLfloat);

    GLint position_loc = glGetAttribLocation(renderer->shader_program, "position");
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) 0);
    glEnableVertexAttribArray(position_loc);

    GLint tex_coord_loc = glGetAttribLocation(renderer->shader_program, "tex_coord");
    glVertexAttribPointer(tex_coord_loc, 2, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(tex_coord_loc);

    GLint tint_loc = glGetAttribLocation(renderer->shader_program, "tint");
    glVertexAttribPointer(tint_loc, 1, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (4 * sizeof(GLfloat)));
    glEnableVertexAttribArray(tint_loc);

    GLint alpha_loc = glGetAttribLocation(renderer->shader_program, "alpha");
    glVertexAttribPointer(alpha_loc, 1, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid *) (5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(alpha_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void create_buttons(glrenderer_t *renderer) {
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
        renderer->visible_btns_mask = false;
        printf("[ERROR] Couldn't load btn texture atlas\n");
    }
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
    GLfloat alpha = 0.0f;

    if (obj_id == GLRENDERER_OBJ_ID_SCREEN) {
        u0    = 0.0f;
        u1    = 1.0f;
        v0    = 0.0f;
        v1    = 1.0f;
        alpha = 1.0f;
    } else {
        u0 = btn_atlas_regions[obj_id].x / (GLfloat) renderer->btn_atlas_tex_w;
        v0 = btn_atlas_regions[obj_id].y / (GLfloat) renderer->btn_atlas_tex_h;
        u1 = (btn_atlas_regions[obj_id].x + btn_atlas_regions[obj_id].w) / (GLfloat) renderer->btn_atlas_tex_w;
        v1 = (btn_atlas_regions[obj_id].y + btn_atlas_regions[obj_id].h) / (GLfloat) renderer->btn_atlas_tex_h;
        if (renderer->visible_btns_mask & (1 << obj_id))
            alpha = renderer->alphas[obj_id];
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

static void resize_screen_tex(glrenderer_t *renderer) {
    glBindTexture(GL_TEXTURE_2D, renderer->screen_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderer->screen_tex_w, renderer->screen_tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    renderer->resize_screen_tex_requested = false;
}

static void resize_pixbufs(glrenderer_t *renderer) {
    for (uint8_t i = 0; i < PIXBUF_COUNT; i++) {
        if ((i == renderer->writing_pixbuf) || !(renderer->resize_pixbuf_requests & (1 << i)))
            continue;

        printf("resizing %d to %dx%d (writing %d)\n", i, renderer->screen_tex_w, renderer->screen_tex_h, renderer->writing_pixbuf);

        size_t   new_size   = renderer->screen_tex_w * renderer->screen_tex_h * 4;
        uint8_t *new_pixbuf = realloc(renderer->pixbufs[i], new_size);
        if (new_pixbuf)
            renderer->pixbufs[i] = new_pixbuf;
        else
            printf("[ERROR] Couldn't allocate pixbuf\n");

        renderer->resize_pixbuf_requests &= ~(1 << i);
    }
}

static void resize_viewport(glrenderer_t *renderer) {
    glViewport(0, 0, renderer->viewport_w, renderer->viewport_h);
    update_orthographic_proj(renderer, 0, renderer->viewport_w, renderer->viewport_h, 0, -1, 1);

    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_END; obj_id++)
        update_vertices(renderer, obj_id, &renderer->obj_coords[obj_id]);

    renderer->resize_viewport_requested = false;
}

static void resize_screen(glrenderer_t *renderer, GLsizei width, GLsizei height) {
    if (!renderer || (width == renderer->screen_tex_w && height == renderer->screen_tex_h))
        return;

    printf("resize request: %dx%d --> %dx%d\n", renderer->screen_tex_w, renderer->screen_tex_h, width, height);

    renderer->resize_pixbuf_requests      = (1 << PIXBUF_COUNT) - 1;
    renderer->resize_screen_tex_requested = true;

    renderer->screen_tex_w = width;
    renderer->screen_tex_h = height;

    // we can already resize non writing pixbufs here
    resize_pixbufs(renderer);

    // resizing screen requires updating viewport to recompute obj coordinates (including screen)
    glrenderer_resize_viewport(renderer, renderer->viewport_w, renderer->viewport_h);
}

static void update_objs(glrenderer_t *renderer) {
    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_END; obj_id++)
        if (renderer->update_obj_requests & (1 << obj_id))
            update_vertices(renderer, obj_id, &renderer->obj_coords[obj_id]);

    renderer->update_obj_requests = 0;
}

glrenderer_t *glrenderer_init(GLsizei screen_w, GLsizei screen_h, uint32_t visible_btns_mask) {
    static bool is_first_init = true;
    if (is_first_init) {
        printf("Renderer: %s\n", glGetString(GL_VERSION));
        is_first_init = false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glrenderer_t *renderer = calloc(1, sizeof(*renderer));
    if (!renderer)
        return NULL;

    renderer->shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);

    renderer->visible_btns_mask = visible_btns_mask;

    renderer->screen_tex   = create_texture(screen_w, screen_h, NULL);
    renderer->screen_tex_w = screen_w;
    renderer->screen_tex_h = screen_h;

    create_buffers(renderer);

    for (uint8_t i = 0; i < PIXBUF_COUNT; i++) {
        renderer->pixbufs[i] = malloc(renderer->screen_tex_w * renderer->screen_tex_h * 4);
        if (!renderer->pixbufs[i]) {
            printf("[ERROR] Couldn't allocate pixbuf\n");
            return NULL;
        }
    }

    if (renderer->visible_btns_mask)
        create_buttons(renderer);

    glUseProgram(renderer->shader_program);

    renderer->u_tex = glGetUniformLocation(renderer->shader_program, "tex");
    glUniform1i(renderer->u_tex, 0);

    renderer->u_proj = glGetUniformLocation(renderer->shader_program, "proj");

    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_END; obj_id++) {
        renderer->tints[obj_id]  = 1.0f;
        renderer->alphas[obj_id] = 1.0f;
    }

    glrenderer_resize_viewport(renderer, screen_w, screen_h);

    if (renderer->visible_btns_mask) {
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
    if (glIsTexture(renderer->btn_atlas_tex))
        glDeleteTextures(1, &renderer->btn_atlas_tex);
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteBuffers(1, &renderer->ebo);

    glDeleteProgram(renderer->shader_program);

    for (uint8_t i = 0; i < PIXBUF_COUNT; i++)
        free(renderer->pixbufs[i]);

    free(renderer);
}

void glrenderer_render(glrenderer_t *renderer) {
    if (!renderer)
        return;

    if (renderer->resize_screen_tex_requested)
        resize_screen_tex(renderer);

    if (renderer->resize_pixbuf_requests)
        resize_pixbufs(renderer);

    if (renderer->resize_viewport_requested)
        resize_viewport(renderer);

    if (renderer->update_obj_requests)
        update_objs(renderer);

    // Call to glUseProgram useless as only one program is ever used in the lifespan of a glrenderer_t instance
    // and it was already called int glrenderer_init()

    glClearColor(renderer->clear_r, renderer->clear_g, renderer->clear_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    glBindTexture(GL_TEXTURE_2D, renderer->screen_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, renderer->screen_tex_w, renderer->screen_tex_h, GL_RGBA, GL_UNSIGNED_BYTE, renderer->pixbufs[renderer->rendering_pixbuf]);

    glDrawElements(GL_TRIANGLE_STRIP, VERTEX_INDICES_OBJ_STRIDE, GL_UNSIGNED_SHORT, (GLvoid *) (GLRENDERER_OBJ_ID_SCREEN * VERTEX_INDICES_OBJ_STRIDE * sizeof(*renderer->vertex_indices)));

    if (renderer->visible_btns_mask) {
        glBindTexture(GL_TEXTURE_2D, renderer->btn_atlas_tex);
        glDrawElements(GL_TRIANGLE_STRIP, (sizeof(renderer->vertex_indices) / sizeof(*renderer->vertex_indices)) - VERTEX_INDICES_OBJ_STRIDE, GL_UNSIGNED_SHORT, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

uint8_t *glrenderer_swap_buffers(glrenderer_t *renderer, size_t w, size_t h) {
    if (!renderer)
        return NULL;

    resize_screen(renderer, w, h);

    renderer->rendering_pixbuf = renderer->writing_pixbuf;
    renderer->writing_pixbuf   = (renderer->writing_pixbuf + 1) % PIXBUF_COUNT;

    if (renderer->resize_pixbuf_requests & (1 << renderer->writing_pixbuf)) {
        bool found = false;

        for (uint8_t i = 0; i < PIXBUF_COUNT; i++) {
            if (!(renderer->resize_pixbuf_requests & (1 << i))) {
                renderer->writing_pixbuf = i;
                found                    = true;
                break;
            }
        }

        if (!found) {
            printf("------------> NO PIXBUF AVAILABLE: RESIZING IN PROGRESS <------------\n");
            return NULL;
        }
    }

    return renderer->pixbufs[renderer->writing_pixbuf];
}

glrenderer_obj_id_t glrenderer_get_obj_at_coord(glrenderer_t *renderer, uint32_t x, uint32_t y) {
    for (glrenderer_obj_id_t obj_id = 0; obj_id < GLRENDERER_OBJ_ID_END; obj_id++) {
        if (!(renderer->visible_btns_mask & (1 << obj_id)))
            continue;
        if (x < renderer->obj_coords[obj_id].x || x > renderer->obj_coords[obj_id].x + renderer->obj_coords[obj_id].w || y < renderer->obj_coords[obj_id].y || y > renderer->obj_coords[obj_id].y + renderer->obj_coords[obj_id].h)
            continue;
        return obj_id;
    }

    return GLRENDERER_OBJ_ID_SCREEN;
}

void glrenderer_resize_viewport(glrenderer_t *renderer, GLsizei width, GLsizei height) {
    if (!renderer)
        return;

    renderer->resize_viewport_requested = true;

    renderer->viewport_w = width;
    renderer->viewport_h = height;

    // fit screen_tex inside viewport while keeping aspect ratio
    GLfloat image_ratio    = (GLfloat) renderer->screen_tex_w / (GLfloat) renderer->screen_tex_h;
    GLfloat viewport_ratio = (GLfloat) renderer->viewport_w / (GLfloat) renderer->viewport_h;

    if (image_ratio > viewport_ratio) {
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].w = renderer->viewport_w;
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h = renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].w / image_ratio;
    } else {
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h = renderer->viewport_h;
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].w = renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h * image_ratio;
    }

    renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].x = (renderer->viewport_w - renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].w) / 2.0;
    if (renderer->visible_btns_mask)
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].y = 0;
    else
        renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].y = (renderer->viewport_h - renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h) / 2.0;

    if (renderer->visible_btns_mask) {
        GLfloat btn_scale = renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].w / (GLfloat) renderer->screen_tex_w;

        renderer->obj_coords[GLRENDERER_OBJ_ID_SELECT] = (rect_t){
            .x = 1.25f * (btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].w * btn_scale),
            .y = renderer->viewport_h - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_SELECT].h * btn_scale
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_START] = (rect_t){
            .x = renderer->viewport_w - 2.25f * (btn_atlas_regions[GLRENDERER_OBJ_ID_START].w * btn_scale),
            .y = renderer->viewport_h - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_START].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_START].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_START].h * btn_scale
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_R] = (rect_t){
            .x = renderer->viewport_w - 1.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_R].w * btn_scale),
            .y = (renderer->viewport_h - renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h) + (btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_R].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_R].h * btn_scale
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_L] = (rect_t){
            .x = 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_L].w * btn_scale),
            .y = (renderer->viewport_h - renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h) + (btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_L].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_L].h * btn_scale
        };

        rect_t dpad_rect = {
            .x = btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].w * btn_scale,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_SELECT].y - 4.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_DPAD_CENTER].h * btn_scale
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP_LEFT] = (rect_t){
            .x = dpad_rect.x,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_LEFT] = (rect_t){
            .x = dpad_rect.x,
            .y = dpad_rect.y + renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP_LEFT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_LEFT].x + dpad_rect.w,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_LEFT].y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x + dpad_rect.w,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_DOWN] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].y + renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP_RIGHT] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP].x + renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP].w,
            .y = dpad_rect.y,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_DOWN_LEFT] = (rect_t){
            .x = dpad_rect.x,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].y + renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_DOWN_RIGHT] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_CENTER].x + dpad_rect.w,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].y + renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_RIGHT].h,
            .w = dpad_rect.w,
            .h = dpad_rect.h
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_A] = (rect_t){
            .x = renderer->viewport_w - 2.0f * (btn_atlas_regions[GLRENDERER_OBJ_ID_A].w * btn_scale),
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_UP].y + 0.8f * btn_atlas_regions[GLRENDERER_OBJ_ID_A].w,
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_A].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_A].h * btn_scale
        };
        renderer->obj_coords[GLRENDERER_OBJ_ID_B] = (rect_t){
            .x = renderer->obj_coords[GLRENDERER_OBJ_ID_A].x - btn_atlas_regions[GLRENDERER_OBJ_ID_B].w * btn_scale,
            .y = renderer->obj_coords[GLRENDERER_OBJ_ID_DPAD_DOWN].y - 0.8f * btn_atlas_regions[GLRENDERER_OBJ_ID_B].w,
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_B].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_B].h * btn_scale
        };

        renderer->obj_coords[GLRENDERER_OBJ_ID_LINK] = (rect_t){
            .x = 0.5f * renderer->viewport_w - 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].w * btn_scale),
            .y = (renderer->viewport_h - renderer->obj_coords[GLRENDERER_OBJ_ID_SCREEN].h) + 0.5f * (btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].h * btn_scale),
            .w = btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].w * btn_scale,
            .h = btn_atlas_regions[GLRENDERER_OBJ_ID_LINK].h * btn_scale
        };
    }
}

void glrenderer_set_obj_tint(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat tint) {
    if (!renderer || obj_id < 0 || !(renderer->visible_btns_mask & (1 << obj_id)) || tint < 0.0f || tint > 1.0f)
        return;

    renderer->update_obj_requests |= (1 << obj_id);

    renderer->tints[obj_id] = tint;
}

void glrenderer_set_obj_alpha(glrenderer_t *renderer, glrenderer_obj_id_t obj_id, GLfloat alpha) {
    if (!renderer || obj_id < 0 || !(renderer->visible_btns_mask & (1 << obj_id)) || alpha < 0.0f || alpha > 1.0f)
        return;

    renderer->update_obj_requests |= (1 << obj_id);

    renderer->alphas[obj_id] = alpha;
}

void glrenderer_set_show_buttons(glrenderer_t *renderer, uint32_t visible_btns_mask) {
    if (!renderer)
        return;

    renderer->visible_btns_mask = visible_btns_mask;

    if (renderer->visible_btns_mask)
        create_buttons(renderer);

    if (renderer->visible_btns_mask) {
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
