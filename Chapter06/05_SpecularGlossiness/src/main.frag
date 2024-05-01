#include <Chapter06/05_SpecularGlossiness/src/inputs.frag>
#include <Chapter06/05_SpecularGlossiness/src/PBR.sp>

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

  vec4 Kao = sampleAO(tc, getMaterialId());
  vec4 Ke  = sampleEmissive(tc, getMaterialId());
  vec4 Kd  = sampleAlbedo(tc, getMaterialId()) * color;
  vec4 mrSample = sampleMetallicRoughness(tc, getMaterialId());

  // world-space normal
  vec3 n = normalize(normal);

  vec3 normalSample = sampleNormal(tc, getMaterialId()).xyz;

  n = perturbNormal(n, worldPos, normalSample, getNormalUV(tc, getMaterialId()));

    if (gl_FrontFacing == false)
    {
    n *= -1.0f;
  }

  PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(Kd, n, perFrame.drawable.cameraPos.xyz, worldPos, mrSample);

  vec3 specular_color = getIBLRadianceContributionGGX(pbrInputs, 1.0);
  vec3 diffuse_color = getIBLRadianceLambertian(pbrInputs.NdotV, n, pbrInputs.perceptualRoughness, pbrInputs.diffuseColor, pbrInputs.reflectance0, 1.0);

  // one hardcoded light source
  vec3 color = specular_color + diffuse_color;

  vec3 lightPos = vec3(0, 0, -5);
  color += calculatePBRLightContribution( pbrInputs, normalize(lightPos - worldPos), vec3(1.0) );
  // ambient occlusion
  color = color * ( Kao.r < 0.01 ? 1.0 : Kao.r );
  // emissive
  color = pow( Ke.rgb + color, vec3(1.0/2.2) );

  out_FragColor = vec4(color, 1.0);

//  out_FragColor = vec4((n + vec3(1.0))*0.5, 1.0);
//  out_FragColor = Kao;
//  out_FragColor = Ke;
//  out_FragColor = Kd;
//  vec2 MeR = mrSample.yz;
//  MeR.x *= getMetallicFactor(getMaterialId());
//  MeR.y *= getRoughnessFactor(getMaterialId());
//  out_FragColor = vec4(MeR.y,MeR.y,MeR.y, 1.0);
//  out_FragColor = vec4(MeR.x,MeR.x,MeR.x, 1.0);
//  out_FragColor = mrSample;
};
