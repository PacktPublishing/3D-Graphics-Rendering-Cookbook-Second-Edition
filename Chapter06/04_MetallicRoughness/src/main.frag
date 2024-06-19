#include <Chapter06/04_MetallicRoughness/src/inputs.frag>
#include <Chapter06/04_MetallicRoughness/src/PBR.sp>

layout (location=0) in vec4 uv0uv1;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in vec4 color;

layout (location=0) out vec4 out_FragColor;

void main()
{
  InputAttributes tc;
  tc.uv[0] = uv0uv1.xy;
  tc.uv[1] = uv0uv1.zw;

  MetallicRoughnessDataGPU mat = getMaterial(getMaterialId());

  vec4 Kao = sampleAO(tc, mat);
  vec4 Ke  = sampleEmissive(tc, mat);
  vec4 Kd  = sampleAlbedo(tc, mat) * color;
  vec4 mrSample = sampleMetallicRoughness(tc, mat);

  // world-space normal
  vec3 n = normalize(normal);

  vec3 normalSample = sampleNormal(tc, mat).xyz;

  // normal mapping
  n = perturbNormal(n, worldPos, normalSample, getNormalUV(tc, mat));

  if (!gl_FrontFacing) n *= -1.0f;

  PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(Kd, n, perFrame.drawable.cameraPos.xyz, worldPos, mrSample);

  vec3 specular_color = getIBLRadianceContributionGGX(pbrInputs, 1.0);
  vec3 diffuse_color = getIBLRadianceLambertian(pbrInputs.NdotV, n, pbrInputs.perceptualRoughness, pbrInputs.diffuseColor, pbrInputs.reflectance0, 1.0);
  vec3 color = specular_color + diffuse_color;

  // one hardcoded light source
  vec3 lightPos = vec3(0, 0, -5);
  color += calculatePBRLightContribution( pbrInputs, normalize(lightPos - worldPos), vec3(1.0) );
  // ambient occlusion
  color = color * ( Kao.r < 0.01 ? 1.0 : Kao.r );
  // emissive
  color = pow( Ke.rgb + color, vec3(1.0/2.2) );

  out_FragColor = vec4(color, 1.0);

// Uncomment to debug:
//  out_FragColor = vec4((n + vec3(1.0))*0.5, 1.0);
//  out_FragColor = Kao;
//  out_FragColor = Ke;
//  out_FragColor = Kd;
//  vec2 MeR = mrSample.yz;
//  MeR.x *= getMetallicFactor(mat);
//  MeR.y *= getRoughnessFactor(mat);
//  out_FragColor = vec4(MeR.y,MeR.y,MeR.y, 1.0);
//  out_FragColor = mrSample;
};
