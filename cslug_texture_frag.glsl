#version 330

     in vec4  v_color;
     in vec2  v_texcoord;
flat in vec4  v_banding;
flat in uvec4 v_glyph;

uniform sampler2D curve_texture;
uniform sampler2D band_texture;

out vec4 frag_color;

uint calculate_root_code(float y1, float y2, float y3) {
    uint i1 = floatBitsToUint(y1) >> 31u;
    uint i2 = floatBitsToUint(y2) >> 30u;
    uint i3 = floatBitsToUint(y3) >> 29u;

    uint shift = (i2 & 2u) | (i1 & ~2u);
    shift = (i3 & 4u) | (shift & ~4u);

    return ((0x2E74u >> shift) & 0x0101u);
}

vec2 solve_horizontal_poly(vec4 p12, vec2 p3) {
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.y;
    float rb = 0.5 / b.y;

    float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
    float t1 = (b.y - d) * ra;
    float t2 = (b.y + d) * ra;

    if (abs(a.y) < 1.0 / 65536.0) {
        t1 = p12.y * rb;
        t2 = t1;
    }

    return vec2((a.x * t1 - b.x * 2.0) * t1 + p12.x,
                (a.x * t2 - b.x * 2.0) * t2 + p12.x);
}

vec2 solve_vertical_poly(vec4 p12, vec2 p3) {
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.x;
    float rb = 0.5 / b.x;

    float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
    float t1 = (b.x - d) * ra;
    float t2 = (b.x + d) * ra;

    if (abs(a.x) < 1.0 / 65536.0) {
        t1 = p12.x * rb;
        t2 = t1;
    }

    return vec2((a.y * t1 - b.y * 2.0) * t1 + p12.y,
                (a.y * t2 - b.y * 2.0) * t2 + p12.y);
}

uvec2 calculate_band_loc(uvec2 glyph_loc, uint offset) {
    return uvec2(glyph_loc.x + offset, glyph_loc.y);
}

float calculate_coverage(float xcov, float ycov, float xwgt, float ywgt) {
    float coverage = max(
        abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0),
        min(abs(xcov), abs(ycov))
    );
    coverage = clamp(coverage, 0.0, 1.0);
    return coverage;
}

void main() {
    vec2 em_per_pixel = fwidth(v_texcoord);
    vec2 pixels_per_em = 1.0 / em_per_pixel;

    uvec2 bandMax = v_glyph.zw;
    bandMax.y &= uint(0x00FF);

    uvec2 band_index = clamp(uvec2(v_texcoord * v_banding.xy + v_banding.zw),
                            uvec2(0), bandMax);
    uvec2 glyph_loc = v_glyph.xy;

    float xcov = 0.0;
    float xwgt = 0.0;

    uvec2 hband_data = uvec2(texelFetch(band_texture, ivec2(glyph_loc.x + band_index.y, glyph_loc.y), 0).xy);
    uvec2 hband_loc  = calculate_band_loc(glyph_loc, hband_data.y);

    for (uint curve_index = uint(0); curve_index < hband_data.x; curve_index++) {
        ivec2 curve_loc = ivec2(texelFetch(band_texture, ivec2(hband_loc.x + curve_index, hband_loc.y), 0).xy);

        vec4 p12 = texelFetch(curve_texture, curve_loc, 0) - vec4(v_texcoord, v_texcoord);
        vec2 p3  = texelFetch(curve_texture, ivec2(curve_loc.x + 1, curve_loc.y), 0).xy - v_texcoord;

        if (max(max(p12.x, p12.z), p3.x) * pixels_per_em.x < -0.5) break;

        uint code = calculate_root_code(p12.y, p12.w, p3.y);
        if (code != 0u) {
            vec2 r = solve_horizontal_poly(p12, p3) * pixels_per_em.x;

            if ((code & 1u) != 0u) {
                xcov += clamp(r.x + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1u) {
                xcov -= clamp(r.y + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    float ycov = 0.0;
    float ywgt = 0.0;

    uvec2 vband_data = uvec2(
        texelFetch(band_texture, ivec2(glyph_loc.x + bandMax.y + uint(1) + band_index.x, glyph_loc.y), 0).xy
    );
    uvec2 vband_loc = calculate_band_loc(glyph_loc, vband_data.y);

    for (uint curve_index = uint(0); curve_index < vband_data.x; curve_index++) {
        ivec2 curve_loc = ivec2(texelFetch(band_texture, ivec2(vband_loc.x + curve_index, vband_loc.y), 0).xy);
        vec4 p12 = texelFetch(curve_texture, curve_loc, 0) - vec4(v_texcoord, v_texcoord);
        vec2 p3  = texelFetch(curve_texture, ivec2(curve_loc.x + 1, curve_loc.y), 0).xy - v_texcoord;

        if (max(max(p12.y, p12.w), p3.y) * pixels_per_em.y < -0.5) break;

        uint code = calculate_root_code(p12.x, p12.z, p3.x);
        if (code != 0u) {
            vec2 r = solve_vertical_poly(p12, p3) * pixels_per_em.y;

            if ((code & 1u) != 0u) {
                ycov -= clamp(r.x + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1u) {
                ycov += clamp(r.y + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    float coverage = calculate_coverage(xcov, ycov, xwgt, ywgt);
    frag_color = v_color * coverage;
}
