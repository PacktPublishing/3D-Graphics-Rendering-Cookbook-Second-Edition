//

#include <Chapter04/04_CubeMap/src/common.sp>

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location=0) out PerVertex vtx;

void main() {
	gl_Position = pc.proj * pc.view * pc.model * vec4(pos, 1.0);

	mat4 model = pc.model;
	mat3 normalMatrix = transpose( inverse(mat3(pc.model)) );

	vtx.uv = uv;
	vtx.worldNormal = normalMatrix * normal;
	vtx.worldPos = (model * vec4(pos, 1.0)).xyz;
}
