#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 texCoord;

layout(set = 2, binding = 0) uniform sampler2D rightSampler;
layout(set = 2, binding = 1) uniform sampler2D leftSampler;
layout(set = 2, binding = 2) uniform sampler2D topSampler;
layout(set = 2, binding = 3) uniform sampler2D botSampler;
layout(set = 2, binding = 4) uniform sampler2D frontSampler;
layout(set = 2, binding = 5) uniform sampler2D backSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 newtextcoord;
    int side = int(round(texCoord.w));

	if(side == 2)
    {
        newtextcoord = (texCoord.zy) / 20.0 + 0.5;
        newtextcoord = vec2(1.0 - newtextcoord.x, newtextcoord.y);
        outColor = texture(rightSampler, newtextcoord);
    }
    else if (side == 1)
    {
        newtextcoord = (texCoord.zy) / 20.0 + 0.5;
        newtextcoord = vec2(newtextcoord.x, newtextcoord.y);
        outColor = texture(leftSampler, newtextcoord);
    }
    else if (side == 3)
    {
        newtextcoord = (texCoord.xz) / 20.0 + 0.5;
        newtextcoord = vec2(1.0 - newtextcoord.x, newtextcoord.y);
        outColor = texture(botSampler, newtextcoord);
    }
    else if (side == 6)
    {
        newtextcoord = (texCoord.xy) / 20.0 + 0.5;
        newtextcoord = vec2(newtextcoord.x, newtextcoord.y);
        outColor = texture(frontSampler, newtextcoord);
    }
    else if (side == 5)
    {
        newtextcoord = (texCoord.xy) / 20.0 + 0.5;
        newtextcoord = vec2(1.0 - newtextcoord.x, newtextcoord.y);
        outColor = texture(backSampler, newtextcoord);
    }
	else
    {
        newtextcoord = (texCoord.xz) / 20.0 + 0.5;
        newtextcoord = vec2(1.0 - newtextcoord.x, newtextcoord.y);
        outColor = texture(topSampler, newtextcoord);
    }
}