#version 450

layout(set = 0, binding = 0) uniform sampler2D wallpaper;

layout(push_constant) uniform Push {
    vec2 viewport;
    vec2 imageSize;
    float glassMix;
    float vignette;
    float blurRadius;
    float _pad;
} pc;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

vec2 coverUv(vec2 uv) {
    float vAspect = pc.viewport.x / max(pc.viewport.y, 1.0);
    float iAspect = pc.imageSize.x / max(pc.imageSize.y, 1.0);
    vec2 scale = vec2(1.0);
    if (vAspect > iAspect) {
        scale.y = iAspect / vAspect;
    } else {
        scale.x = vAspect / iAspect;
    }
    return (uv - 0.5) * scale + 0.5;
}

vec3 blur9(sampler2D tex, vec2 uv, vec2 step) {
    vec3 sum = vec3(0.0);
    sum += texture(tex, uv + vec2(-step.x, -step.y)).rgb * 0.0625;
    sum += texture(tex, uv + vec2( 0.0,    -step.y)).rgb * 0.125;
    sum += texture(tex, uv + vec2( step.x, -step.y)).rgb * 0.0625;
    sum += texture(tex, uv + vec2(-step.x,  0.0   )).rgb * 0.125;
    sum += texture(tex, uv                          ).rgb * 0.25;
    sum += texture(tex, uv + vec2( step.x,  0.0   )).rgb * 0.125;
    sum += texture(tex, uv + vec2(-step.x,  step.y)).rgb * 0.0625;
    sum += texture(tex, uv + vec2( 0.0,     step.y)).rgb * 0.125;
    sum += texture(tex, uv + vec2( step.x,  step.y)).rgb * 0.0625;
    return sum;
}

void main() {
    vec2 uv = coverUv(fragUv);
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    vec2 texel = pc.blurRadius / max(pc.imageSize, vec2(1.0));
    vec3 c0 = blur9(wallpaper, uv,                         texel);
    vec3 c1 = blur9(wallpaper, uv + vec2(0.7,  0.0) * texel, texel);
    vec3 c2 = blur9(wallpaper, uv + vec2(0.0,  0.7) * texel, texel);
    vec3 c3 = blur9(wallpaper, uv - vec2(0.7,  0.7) * texel, texel);
    vec3 blurred = (c0 + c1 + c2 + c3) * 0.25;

    vec3 glass = vec3(0.06, 0.08, 0.13);
    vec3 mixed = mix(blurred, glass, pc.glassMix);

    vec2 d = fragUv - vec2(0.5);
    float v = 1.0 - smoothstep(0.55, 0.95, length(d));
    mixed *= mix(1.0, v, pc.vignette);

    outColor = vec4(mixed, 1.0);
}
