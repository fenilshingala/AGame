#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec4 texCoord;

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
    vec4 p = ubo.projection * ubo.view * uboModel.model * vec4(inPosition.x*9.0, inPosition.y*9.0, inPosition.z*9.0, 1.0);
    gl_Position = p.xyww;
    texCoord = vec4(inPosition.x, inPosition.y, inPosition.z, inPosition.w);
}