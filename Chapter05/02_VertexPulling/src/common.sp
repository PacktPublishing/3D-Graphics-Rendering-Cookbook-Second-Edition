//

struct Vertex {
	float x, y, z;
	float u, v;
};

layout(std430, buffer_reference) readonly buffer Vertices {
	Vertex in_Vertices[];
};

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
	Vertices vtx;
	uint texture;
};
