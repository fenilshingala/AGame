#version 450
#extension GL_ARB_separate_shader_objects : enable

// IN
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// OUT
layout(location = 0) out vec2 texCoord;

// UNIFORM
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
    texCoord = inTexCoord;
}