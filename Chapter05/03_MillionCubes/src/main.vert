//

layout(push_constant) uniform PerFrameData {
	mat4 viewproj;
	uint textureId;
	uvec2 bufId;
	float time;
};

layout (location=0) out vec3 color;
layout (location=1) out vec2 uv;

layout(std430, buffer_reference) readonly buffer Positions {
  vec4 pos[]; // pos, initialAngle
};

const int indices[36] = int[36](0,  2,  1,  2,  3,  1,  5,  4,  1,  1,  4,  0,
                                0,  4,  6,  0,  6,  2,  6,  5,  7,  6,  4,  5,
                                2,  6,  3,  6,  7,  3,  7,  1,  3,  7,  5,  1);

const vec3 colors[7] = vec3[7](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
	vec3(1.0, 1.0, 0.0),
	vec3(0.0, 1.0, 1.0),
	vec3(1.0, 0.0, 1.0),
	vec3(1.0, 1.0, 1.0));

mat4 translate(mat4 m, vec3 v) {
	mat4 Result = m;
	Result[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
	return Result;
}

mat4 rotate(mat4 m, float angle, vec3 v) {
	float a = angle;
	float c = cos(a);
	float s = sin(a);

	vec3 axis = normalize(v);
	vec3 temp = (float(1.0) - c) * axis;

	mat4 r;
	r[0][0] = c + temp[0] * axis[0];
	r[0][1] = temp[0] * axis[1] + s * axis[2];
	r[0][2] = temp[0] * axis[2] - s * axis[1];

	r[1][0] = temp[1] * axis[0] - s * axis[2];
	r[1][1] = c + temp[1] * axis[1];
	r[1][2] = temp[1] * axis[2] + s * axis[0];

	r[2][0] = temp[2] * axis[0] + s * axis[1];
	r[2][1] = temp[2] * axis[1] - s * axis[0];
	r[2][2] = c + temp[2] * axis[2];

	mat4 res;
	res[0] = m[0] * r[0][0] + m[1] * r[0][1] + m[2] * r[0][2];
	res[1] = m[0] * r[1][0] + m[1] * r[1][1] + m[2] * r[1][2];
	res[2] = m[0] * r[2][0] + m[1] * r[2][1] + m[2] * r[2][2];
	res[3] = m[3];
	return res;
}

void main() {
	vec4 center = Positions(bufId).pos[gl_InstanceIndex];
	mat4 model = rotate(translate(mat4(1.0f), center.xyz), time + center.w, vec3(1.0f, 1.0f, 1.0f));

	int idx = indices[gl_VertexIndex];

   vec3 xyz = vec3(idx & 1, (idx & 4) >> 2, (idx & 2) >> 1);

   const float edge = 1.0;

	gl_Position = viewproj * model * vec4(edge * (xyz - vec3(0.5)), 1.0);

	int face = gl_VertexIndex / 6;

   if (face == 0 || face == 3) uv = vec2(xyz.x, xyz.z);
   if (face == 1 || face == 4) uv = vec2(xyz.x, xyz.y);
   if (face == 2 || face == 5) uv = vec2(xyz.y, xyz.z);

   color = colors[gl_InstanceIndex % 7];
}
