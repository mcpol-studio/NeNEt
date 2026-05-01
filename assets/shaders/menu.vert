#version 450

layout(push_constant) uniform Push {
    vec4 colors[3];
    int hoverIndex;
} pc;

layout(location = 0) out vec4 fragColor;

const vec4 kRects[3] = vec4[](
    vec4(-0.30f, -0.13f, 0.60f, 0.10f),
    vec4(-0.30f,  0.01f, 0.60f, 0.10f),
    vec4(-0.30f,  0.15f, 0.60f, 0.10f)
);

void main() {
    int btn = gl_VertexIndex / 6;
    int corner = gl_VertexIndex % 6;

    vec4 r = kRects[btn];
    float x0 = r.x;
    float x1 = r.x + r.z;
    float y0 = r.y;
    float y1 = r.y + r.w;

    bool hover = (btn == pc.hoverIndex);
    if (hover) {
        x0 -= 0.012;
        x1 += 0.012;
        y0 -= 0.010;
        y1 += 0.010;
    }

    vec2 p;
    if      (corner == 0) p = vec2(x0, y0);
    else if (corner == 1) p = vec2(x0, y1);
    else if (corner == 2) p = vec2(x1, y1);
    else if (corner == 3) p = vec2(x0, y0);
    else if (corner == 4) p = vec2(x1, y1);
    else                  p = vec2(x1, y0);

    gl_Position = vec4(p, 0.0, 1.0);

    vec3 c = pc.colors[btn].rgb;
    if (hover) c = mix(c, vec3(1.0), 0.40);
    fragColor = vec4(c, 1.0);
}
