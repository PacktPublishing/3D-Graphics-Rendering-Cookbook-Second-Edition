//

layout(std430, buffer_reference) buffer Materials;
layout(std430, buffer_reference) buffer Environments;

layout(std430, buffer_reference) buffer PerDrawData {
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
};

struct TransformsBuffer {
  mat4 model;
  uint matId;
  uint nodeRef;  // for cpu only
  uint meshRef;  // for cpu only
  uint opaque;  // for cpu only
};

layout(std430, buffer_reference) readonly buffer Transforms {
	TransformsBuffer transforms[];
};

layout(push_constant) uniform PerFrameData {
	PerDrawData drawable;
	Materials materials;
	Environments environments;
	Transforms  transforms;
	uint transformId;
	uint envId;
} perFrame;

uint getMaterialId() {
	return perFrame.transforms.transforms[perFrame.transformId].matId;
}

uint getEnvironmentId() {
	return perFrame.envId;
}
