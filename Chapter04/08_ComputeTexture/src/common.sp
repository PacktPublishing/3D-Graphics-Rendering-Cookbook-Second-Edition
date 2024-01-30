//

layout(push_constant) uniform PerFrameData {
	mat4 viewproj;
	uint textureId;
	uvec2 bufPosAngleId;
	uvec2 bufMatricesId;
	uvec2 bufVerticesId;
	float time;
};

layout(std430, buffer_reference) readonly buffer Positions {
  vec4 pos[]; // pos, initialAngle
};
