#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define CSLUG_IMPLEMENTATION
#include "cslug.h"

#define VERTS_PER_GLYPH 6
#define MAX_GLYPHS 128
#define MAX_TEXT 1024

typedef struct {
    cslug_glyph glyphs[MAX_GLYPHS];
    float ascent, descent;

    unsigned int curve_buf_id;
    unsigned int band_buf_id;
    Shader shader;
    unsigned int vao_id;
    unsigned int vbo_id;

    int loc_transform;
    int loc_screen_size;
    int loc_curve_tex;
    int loc_band_tex;
} font_t;

typedef struct {
  float pos_x, pos_y, pos_z, pos_w;
  float tex_x, tex_y, glyph_loc, glyph_info;
  float jac_00, jac_01, jac_10, jac_11;
  float band_scale_x, band_scale_y, band_offset_x, band_offset_y;
  float r, g, b, a;
} vertex_t;

font_t load_font(const char *ttf_path) {
    font_t font = {0};

    FILE *f = fopen(ttf_path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", ttf_path); return font; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ttf = malloc(sz);
    if (fread(ttf, 1, sz, f) != (size_t)sz) {
        fprintf(stderr, "failed to read %s\n", ttf_path);
        free(ttf);
        fclose(f);
        return font;
    }
    fclose(f);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, 0)) {
        fprintf(stderr, "failed to init font\n");
        free(ttf);
        return font;
    }

    float em_scale = stbtt_ScaleForMappingEmToPixels(&info, 1.0f);

    int asc, desc, lg;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &lg);
    font.ascent  = asc * em_scale;
    font.descent = desc * em_scale;

    cslug_buffers_packed buffers = {0};

    for (int cp = 0; cp < MAX_GLYPHS; cp++) {
        font.glyphs[cp].advance = 0;
        if (' ' <= cp && cp <= '~') {
            cslug_build_glyph_for_buffer(&info, cp, em_scale, &buffers, &font.glyphs[cp]);
        }
    }

    free(ttf);

    font.curve_buf_id = rlLoadShaderBuffer(buffers.curves.len * sizeof(cslug_f16), buffers.curves.ptr,
                                           RL_STATIC_DRAW);
    font.band_buf_id  = rlLoadShaderBuffer(buffers.bands.len * sizeof(cslug_u16), buffers.bands.ptr,
                                           RL_STATIC_DRAW);

    cslug_free_buffers_packed(&info, &buffers);

    font.shader = LoadShader("cslug_buffer_vert.glsl", "cslug_buffer_frag.glsl");
    font.loc_transform   = GetShaderLocation(font.shader, "transform");
    font.loc_screen_size = GetShaderLocation(font.shader, "screen_size");
    font.loc_curve_tex   = GetShaderLocation(font.shader, "curve_texture");
    font.loc_band_tex    = GetShaderLocation(font.shader, "band_texture");

    int max_verts = MAX_TEXT * VERTS_PER_GLYPH;
    int buf_size = max_verts * sizeof(vertex_t);
    int stride = sizeof(vertex_t);

    font.vao_id = rlLoadVertexArray();
    rlEnableVertexArray(font.vao_id);
    font.vbo_id = rlLoadVertexBuffer(NULL, buf_size, true);
    for (int i = 0; i < 5; i++) {
        rlSetVertexAttribute(i, 4, RL_FLOAT, false, stride, i * 4 * (int)sizeof(float));
        rlEnableVertexAttribute(i);
    }
    rlDisableVertexArray();

    return font;
}

void free_font(font_t *font) {
    rlUnloadVertexBuffer(font->vbo_id);
    rlUnloadVertexArray(font->vao_id);
    rlUnloadShaderBuffer(font->curve_buf_id);
    rlUnloadShaderBuffer(font->band_buf_id);
    UnloadShader(font->shader);
}

void draw_text(font_t *font, const char *text, Vector3 pos, float size, Color color, Matrix view_proj) {
    int len = strlen(text);
    if (len == 0 || MAX_TEXT < len) return;

    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    float n = 0.70710678f; // 1/sqrt(2)

    vertex_t vertices[MAX_TEXT * VERTS_PER_GLYPH];
    int vertex_count = 0;
    float cursor = 0.0f; // em-space cursor

    for (int i = 0; i < len; i++) {
        int ch = text[i];
        if (MAX_GLYPHS <= ch) continue;
        cslug_glyph *gl = &font->glyphs[ch];

        // em-space
        float ex0 = gl->x0;
        float ey0 = gl->y0;
        float ex1 = gl->x1;
        float ey1 = gl->y1;

        // object-space
        float ox0 = cursor + ex0;
        float ox1 = cursor + ex1;
        float oy0 = ey0;
        float oy1 = ey1;

        uint32_t packed_loc  = (uint32_t)gl->glyph_band_texel;
        uint32_t packed_info = ((uint32_t)gl->band_max_y << 16) | (uint32_t)gl->band_max_x;

        struct { float ox,oy,ex,ey,nx,ny; } corners[4] = {
            { ox0, oy1, ex0, ey1, -n,  n }, // TL
            { ox1, oy1, ex1, ey1,  n,  n }, // TR
            { ox1, oy0, ex1, ey0,  n, -n }, // BR
            { ox0, oy0, ex0, ey0, -n, -n }, // BL
        };
        int tri[6] = { 0, 3, 1, 1, 3, 2 };

        for (int vi = 0; vi < 6; vi++) {
            int ci = tri[vi];
            vertex_t *v = vertices + vertex_count;
            v->pos_x = corners[ci].ox;
            v->pos_y = corners[ci].oy;
            v->pos_z = corners[ci].nx;
            v->pos_w = corners[ci].ny;
            v->tex_x = corners[ci].ex;
            v->tex_y = corners[ci].ey;
            v->glyph_loc  = *(float*)(&packed_loc);
            v->glyph_info = *(float*)(&packed_info);
            v->jac_00 = 1.0f; v->jac_01 = 0.0f;
            v->jac_10 = 0.0f; v->jac_11 = 1.0f;
            v->band_scale_x = gl->band_scale_x;
            v->band_scale_y = gl->band_scale_y;
            v->band_offset_x = gl->band_offset_x;
            v->band_offset_y = gl->band_offset_y;
            v->r = r;  v->g = g;  v->b = b;  v->a = a;
            vertex_count++;
        }
        cursor += gl->advance;
    }

    Matrix model = MatrixMultiply(MatrixTranslate(pos.x, pos.y, pos.z), MatrixScale(size, size, size));
    Matrix mvp = MatrixMultiply(model, view_proj);
    float screen_size[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };

    rlEnableShader(font->shader.id);
    rlSetUniform(font->loc_transform, &mvp, RL_SHADER_UNIFORM_VEC4, 4);
    rlSetUniform(font->loc_screen_size, screen_size, RL_SHADER_UNIFORM_VEC2, 1);

    int slot0 = 0, slot1 = 1;
    rlSetUniform(font->loc_curve_tex, &slot0, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(font->loc_band_tex,  &slot1, RL_SHADER_UNIFORM_INT, 1);

    rlBindShaderBuffer(font->curve_buf_id, 2);
    rlBindShaderBuffer(font->band_buf_id,  3);

    rlUpdateVertexBuffer(font->vbo_id, vertices, vertex_count * sizeof(vertex_t), 0);
    rlEnableVertexArray(font->vao_id);
    rlDrawVertexArray(0, vertex_count);
    rlDisableVertexArray();

    rlActiveTextureSlot(1);
    rlDisableTexture();
    rlActiveTextureSlot(0);
    rlDisableTexture();
    rlDisableShader();
}

int main(void) {
    InitWindow(800, 600, "Slug Text Rendering");
    SetTargetFPS(60);

    font_t font = load_font("NotoSans-Regular.ttf");

    Camera3D camera = {
        .position = { 3.0f, 1.5f, 3.0f },
        .target   = { 1.5f, 0.2f, 0.0f },
        .up       = { 0.0f, 1.0f, 0.0f },
        .fovy     = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    while (!WindowShouldClose()) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            UpdateCamera(&camera, CAMERA_FREE);
        }

        Matrix view = GetCameraMatrix(camera);
        float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
        Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01f, 100.0f);
        Matrix view_proj = MatrixMultiply(view, proj);

        BeginDrawing();
        ClearBackground((Color){ 30, 30, 30, 255 });

        draw_text(&font, "Hello, Slug!",
                  (Vector3){ 0, 0.5f, -0.5 }, 0.5f, WHITE, view_proj);

        draw_text(&font, "GPU vector text",
                  (Vector3){ 0, 0, 0 }, 0.3f, YELLOW, view_proj);

        draw_text(&font, "3D perspective rendering",
                  (Vector3){ 0, -0.4f, 0.4 }, 0.2f, GREEN, view_proj);

        draw_text(&font, "abcdefghijklmnopqrstuvwxyz",
                  (Vector3){ 0, -0.8f, 0.8 }, 0.15f, (Color){ 100, 180, 255, 255 }, view_proj);

        DrawFPS(10, 10);
        EndDrawing();
    }

    free_font(&font);
    CloseWindow();
    return 0;
}
