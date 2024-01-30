#include <data/shaders/gltf/inputs.frag>
#include <data/shaders/gltf/PBR.sp>

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

	PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(tc, Kd, mrSample);

	vec3 normalSample = sampleNormal(tc, getMaterialId()).xyz;
	perturbNormal(n, worldPos, normalSample, getNormalUV(tc, getMaterialId()), pbrInputs);

	vec3 v = normalize(perFrame.drawable.cameraPos.xyz - worldPos);	// Vector from surface point to camera

	pbrInputs.v = v;
	pbrInputs.NdotV = clamp(abs(dot(pbrInputs.n, pbrInputs.v)), 0.001, 1.0);

	pbrInputs.ior = 1.5;

	if ((getMaterialType(getMaterialId()) & 4) != 0) {
		pbrInputs.sheenColorFactor = getSheenColorFactor(tc, getMaterialId()).rgb;
		pbrInputs.sheenRoughnessFactor = getSheenRoughnessFactor(tc, getMaterialId());
	}

	vec3 clearCoatContrib = vec3(0);

	if ((getMaterialType(getMaterialId()) & 8) != 0) {
      pbrInputs.clearcoatFactor = getClearcoatFactor(tc, getMaterialId());
      pbrInputs.clearcoatRoughness = clamp(getClearcoatRoughnessFactor(tc, getMaterialId()), 0.0, 1.0);
      pbrInputs.clearcoatF0 = vec3(pow((pbrInputs.ior - 1.0) / (pbrInputs.ior + 1.0), 2.0));
      pbrInputs.clearcoatF90 = vec3(1.0);
	  pbrInputs.clearcoatNormal = mat3(pbrInputs.t, pbrInputs.b, pbrInputs.ng) * sampleClearcoatNormal(tc, getMaterialId()).rgb;
	  clearCoatContrib = getIBLRadianceGGX(pbrInputs.clearcoatNormal, pbrInputs.v, pbrInputs.clearcoatRoughness, pbrInputs.clearcoatF0, 1.0);
	}

	vec3 specular_color = getIBLRadianceContributionGGX(pbrInputs, pbrInputs.specularWeight);
	vec3 diffuse_color = getIBLRadianceLambertian(pbrInputs.NdotV, n, pbrInputs.perceptualRoughness, pbrInputs.diffuseColor, pbrInputs.reflectance0, pbrInputs.specularWeight);

	// one hardcoded light source
	vec3 color =  specular_color + diffuse_color;

	vec3 lightPos = vec3(0, 0, -5);
	color += calculatePBRLightContribution( pbrInputs, normalize(lightPos - worldPos), vec3(1.0) );
	// ambient occlusion
	color = color * ( Kao.r < 0.01 ? 1.0 : Kao.r );
	// emissive
	color = pow( Ke.rgb * getEmissiveFactorAlphaCutoff(getMaterialId()).rgb + color, vec3(1.0/2.2) );

	if ((getMaterialType(getMaterialId()) & 4) != 0) {
		color += getIBLRadianceCharlie(pbrInputs);
	}

	if ((getMaterialType(getMaterialId()) & 8) != 0) {
		vec3 clearcoatFresnel = F_Schlick(pbrInputs.clearcoatF0, pbrInputs.clearcoatF90, clampedDot(pbrInputs.clearcoatNormal, pbrInputs.v));
		color = color * (1.0 - pbrInputs.clearcoatFactor * clearcoatFresnel) + clearCoatContrib;
	}

	out_FragColor = vec4(color, 1.0);

//	out_FragColor = vec4((n + vec3(1.0))*0.5, 1.0);
//	out_FragColor = Kao;
//	out_FragColor = Ke;
//	out_FragColor = Kd;
//	vec2 MeR = mrSample.yz;
//	MeR.x *= getMetallicFactor(getMaterialId());
//	MeR.y *= getRoughnessFactor(getMaterialId());
//	out_FragColor = vec4(MeR.y,MeR.y,MeR.y, 1.0);
//	out_FragColor = vec4(MeR.x,MeR.x,MeR.x, 1.0);
//	out_FragColor = mrSample;
}
