#version 450
#extension GL_ARB_separate_shader_objects : enable

// IN
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// OUT
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;

// UNIFORM
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    vec3 in_fragColor;
    float pad0;
} ubo;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragColor = ubo.in_fragColor;
    texCoord = inTexCoord;
}