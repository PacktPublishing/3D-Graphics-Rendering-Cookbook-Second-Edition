//

layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PerFrameData {
	uint textureId;
};

void main() {
	out_FragColor = textureBindless2D(textureId, 0, uv);
};
