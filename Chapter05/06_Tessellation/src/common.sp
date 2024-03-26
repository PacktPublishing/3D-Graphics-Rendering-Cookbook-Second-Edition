//

struct Vertex {
	float x, y, z;
	float u, v;
};

layout(std430, buffer_reference) readonly buffer Vertices {
	Vertex in_Vertices[];
};

layout(std430, buffer_reference) readonly buffer PerFrameData {
	mat4 model;
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
	uint texture;
	float tesselationScale;
	Vertices vtx;
};

layout(push_constant) uniform PushConstants {
	PerFrameData pc;
};
