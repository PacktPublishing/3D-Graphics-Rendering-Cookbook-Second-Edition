//

#include <Chapter10/02_ShadowMapping/src/common.sp>

layout (location=0) in PerVertex vtx;

layout (location=0) out vec4 out_FragColor;

float PCF3(vec3 uvw) {
  float size = 1.0 / textureBindlessSize2D(pc.perFrame.shadowTexture).x;
  float shadow = 0.0;
  for (int v=-1; v<=+1; v++)
    for (int u=-1; u<=+1; u++)
      shadow += textureBindless2DShadow(pc.perFrame.shadowTexture, pc.perFrame.shadowSampler, uvw + size * vec3(u, v, 0));
  return shadow / 9;
}

float shadow(vec4 s) {
  s = s / s.w;
  if (s.z > -1.0 && s.z < 1.0) {
    float shadowSample = PCF3(vec3(s.x, 1.0 - s.y, s.z + pc.perFrame.depthBias));
    return mix(0.3, 1.0, shadowSample);
  }
  return 1.0;
}

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
  float Kd = NdotL * shadow(vtx.shadowCoords) * spotLightFactor(vtx.worldPos);

  out_FragColor = textureBindless2D(pc.texture, 0, vtx.uv) * clamp(Ka + Kd, 0.3, 1.0);
}
