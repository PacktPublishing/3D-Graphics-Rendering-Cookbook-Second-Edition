//
#include <Chapter04/09_ComputeMesh/src/common.sp>

layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 barycoords;

layout (location=0) out vec4 out_FragColor;

layout (constant_id = 0) const bool isColored = false;

float edgeFactor(float thickness) {
	vec3 a3 = smoothstep( vec3( 0.0 ), fwidth(barycoords) * thickness, barycoords);
	return min( min( a3.x, a3.y ), a3.z );
}

vec3 hue2rgb(float hue)
{
  float h = fract(hue);
  float r = abs(h * 6 - 3) - 1;
  float g = 2 - abs(h * 6 - 2);
  float b = 2 - abs(h * 6 - 4);
  return clamp(vec3(r,g,b), vec3(0), vec3(1));
}

void main() {
  float NdotL = dot(normalize(normal), normalize(vec3(0, 0, +1)));

  float intensity = 1.0 * clamp(NdotL, 0.75, 1);
  vec3 color = isColored ? intensity * hue2rgb(uv.x) : textureBindless2D(pc.textureId, 0, vec2(8,1) * uv).xyz;

  out_FragColor = vec4(color, 1.0);

  if (isColored && pc.numU <= 64 && pc.numV <= 64) {
    out_FragColor = vec4( mix( vec3(0.0), color, edgeFactor(1.0) ), 1.0 );
  }
}
