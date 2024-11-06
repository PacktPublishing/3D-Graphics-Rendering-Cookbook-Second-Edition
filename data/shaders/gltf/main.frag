//

layout (location=0) in vec4 uv0uv1;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in vec4 color;
layout (location=4) in flat int oBaseInstance;

layout (location=0) out vec4 out_FragColor;

#include <data/shaders/gltf/inputs.frag>
#include <data/shaders/gltf/PBR.sp>

void main()
{
  InputAttributes tc;
  tc.uv[0] = uv0uv1.xy;
  tc.uv[1] = uv0uv1.zw;

  MetallicRoughnessDataGPU mat = getMaterial(getMaterialId());
  EnvironmentMapDataGPU envMap = getEnvironmentMap(getEnvironmentId());

  vec4 Kd  = sampleAlbedo(tc, mat) * color;

  if ((mat.alphaMode == 1) && (mat.emissiveFactorAlphaCutoff.w > Kd.a)) {
    discard;
  }

  if (isMaterialTypeUnlit(mat)) {
    out_FragColor = Kd;
    return;
  }


  vec4 Kao = sampleAO(tc, mat);
  vec4 Ke  = sampleEmissive(tc, mat);
  vec4 mrSample = sampleMetallicRoughness(tc, mat);


  bool isSheen = isMaterialTypeSheen(mat);
  bool isClearCoat = isMaterialTypeClearCoat(mat);
  bool isSpecular = isMaterialTypeSpecular(mat);
  bool isTransmission = isMaterialTypeTransmission(mat);
  bool isVolume = isMaterialTypeVolume(mat);

  vec3 n = normalize(normal);

  PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(tc, Kd, mrSample, mat);
  pbrInputs.n = n;
  pbrInputs.ng = n;

  if (mat.normalTexture != ~0) {
    vec3 normalSample = sampleNormal(tc, mat).xyz;
    perturbNormal(n, worldPos, normalSample, getNormalUV(tc, mat), pbrInputs);
    n = pbrInputs.n;
  }
  vec3 v = normalize(perFrame.drawable.cameraPos.xyz - worldPos);  // Vector from surface point to camera

  pbrInputs.v = v;
  pbrInputs.NdotV = clamp(abs(dot(pbrInputs.n, pbrInputs.v)), 0.001, 1.0);

  if (isSheen) {
    pbrInputs.sheenColorFactor = getSheenColorFactor(tc, mat).rgb;
    pbrInputs.sheenRoughnessFactor = getSheenRoughnessFactor(tc, mat);
  }

  vec3 clearCoatContrib = vec3(0);

  if (isClearCoat) {
    pbrInputs.clearcoatFactor = getClearcoatFactor(tc, mat);
    pbrInputs.clearcoatRoughness = clamp(getClearcoatRoughnessFactor(tc, mat), 0.0, 1.0);
    pbrInputs.clearcoatF0 = vec3(pow((pbrInputs.ior - 1.0) / (pbrInputs.ior + 1.0), 2.0));
    pbrInputs.clearcoatF90 = vec3(1.0);


    if (mat.clearCoatNormalTextureUV>-1) {
      pbrInputs.clearcoatNormal = mat3(pbrInputs.t, pbrInputs.b, pbrInputs.ng) * sampleClearcoatNormal(tc, mat).rgb;
    } else {
      pbrInputs.clearcoatNormal =pbrInputs.ng;
    }
    clearCoatContrib = getIBLRadianceGGX(pbrInputs.clearcoatNormal, pbrInputs.v, pbrInputs.clearcoatRoughness, pbrInputs.clearcoatF0, 1.0, envMap);
  }

  if (isTransmission) {
    pbrInputs.transmissionFactor = getTransmissionFactor(tc, mat);
  }

  if (isVolume) {
    pbrInputs.thickness = getVolumeTickness(tc, mat);
    pbrInputs.attenuation = getVolumeAttenuation(mat);
  }

  // IBL contribution
  vec3 specularColor = getIBLRadianceContributionGGX(pbrInputs, pbrInputs.specularWeight, envMap);
  vec3 diffuseColor = getIBLRadianceLambertian(pbrInputs.NdotV, n, pbrInputs.perceptualRoughness, pbrInputs.diffuseColor, pbrInputs.reflectance0, pbrInputs.specularWeight, envMap);

  vec3 transmission = vec3(0,0,0);
  if (isTransmission) {
    transmission += getIBLVolumeRefraction(
        pbrInputs.n, pbrInputs.v,
        pbrInputs.perceptualRoughness,
        pbrInputs.diffuseColor, pbrInputs.reflectance0, pbrInputs.reflectance90,
        worldPos, getModel(), getViewProjection(),
        pbrInputs.ior, pbrInputs.thickness, pbrInputs.attenuation.rgb, pbrInputs.attenuation.w);
  }

  vec3 sheenColor = vec3(0);

  if (isSheen) {
    sheenColor += getIBLRadianceCharlie(pbrInputs, envMap);
  }


  vec3 lights_diffuse = vec3(0);
  vec3 lights_specular = vec3(0);
  vec3 lights_sheen = vec3(0);
  vec3 lights_clearcoat = vec3(0);
  vec3 lights_transmission = vec3(0);

  float albedoSheenScaling = 1.0;

  for (uint i = 0; i < getLightsCount(); ++i)
  {
    Light light = getLight(i);

    vec3 pointToLight = (light.type == LightType_Directional) ? -light.direction : light.position - worldPos;

    // BSTF
    vec3 l = normalize(pointToLight);
    vec3 h = normalize(l + v);
    float NdotL = clampedDot(n, l);
    float NdotV = clampedDot(n, v);
    float NdotH = clampedDot(n, h);
    float LdotH = clampedDot(l, h);
    float VdotH = clampedDot(v, h);
    if (NdotL > 0.0 || NdotV > 0.0) 
    {
      // Calculation of analytical light
      // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
      vec3 intensity = getLightIntensity(light, pointToLight);

      lights_diffuse += intensity * NdotL *  getBRDFLambertian(pbrInputs.reflectance0, pbrInputs.reflectance90, pbrInputs.diffuseColor, pbrInputs.specularWeight, VdotH);
      lights_specular += intensity * NdotL * getBRDFSpecularGGX(pbrInputs.reflectance0, pbrInputs.reflectance90, pbrInputs.alphaRoughness, pbrInputs.specularWeight, VdotH, NdotL, NdotV, NdotH);

      if (isSheen) {
        lights_sheen += intensity * getPunctualRadianceSheen(pbrInputs.sheenColorFactor, pbrInputs.sheenRoughnessFactor, NdotL, NdotV, NdotH);
        albedoSheenScaling = min(1.0 - max3(pbrInputs.sheenColorFactor) * albedoSheenScalingFactor(NdotV, pbrInputs.sheenRoughnessFactor),
        1.0 - max3(pbrInputs.sheenColorFactor) * albedoSheenScalingFactor(NdotL, pbrInputs.sheenRoughnessFactor));
      }

      if (isClearCoat) {
        lights_clearcoat += intensity * getPunctualRadianceClearCoat(pbrInputs.clearcoatNormal, v, l, h, VdotH,
        pbrInputs.clearcoatF0, pbrInputs.clearcoatF90, pbrInputs.clearcoatRoughness);
      }
    }
        // BDTF
    if (isTransmission) {
      // If the light ray travels through the geometry, use the point it exits the geometry again.
      // That will change the angle to the light source, if the material refracts the light ray.
      vec3 transmissionRay = getVolumeTransmissionRay(n, v, pbrInputs.thickness, pbrInputs.ior, getModel());
      pointToLight -= transmissionRay;
      l = normalize(pointToLight);

      vec3 intensity = getLightIntensity(light, pointToLight);
      vec3 transmittedLight = intensity * getPunctualRadianceTransmission(n, v, l, pbrInputs.alphaRoughness, pbrInputs.reflectance0, pbrInputs.clearcoatF90, pbrInputs.diffuseColor, pbrInputs.ior);

      if (isVolume) {
        transmittedLight = applyVolumeAttenuation(transmittedLight, length(transmissionRay), pbrInputs.attenuation.rgb, pbrInputs.attenuation.w);
      }

      lights_transmission += transmittedLight;
     }
    }

  // ambient occlusion
  float occlusion = Kao.r < 0.01 ? 1.0 : Kao.r;
  float occlusionStrength = getOcclusionFactor(mat);
  diffuseColor = lights_diffuse + mix(diffuseColor, diffuseColor * occlusion, occlusionStrength);
  specularColor = lights_specular + mix(specularColor, specularColor * occlusion, occlusionStrength);
  sheenColor = lights_sheen + mix(sheenColor, sheenColor * occlusion, occlusionStrength);

  vec3 emissiveColor = Ke.rgb * sampleEmissive(tc, mat).rgb;

  vec3 clearcoatFresnel = vec3(0);
  if (isClearCoat) {
    clearcoatFresnel = F_Schlick(pbrInputs.clearcoatF0, pbrInputs.clearcoatF90, clampedDot(pbrInputs.clearcoatNormal, pbrInputs.v));
  }

  if (isTransmission) {
    diffuseColor = mix(diffuseColor, transmission, pbrInputs.transmissionFactor);
  }

  vec3 color =  specularColor + diffuseColor + emissiveColor + sheenColor;
  color = color * (1.0 - pbrInputs.clearcoatFactor * clearcoatFresnel) + clearCoatContrib;


  color = pow(color, vec3(1.0/2.2));
  out_FragColor = vec4(color, 1.0);

//  DEBUG 
//  out_FragColor = vec4((n + vec3(1.0))*0.5, 1.0);
//  out_FragColor = vec4((pbrInputs.n + vec3(1.0))*0.5, 1.0);
//  out_FragColor = vec4((normal + vec3(1.0))*0.5, 1.0);
//  out_FragColor = Kao;
//  out_FragColor = Ke;
//  out_FragColor = Kd;
//  vec2 MeR = mrSample.yz;
//  MeR.x *= getMetallicFactor(mat);
//  MeR.y *= getRoughnessFactor(mat);
//  out_FragColor = vec4(MeR.y,MeR.y,MeR.y, 1.0);
//  out_FragColor = vec4(MeR.x,MeR.x,MeR.x, 1.0);
//  out_FragColor = mrSample;
//  out_FragColor = vec4(transmission, 1.0);
//  out_FragColor = vec4(punctualColor, 1.0);
}

