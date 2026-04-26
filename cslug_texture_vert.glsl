#version 330

layout(location = 0) in vec4 in_pos;
layout(location = 1) in vec4 in_tex;
layout(location = 2) in vec4 in_jac;
layout(location = 3) in vec4 in_bnd;
layout(location = 4) in vec4 in_col;

uniform vec4 transform[4];
uniform vec2 screen_size;

     out vec4  v_color;
     out vec2  v_texcoord;
flat out vec4  v_banding;
flat out uvec4 v_glyph;

vec2 slug_dialate(vec4 pos, vec4 tex, vec4 jac,
                vec4 m0, vec4 m1, vec4 m3, vec2 dim,
                out vec2 vpos) {
    vec2 n = normalize(pos.zw);
    float s = dot(m3.xy, pos.xy) + m3.w;
    float t = dot(m3.xy, n);

    float u = (s * dot(m0.xy, n) - t * (dot(m0.xy, pos.xy) + m0.w)) * dim.x;
    float v = (s * dot(m1.xy, n) - t * (dot(m1.xy, pos.xy) + m1.w)) * dim.y;

    float s2 = s * s;
    float st = s * t;
    float uv = u * u + v * v;
    vec2 d = pos.zw * (s2 * (st + sqrt(uv)) / (uv - st * st));

    vpos = pos.xy + d;
    return vec2(tex.x + dot(d, jac.xy), tex.y + dot(d, jac.zw));
}

void main() {
    vec2 p;

    v_texcoord = slug_dialate(in_pos, in_tex, in_jac,
                              transform[0], transform[1], transform[3],
                              screen_size, p);

    gl_Position.x = p.x * transform[0].x + p.y * transform[0].y + transform[0].w;
    gl_Position.y = p.x * transform[1].x + p.y * transform[1].y + transform[1].w;
    gl_Position.z = p.x * transform[2].x + p.y * transform[2].y + transform[2].w;
    gl_Position.w = p.x * transform[3].x + p.y * transform[3].y + transform[3].w;

    uvec2 g = floatBitsToUint(in_tex.zw);
    v_glyph = uvec4(g.x & 0xFFFFu, g.x >> 16u, g.y & 0xFFFFu, g.y >> 16u);
    v_banding = in_bnd;
    v_color = in_col;
}
