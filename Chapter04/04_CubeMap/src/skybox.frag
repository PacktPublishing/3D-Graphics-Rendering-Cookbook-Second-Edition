//

#include <Chapter04/04_CubeMap/src/common.sp>

layout (location=0) in vec3 dir;

layout (location=0) out vec4 out_FragColor;

void main() {
	out_FragColor = textureBindlessCube(pc.texCube, 0, dir);
};
