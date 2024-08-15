//

layout (location=0) out vec4 oUV0UV1;
layout (location=1) out vec3 oNormal;
layout (location=2) out vec3 oWorldPos;
layout (location=3) out vec4 oColor;
layout (location=4) out flat int oBaseInstance;

#include <data/shaders/gltf/inputs.vert>

void main() {
  mat4 model = getModel();
  mat4 MVP = getViewProjection() * model;

  vec3 pos = getPosition();
  gl_Position = MVP * vec4(pos, 1.0);

  oUV0UV1 = vec4(getTexCoord(0), getTexCoord(1));
  oColor = getColor();

  mat3 normalMatrix = transpose( inverse(mat3(model)) );

  oNormal = normalMatrix  * getNormal();
  vec4 posClip = model * vec4(pos, 1.0);
  oWorldPos = posClip.xyz/posClip.w;

  oBaseInstance = gl_BaseInstance;
}
