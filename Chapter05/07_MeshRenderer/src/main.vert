//

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
};

layout (location=0) in vec3 in_pos;
layout (location=1) in vec2 in_tc;
layout (location=2) in vec3 in_normal;

layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;

void main() {
	gl_Position = MVP * vec4(in_pos, 1.0);
	uv = in_tc;
	normal = in_normal;
}
