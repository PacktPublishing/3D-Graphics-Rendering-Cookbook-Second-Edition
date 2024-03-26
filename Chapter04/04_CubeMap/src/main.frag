//

#include <Chapter04/04_CubeMap/src/common.sp>

layout (location=0) in PerVertex vtx;

layout (location=0) out vec4 out_FragColor;

void main() {
	vec3 n = normalize(vtx.worldNormal);
	vec3 v = normalize(pc.cameraPos.xyz - vtx.worldPos);
	vec3 reflection = -normalize(reflect(v, n));

	vec4 colorRefl = textureBindlessCube(pc.texCube, 0, reflection);
	vec4 Ka = colorRefl * 0.3;

	float NdotL = clamp(dot(n, normalize(vec3(0,0,-1))), 0.1, 1.0);
	vec4 Kd = textureBindless2D(pc.tex, 0, vtx.uv) * NdotL;

	out_FragColor = Ka + Kd;
};
