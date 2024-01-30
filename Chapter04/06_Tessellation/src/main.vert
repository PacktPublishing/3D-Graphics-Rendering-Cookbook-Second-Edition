//

#include <Chapter04/06_Tessellation/src/common.sp>

vec3 getPosition(int i) {
	return vec3(pc.vtx.in_Vertices[i].x, pc.vtx.in_Vertices[i].y, pc.vtx.in_Vertices[i].z);
}

vec2 getTexCoord(int i) {
	return vec2(pc.vtx.in_Vertices[i].u, pc.vtx.in_Vertices[i].v);
}

layout (location=0) out vec2 uv_in;
layout (location=1) out vec3 worldPos_in;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	vec4 pos = vec4(getPosition(gl_VertexIndex), 1.0);
	gl_Position = pc.proj * pc.view * pc.model * pos;

	uv_in = getTexCoord(gl_VertexIndex);
	worldPos_in = (pc.model * pos).xyz;
}
