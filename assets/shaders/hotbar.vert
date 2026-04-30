#version 450

layout(push_constant) uniform Push {
    vec4 colors[6];
    int selectedSlot;
} pc;

layout(location = 0) out vec4 fragColor;

const float kSlotW  = 0.075;
const float kSlotH  = 0.135;
const float kGap    = 0.012;
const float kBottom = 0.95;
const float kTop    = kBottom - kSlotH;

void main() {
    int slot   = gl_VertexIndex / 6;
    int corner = gl_VertexIndex % 6;

    float totalW = 6.0 * kSlotW + 5.0 * kGap;
    float startX = -totalW * 0.5;
    float x0 = startX + float(slot) * (kSlotW + kGap);
    float x1 = x0 + kSlotW;
    float y0 = kTop;
    float y1 = kBottom;

    bool selected = (slot == pc.selectedSlot);
    if (selected) {
        x0 -= 0.008;
        x1 += 0.008;
        y0 -= 0.020;
    }

    vec2 p;
    if      (corner == 0) p = vec2(x0, y0);
    else if (corner == 1) p = vec2(x0, y1);
    else if (corner == 2) p = vec2(x1, y1);
    else if (corner == 3) p = vec2(x0, y0);
    else if (corner == 4) p = vec2(x1, y1);
    else                  p = vec2(x1, y0);

    gl_Position = vec4(p, 0.0, 1.0);

    vec3 c = pc.colors[slot].rgb;
    if (selected) c = mix(c, vec3(1.0), 0.40);
    fragColor = vec4(c, 1.0);
}
