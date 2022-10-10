#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

// UNIFORM
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 projection;
} ubo;

layout (set = 1, binding = 0) uniform UBOModelMatrix 
{
	mat4 model;
} uboModel;

void main() {
    gl_Position = ubo.projection * ubo.view * uboModel.model * vec4(inPosition.x, inPosition.y, inPosition.z, 1.0);
}