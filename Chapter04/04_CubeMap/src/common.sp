//

layout(std430, buffer_reference) readonly buffer PerFrameData {
	mat4 model;
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
	uint tex;
	uint texCube;
};

layout(push_constant) uniform PushConstants {
	PerFrameData pc;
};

struct PerVertex {
	vec2 uv;
	vec3 worldNormal;
	vec3 worldPos;
};
