#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec2 inAtlasOrigin;

layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 tint;
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUv;
layout(location = 2) out vec2 fragAtlasOrigin;
layout(location = 3) out vec4 fragTint;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragUv = inUv;
    fragAtlasOrigin = inAtlasOrigin;
    fragTint = pc.tint;
}
