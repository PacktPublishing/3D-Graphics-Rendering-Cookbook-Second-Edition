// Based on: https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.

struct PBRInfo {
  // geometry properties
  float NdotL;                  // cos angle between normal and light direction
  float NdotV;                  // cos angle between normal and view direction
  float NdotH;                  // cos angle between normal and half vector
  float LdotH;                  // cos angle between light direction and half vector
  float VdotH;                  // cos angle between view direction and half vector

  // Normal
  vec3 n;              // Sahding normal
  vec3 ng;            // Geometry normal
  vec3 t;              // Geometry tangent
  vec3 b;              // Geometry bitangent

  vec3 v;              // vector from surface point to camera

  // material properties
  float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
  vec3 reflectance0;            // full reflectance color (normal incidence angle)
  vec3 reflectance90;           // reflectance color at grazing angle
  float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
  vec3 diffuseColor;            // color contribution from diffuse lighting
  vec3 specularColor;           // color contribution from specular lighting

  vec4 baseColor;
  float metallic;

  float sheenRoughnessFactor;
  vec3 sheenColorFactor;

  vec3 clearcoatF0;
  vec3 clearcoatF90;
  float clearcoatFactor;
  vec3 clearcoatNormal;
  float clearcoatRoughness;

  // KHR_materials_specular 
  float specularWeight; // product of specularFactor and specularTexture.a

  float transmissionFactor;

  float thickness;
  vec4 attenuation; // rgb - color, w - distance

  // KHR_materials_iridescence
  float iridescenceFactor;
  float iridescenceIor;
  float iridescenceThickness;

  // KHR_materials_anisotropy
  vec3 anisotropicT;
  vec3 anisotropicB;
  float anisotropyStrength;

  float ior;
};

const float M_PI = 3.141592653589793;

vec4 SRGBtoLINEAR(vec4 srgbIn) {
  vec3 linOut = pow(srgbIn.xyz,vec3(2.2));

  return vec4(linOut, srgbIn.a);
}

float clampedDot(vec3 x, vec3 y) {
  return clamp(dot(x, y), 0.0, 1.0);
}

//
// Fresnel
//
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://github.com/wdas/brdf/tree/master/src/brdfs
// https://google.github.io/filament/Filament.md.html
//

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH) {
  return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float F_Schlick(float f0, float f90, float VdotH) {
  float x = clamp(1.0 - VdotH, 0.0, 1.0);
  float x2 = x * x;
  float x5 = x * x2 * x2;
  return f0 + (f90 - f0) * x5;
}

float F_Schlick(float f0, float VdotH) {
  float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
  return F_Schlick(f0, f90, VdotH);
}

vec3 F_Schlick(vec3 f0, float f90, float VdotH) {
  float x = clamp(1.0 - VdotH, 0.0, 1.0);
  float x2 = x * x;
  float x5 = x * x2 * x2;
  return f0 + (f90 - f0) * x5;
}

vec3 F_Schlick(vec3 f0, float VdotH) {
  float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
  return F_Schlick(f0, f90, VdotH);
}

vec3 Schlick_to_F0(vec3 f, vec3 f90, float VdotH) {
  float x = clamp(1.0 - VdotH, 0.0, 1.0);
  float x2 = x * x;
  float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

  return (f - f90 * x5) / (1.0 - x5);
}

float Schlick_to_F0(float f, float f90, float VdotH) {
  float x = clamp(1.0 - VdotH, 0.0, 1.0);
  float x2 = x * x;
  float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

  return (f - f90 * x5) / (1.0 - x5);
}

vec3 Schlick_to_F0(vec3 f, float VdotH) {
  return Schlick_to_F0(f, vec3(1.0), VdotH);
}

float Schlick_to_F0(float f, float VdotH) {
  return Schlick_to_F0(f, 1.0, VdotH);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;

  float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
  float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

  float GGX = GGXV + GGXL;

  return GGX > 0.0 ? 0.5 / GGX : 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;
  float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
  return alphaRoughnessSq / (M_PI * f * f);
}

// specularWeight is introduced with KHR_materials_specular
vec3 getIBLRadianceLambertian(float NdotV, vec3 n, float roughness, vec3 diffuseColor, vec3 F0, float specularWeight, EnvironmentMapDataGPU envMap) {
  vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));

  vec2 f_ab = sampleBRDF_LUT(brdfSamplePoint, envMap).rg;

  vec3 irradiance = sampleEnvMapIrradiance(n.xyz, envMap).rgb;

  // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
  // Roughness dependent fresnel, from Fdez-Aguera

  vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
  vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
  vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

  // Multiple scattering, from Fdez-Aguera
  float Ems = (1.0 - (f_ab.x + f_ab.y));
  vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
  vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
  vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

  return (FmsEms + k_D) * irradiance;
}

//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 getBRDFLambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH) {
  // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
  return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 getBRDFSpecularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH) {
  vec3 F = F_Schlick(f0, f90, VdotH);
  float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
  float D = D_GGX(NdotH, alphaRoughness);

  return specularWeight * F * Vis * D;
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0, float specularWeight, EnvironmentMapDataGPU envMap ) {
  float NdotV = clampedDot(n, v);

  float mipCount = float(sampleEnvMapQueryLevels(envMap));
  float lod = roughness * float(mipCount - 1);
  vec3 reflection = normalize(reflect(-v, n));

  vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec3 f_ab = sampleBRDF_LUT(brdfSamplePoint, envMap).rgb;
  vec3 specularLight = sampleEnvMapLod(reflection.xyz, lod, envMap).rgb;

  // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
  // Roughness dependent fresnel, from Fdez-Aguera
  vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
  vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
  vec3 FssEss = k_S * f_ab.x + f_ab.y;

  return specularWeight * specularLight * FssEss;
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 getIBLRadianceContributionGGX(PBRInfo pbrInputs, float specularWeight, EnvironmentMapDataGPU envMap) {
  vec3 n = pbrInputs.n;
  vec3 v = pbrInputs.v;
  vec3 reflection = normalize(reflect(-v, n));
  float mipCount = float(sampleEnvMapQueryLevels(envMap));
  float lod = pbrInputs.perceptualRoughness * (mipCount - 1);

  // retrieve a scale and bias to F0. See [1], Figure 3
  vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, pbrInputs.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec3 brdf = sampleBRDF_LUT(brdfSamplePoint, envMap).rgb;
  // HDR envmaps are already linear
  vec3 specularLight = sampleEnvMapLod(reflection.xyz, lod, envMap).rgb;

  // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
  // Roughness dependent fresnel, from Fdez-Aguera
  vec3 Fr = max(vec3(1.0 - pbrInputs.perceptualRoughness), pbrInputs.reflectance0) - pbrInputs.reflectance0;
  vec3 k_S = pbrInputs.reflectance0 + Fr * pow(1.0 - pbrInputs.NdotV, 5.0);
  vec3 FssEss = k_S * brdf.x + brdf.y;

  return specularWeight * specularLight * FssEss;
}


// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
vec3 diffuseBurley(PBRInfo pbrInputs) {
  float f90 = 2.0 * pbrInputs.LdotH * pbrInputs.LdotH * pbrInputs.alphaRoughness - 0.5;

  return (pbrInputs.diffuseColor / M_PI) * (1.0 + f90 * pow((1.0 - pbrInputs.NdotL), 5.0)) * (1.0 + f90 * pow((1.0 - pbrInputs.NdotV), 5.0));
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs) {
  return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs) {
  float NdotL = pbrInputs.NdotL;
  float NdotV = pbrInputs.NdotV;
  float rSqr = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;

  float attenuationL = 2.0 * NdotL / (NdotL + sqrt(rSqr + (1.0 - rSqr) * (NdotL * NdotL)));
  float attenuationV = 2.0 * NdotV / (NdotV + sqrt(rSqr + (1.0 - rSqr) * (NdotV * NdotV)));
  return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs) {
  float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
  float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
  return roughnessSq / (M_PI * f * f);
}

vec3 getIBLRadianceCharlie(PBRInfo pbrInputs, EnvironmentMapDataGPU envMap) {
  float sheenRoughness = pbrInputs.sheenRoughnessFactor;
  vec3 sheenColor = pbrInputs.sheenColorFactor;
  float mipCount = float(sampleEnvMapQueryLevels(envMap));
  float lod = sheenRoughness * float(mipCount - 1);
  vec3 reflection = normalize(reflect(-pbrInputs.v, pbrInputs.n));

  vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, sheenRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  float brdf = sampleBRDF_LUT(brdfSamplePoint, envMap).b;
  vec3 sheenSample = sampleCharlieEnvMapLod(reflection.xyz, lod, envMap).rgb;

  return sheenSample * sheenColor * brdf;
}

PBRInfo calculatePBRInputsMetallicRoughness(InputAttributes tc, vec4 albedo, vec4 mrSample, MetallicRoughnessDataGPU mat) {
  PBRInfo pbrInputs;
  
  bool isSpecularGlossiness = (getMaterialType(mat) & 2) != 0;
  bool isSpecular = (getMaterialType(mat) & 0x10) != 0;

  pbrInputs.ior = getIOR(mat);
  vec3 f0 = isSpecularGlossiness ? getSpecularFactor(mat) * mrSample.rgb : vec3(pow((pbrInputs.ior - 1)/(pbrInputs.ior + 1), 2));

  float metallic = getMetallicFactor(mat);
  metallic = mrSample.b * metallic;
  metallic = clamp(metallic, 0.0, 1.0);

  pbrInputs.baseColor = albedo;
  pbrInputs.metallic = metallic;
  pbrInputs.specularWeight = 1.0;

  if (isSpecular) {
    vec3 dielectricSpecularF0 = min(f0 *  getSpecularColorFactor(tc, mat), vec3(1.0));
    f0 = mix(dielectricSpecularF0, pbrInputs.baseColor.rgb, metallic);
    pbrInputs.specularWeight = getSpecularFactor(tc, mat);//u_KHR_materials_specular_specularFactor * specularTexture.a;
    //pbrInputs.diffuseColor = mix(pbrInputs.baseColor.rgb, vec3(0), metallic);
  }

  float perceptualRoughness = isSpecularGlossiness ? getGlossinessFactor(mat): getRoughnessFactor(mat);

  const float c_MinRoughness = 0.04;

  // Metallic roughness:
  // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
  // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
  // Specular Glossiness:
  // Glossiness is stored in alpha channel
  perceptualRoughness = isSpecularGlossiness ? 1.0 - mrSample.a * perceptualRoughness : clamp(mrSample.g * perceptualRoughness, c_MinRoughness, 1.0);

  // Roughness is authored as perceptual roughness; as is convention,
  // convert to material roughness by squaring the perceptual roughness [2].
  float alphaRoughness = perceptualRoughness * perceptualRoughness;

  vec3 diffuseColor = isSpecularGlossiness ?  pbrInputs.baseColor.rgb * (1.0 - max(max(f0.r, f0.g), f0.b)) : mix(pbrInputs.baseColor.rgb, vec3(0), metallic); 
  vec3 specularColor = isSpecularGlossiness ? f0 : mix(f0, pbrInputs.baseColor.rgb, metallic);

  // Compute reflectance.
  float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

  // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
  // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
  float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
  vec3 specularEnvironmentR0 = specularColor.rgb;
  vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

  pbrInputs.alphaRoughness = alphaRoughness;

  pbrInputs.perceptualRoughness = perceptualRoughness;
  pbrInputs.reflectance0 = specularEnvironmentR0;
  pbrInputs.reflectance90 = specularEnvironmentR90;

  pbrInputs.diffuseColor = diffuseColor;
  pbrInputs.specularColor = specularColor;

  return pbrInputs;
}

vec3 calculatePBRLightContribution( inout PBRInfo pbrInputs, vec3 lightDirection, vec3 lightColor ) {
  vec3 n = pbrInputs.n;
  vec3 v = pbrInputs.v;
  vec3 l = normalize(lightDirection);  // Vector from surface point to light
  vec3 h = normalize(l+v);        // Half vector between both l and v

  float NdotV = pbrInputs.NdotV;
  float NdotL = clamp(dot(n, l), 0.001, 1.0);
  float NdotH = clamp(dot(n, h), 0.0, 1.0);
  float LdotH = clamp(dot(l, h), 0.0, 1.0);
  float VdotH = clamp(dot(v, h), 0.0, 1.0);

  vec3 color = vec3(0);

  if (NdotL > 0.0 || NdotV > 0.0) {
    pbrInputs.NdotL = NdotL;
    pbrInputs.NdotH = NdotH;
    pbrInputs.LdotH = LdotH;
    pbrInputs.VdotH = VdotH;

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuseBurley(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    color = NdotL * lightColor * (diffuseContrib + specContrib);
  }
  return color;
}

// http://www.thetenthplanet.de/archives/1180
// modified to fix handedness of the resulting cotangent frame
mat3 cotangentFrame( vec3 N, vec3 p, vec2 uv, inout PBRInfo pbrInputs ) {
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx( p );
  vec3 dp2 = dFdy( p );
  vec2 duv1 = dFdx( uv );
  vec2 duv2 = dFdy( uv );

  if (length(duv1) <= 1e-2) {
    duv1 = vec2(1.0, 0.0);
  }

  if (length(duv2) <= 1e-2) {
    duv2 = vec2(0.0, 1.0);
  }

  // solve the linear system
  vec3 dp2perp = cross( dp2, N );
  vec3 dp1perp = cross( N, dp1 );
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );

  // calculate handedness of the resulting cotangent frame
  float w = dot(cross(N, T), B) < 0.0 ? -1.0 : 1.0;

  // adjust tangent if needed
  T = T * w;

  if (gl_FrontFacing == false) {
    N *= -1.0f;
    T *= -1.0f;
    B *= -1.0f;
  }

  pbrInputs.t = T * invmax;
  pbrInputs.b = B * invmax;
  pbrInputs.ng = N;

  return mat3( pbrInputs.t, pbrInputs.b, N );
}

void perturbNormal(vec3 n, vec3 v, vec3 normalSample, vec2 uv, inout PBRInfo pbrInputs) {
  vec3 map = normalize( 2.0 * normalSample - vec3(1.0) );
  mat3 TBN = cotangentFrame(n, v, uv, pbrInputs);

  pbrInputs.n = normalize(TBN * map);
}

// Check normals - model has tangents and a normal map!

vec3 getVolumeTransmissionRay(vec3 n, vec3 v, float thickness, float ior, mat4 modelMatrix) {
  // direction of refracted light
  vec3 refractionVector = refract(-v, n, 1.0 / ior);

  // compute rotation-independent scaling of the model matrix
  vec3 modelScale = vec3(length(modelMatrix[0].xyz),
                         length(modelMatrix[1].xyz),
                         length(modelMatrix[2].xyz));

  // the thickness is specified in local space
  return normalize(refractionVector) * thickness * modelScale.xyz;
}

vec3 applyVolumeAttenuation(vec3 radiance, float transmissionDistance, vec3 attenuationColor, float attenuationDistance) {
  if (attenuationDistance == 0.0) {
    // Attenuation distance is +∞ (which we indicate by zero), i.e. the transmitted color is not attenuated at all.
    return radiance;
  }

  // Compute light attenuation using Beer's law.
  vec3 attenuationCoefficient = -log(attenuationColor) / attenuationDistance;
  vec3 transmittance = exp(-attenuationCoefficient * transmissionDistance); // Beer's law
  return transmittance * radiance;
}

float applyIorToRoughness(float roughness, float ior) {
  // Scale roughness with IOR so that an IOR of 1.0 results in no microfacet refraction and
  // an IOR of 1.5 results in the default amount of microfacet refraction.
  return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}


vec3 getTransmissionSample(vec2 fragCoord, float roughness, float ior) {
  const ivec2 size = textureBindlessSize2D(perFrame.transmissionFramebuffer);
  const vec2 uv = fragCoord;
  float framebufferLod = log2(float(size.x)) * applyIorToRoughness(roughness, ior);
  vec3 transmittedLight = textureBindless2DLod(perFrame.transmissionFramebuffer, perFrame.transmissionFramebufferSampler, uv, framebufferLod).rgb; 
// DEBUG:
//  return vec3(uv, 0.0);
  return transmittedLight;
}

vec3 getIBLVolumeRefraction(vec3 n, vec3 v, float perceptualRoughness, vec3 baseColor, vec3 f0, vec3 f90,
  vec3 position, mat4 modelMatrix, mat4 viewProjMatrix, float ior, float thickness, vec3 attenuationColor, float attenuationDistance)
{
  vec3 transmissionRay = getVolumeTransmissionRay(n, v, thickness, ior, modelMatrix);
  vec3 refractedRayExit = position + transmissionRay;

  // Project refracted vector on the framebuffer, while mapping to normalized device coordinates.
  vec4 ndcPos = viewProjMatrix * vec4(refractedRayExit, 1.0);
  vec2 refractionCoords = ndcPos.xy / ndcPos.w;
  refractionCoords += 1.0;
  refractionCoords /= 2.0;
  refractionCoords.y = 1.0- refractionCoords.y;

  // Sample framebuffer to get pixel the refracted ray hits.
  vec3 transmittedLight = getTransmissionSample(refractionCoords, perceptualRoughness, ior);

  vec3 attenuatedColor = applyVolumeAttenuation(transmittedLight, length(transmissionRay), attenuationColor, attenuationDistance);

  // Sample GGX LUT to get the specular component.
  float NdotV = clampedDot(n, v);
  vec2 brdfSamplePoint = clamp(vec2(NdotV, perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  vec2 brdf = sampleBRDF_LUT(brdfSamplePoint, getEnvironmentMap(getEnvironmentId())).rg;
  vec3 specularColor = f0 * brdf.x + f90 * brdf.y;

  return (1.0 - specularColor) * attenuatedColor * baseColor;
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(float range, float distance) {
  if (range <= 0.0) {
    // negative range means unlimited
    return 1.0 / pow(distance, 2.0);
  }
  return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos) {
  float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));

  if (actualCos > outerConeCos) {
    if (actualCos < innerConeCos) {
      return smoothstep(outerConeCos, innerConeCos, actualCos);
    }
    return 1.0;
  }
  return 0.0;
}

vec3 getLightIntensity(Light light, vec3 pointToLight) {
  float rangeAttenuation = 1.0;
  float spotAttenuation = 1.0;

  if (light.type != LightType_Directional) {
    rangeAttenuation = getRangeAttenuation(light.range, length(pointToLight));
  }
  if (light.type == LightType_Spot) {
    spotAttenuation = getSpotAttenuation(pointToLight, light.direction, light.outerConeCos, light.innerConeCos);
  }

  return rangeAttenuation * spotAttenuation * light.intensity * light.color;
}

// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
float D_Charlie(float sheenRoughness, float NdotH) {
  sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
  float alphaG = sheenRoughness * sheenRoughness;
  float invR = 1.0 / alphaG;
  float cos2h = NdotH * NdotH;
  float sin2h = 1.0 - cos2h;
  return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}

float lambdaSheenNumericHelper(float x, float alphaG) {
  float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
  float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
  float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
  float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
  float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
  float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
  return a / (1.0 + b * pow(x, c)) + d * x + e;
}

float lambdaSheen(float cosTheta, float alphaG) {
  if (abs(cosTheta) < 0.5) {
    return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
  }

  return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) - lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
}

float V_Sheen(float NdotL, float NdotV, float sheenRoughness) {
  sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
  float alphaG = sheenRoughness * sheenRoughness;

  return clamp(1.0 / ((1.0 + lambdaSheen(NdotV, alphaG) + lambdaSheen(NdotL, alphaG)) * (4.0 * NdotV * NdotL)), 0.0, 1.0);
}

// f_sheen
vec3 getBRDFSpecularSheen(vec3 sheenColor, float sheenRoughness, float NdotL, float NdotV, float NdotH) {
  float sheenDistribution = D_Charlie(sheenRoughness, NdotH);
  float sheenVisibility = V_Sheen(NdotL, NdotV, sheenRoughness);
  return sheenColor * sheenDistribution * sheenVisibility;
}

vec3 getPunctualRadianceSheen(vec3 sheenColor, float sheenRoughness, float NdotL, float NdotV, float NdotH) {
  return NdotL * getBRDFSpecularSheen(sheenColor, sheenRoughness, NdotL, NdotV, NdotH);
}

// https://github.com/DassaultSystemes-Technology/dspbr-pt/blob/e7cfa6e9aab2b99065a90694e1f58564d675c1a4/packages/lib/shader/bsdfs/sheen.glsl#L16C1-L20C2
float albedoSheenScalingFactor(float NdotV, float sheenRoughnessFactor) {
  float c = 1.0 - NdotV;
  float c3 = c * c * c;
  return 0.65584461 * c3 + 1.0 / (4.16526551 + exp(-7.97291361 * sqrt(sheenRoughnessFactor) + 6.33516894));
}

float max3(vec3 v) {
  return max(max(v.x, v.y), v.z);
}

vec3 getPunctualRadianceClearCoat(vec3 clearcoatNormal, vec3 v, vec3 l, vec3 h, float VdotH, vec3 f0, vec3 f90, float clearcoatRoughness) {
  float NdotL = clampedDot(clearcoatNormal, l);
  float NdotV = clampedDot(clearcoatNormal, v);
  float NdotH = clampedDot(clearcoatNormal, h);
  return NdotL * getBRDFSpecularGGX(f0, f90, clearcoatRoughness * clearcoatRoughness, 1.0, VdotH, NdotL, NdotV, NdotH);
}

vec3 getPunctualRadianceTransmission(vec3 normal, vec3 view, vec3 pointToLight, float alphaRoughness, vec3 f0, vec3 f90, vec3 baseColor, float ior) {
  float transmissionRougness = applyIorToRoughness(alphaRoughness, ior);

  vec3 n = normalize(normal);       // Outward direction of surface point
  vec3 v = normalize(view);         // Direction from surface point to view
  vec3 l = normalize(pointToLight);
  vec3 l_mirror = normalize(l + 2.0*n*dot(-l, n));   // Mirror light reflection vector on surface
  vec3 h = normalize(l_mirror + v);                  // Halfway vector between transmission light vector and v

  float D = D_GGX(clamp(dot(n, h), 0.0, 1.0), transmissionRougness);
  vec3 F = F_Schlick(f0, f90, clamp(dot(v, h), 0.0, 1.0));
  float Vis = V_GGX(clamp(dot(n, l_mirror), 0.0, 1.0), clamp(dot(n, v), 0.0, 1.0), transmissionRougness);

  // Transmission BTDF
  return (1.0 - F) * baseColor * D * Vis;
}
