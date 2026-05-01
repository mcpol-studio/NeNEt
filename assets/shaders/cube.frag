#version 450

layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) in vec2 fragAtlasOrigin;
layout(location = 3) in vec4 fragTint;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 localUv = fract(fragUv);
    vec2 atlasUv = fragAtlasOrigin + localUv * (1.0 / 64.0);
    vec4 tex = texture(atlas, atlasUv);

    if (tex.a < 0.5) discard;

    vec3 c = tex.rgb * fragColor;
    c = mix(c, fragTint.rgb, fragTint.a);
    outColor = vec4(c, 1.0);
}
