#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (set = 0, binding = 0) uniform UniformBufferObject 
{
	mat4 view;
	mat4 projection;
	//vec3 camPos;
} ubo;

layout (set = 1, binding = 0) uniform UBOModelMatrix 
{
	mat4 model;
} uboModel;

#define MAX_NUM_JOINTS 128

layout (set = 2, binding = 0) uniform UBONode {
	mat4 matrix;
	mat4 jointMatrix[MAX_NUM_JOINTS];
	float jointCount;
} node;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	vec4 locPos;
	if (node.jointCount > 0.0) {
		// Mesh is skinned
		mat4 skinMat = 
			inWeight0.x * node.jointMatrix[int(inJoint0.x)] +
			inWeight0.y * node.jointMatrix[int(inJoint0.y)] +
			inWeight0.z * node.jointMatrix[int(inJoint0.z)] +
			inWeight0.w * node.jointMatrix[int(inJoint0.w)];

		locPos = uboModel.model * node.matrix * skinMat * vec4(inPos, 1.0);
		outNormal = normalize(transpose(inverse(mat3(uboModel.model * node.matrix * skinMat))) * inNormal);
	} else {
		locPos = uboModel.model * node.matrix * vec4(inPos, 1.0);
		outNormal = normalize(transpose(inverse(mat3(uboModel.model * node.matrix))) * inNormal);
	}
	locPos.y = -locPos.y;
	outWorldPos = locPos.xyz / locPos.w;
	outUV0 = inUV0;
	outUV1 = inUV1;
	gl_Position =  ubo.projection * ubo.view * vec4(outWorldPos, 1.0);
}
