//

#include <Chapter05/04_InstancedMeshes/src/common.sp>

layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;
layout (location=2) out vec3 color;

layout(std430, buffer_reference) readonly buffer Matrices {
	mat4 mtx[];
};

struct Vertex {
	float x, y, z;
	float u, v;
	float nx, ny, nz;
};

layout(std430, buffer_reference) readonly buffer Vertices {
	Vertex in_Vertices[];
};

const vec3 colors[3] = vec3[3](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(1.0, 1.0, 1.0));

void main() {
	Vertex vtx = Vertices(bufVerticesId).in_Vertices[gl_VertexIndex];

	mat4 model = Matrices(bufMatricesId).mtx[gl_InstanceIndex];

   const float scale = 10.0;

	gl_Position = viewproj * model * vec4(scale * vtx.x, scale * vtx.y, scale * vtx.z, 1.0);

	mat3 normalMatrix = transpose( inverse(mat3(model)) );

	uv = vec2(vtx.u, vtx.v);
   normal = normalMatrix * vec3(vtx.nx, vtx.ny, vtx.nz);
   color = colors[gl_InstanceIndex % 3];
}
