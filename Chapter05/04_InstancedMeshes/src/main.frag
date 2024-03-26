//
layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 color;

layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PerFrameData {
	mat4 viewproj;
	uint textureId;
};

void main() {
	vec3 n = normalize(normal);
	vec3 l = normalize(vec3(1.0, 0.0, 1.0));
	float NdotL = clamp(dot(n, l), 0.3, 1.0);

	out_FragColor = textureBindless2D(textureId, 0, uv) * NdotL * vec4(color, 1.0);
};
