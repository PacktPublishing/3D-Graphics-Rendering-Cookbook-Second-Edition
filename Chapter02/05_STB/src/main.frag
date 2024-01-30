//
#version 460 core
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 0, binding = 0) uniform texture2D kTextures2D[];
layout (set = 0, binding = 1) uniform sampler kSamplers[];

layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PerFrameData {
	uniform mat4 MVP;
	uint textureId;
};

vec4 textureBindless2D(uint textureid, uint samplerid, vec2 uv) {
  return texture(sampler2D(kTextures2D[textureid], kSamplers[samplerid]), uv);
}

void main() {
	out_FragColor = textureBindless2D(textureId, 0, uv);
};
