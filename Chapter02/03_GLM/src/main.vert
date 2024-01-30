//
#version 460 core

layout(push_constant) uniform PerFrameData {
	mat4 mvp;
};

layout (constant_id = 0) const bool isWireframe = false;

layout (location=0) out vec3 color;

const vec3 pos[8] = vec3[8](
	vec3(-1.0,-1.0, 1.0), vec3( 1.0,-1.0, 1.0), vec3( 1.0, 1.0, 1.0), vec3(-1.0, 1.0, 1.0),
	vec3(-1.0,-1.0,-1.0), vec3( 1.0,-1.0,-1.0), vec3( 1.0, 1.0,-1.0), vec3(-1.0, 1.0,-1.0)
);

const vec3 col[8] = vec3[8](
	vec3( 1.0, 0.0, 0.0), vec3( 0.0, 1.0, 0.0), vec3( 0.0, 0.0, 1.0), vec3( 1.0, 1.0, 0.0),
	vec3( 1.0, 1.0, 0.0), vec3( 0.0, 0.0, 1.0), vec3( 0.0, 1.0, 0.0), vec3( 1.0, 0.0, 0.0)
);

const uint indices[36] = uint[36](
	0, 1, 2, 2, 3, 0, // front
	1, 5, 6, 6, 2, 1, // right
	7, 6, 5, 5, 4, 7, // back
	4, 0, 3, 3, 7, 4, // left
	4, 5, 1, 1, 0, 4, // bottom
	3, 2, 6, 6, 7, 3 // top
);

void main() {
	uint idx = indices[gl_VertexIndex];
	gl_Position = mvp * vec4(pos[idx], 1.0);
	color = isWireframe ? vec3(0.0) : col[idx];
}
