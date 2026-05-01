#version 450

layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 color;
} pc;

layout(location = 0) out vec4 fragColor;

const int kIndices[24] = int[](
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7
);

const vec3 kCorners[8] = vec3[](
    vec3(0.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0),
    vec3(1.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 1.0), vec3(0.0, 1.0, 1.0)
);

void main() {
    int corner = kIndices[gl_VertexIndex];
    vec3 pos = kCorners[corner];
    gl_Position = pc.mvp * vec4(pos, 1.0);
    fragColor = pc.color;
}
