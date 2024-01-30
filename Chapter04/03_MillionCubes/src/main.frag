//
layout (location=0) in vec3 color;
layout (location=1) in vec2 uv;

layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PerFrameData {
	mat4 proj;
	uint textureId;
};

void main() {
	out_FragColor = textureBindless2D(textureId, 0, uv) * vec4(color, 1.0);
};
