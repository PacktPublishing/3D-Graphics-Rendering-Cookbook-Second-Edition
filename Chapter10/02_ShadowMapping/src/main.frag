//

#include <Chapter10/02_ShadowMapping/src/common.sp>
#include <data/shaders/Shadow.sp>

layout (location=0) in PerVertex vtx;

layout (location=0) out vec4 out_FragColor;

float spotLightFactor(vec3 worldPos)
{
  vec3 dirLight = normalize(worldPos - pc.perFrame.lightPos.xyz);
  vec3 dirSpot  = normalize(-pc.perFrame.lightPos.xyz); // light is always looking at (0, 0, 0)

  float rho = dot(dirLight, dirSpot);

  float outerAngle = pc.perFrame.lightAngles.x;
  float innerAngle = pc.perFrame.lightAngles.y;

  if (rho > outerAngle)
    return smoothstep(outerAngle, innerAngle, rho);

  return 0.0;
}

void main() {
  vec3 n = normalize(vtx.worldNormal);
  vec3 l = normalize(pc.perFrame.lightPos.xyz);

  float NdotL = clamp(dot(n, l), 0.1, 1.0);

  float Ka = 0.1;
  float Kd = NdotL * shadow(vtx.shadowCoords, pc.perFrame.shadowTexture, pc.perFrame.shadowSampler) * spotLightFactor(vtx.worldPos);

  out_FragColor = textureBindless2D(pc.texture, 0, vtx.uv) * clamp(Ka + Kd, 0.3, 1.0);
}
