//
#version 460 core

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
};

layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;

void main() {
	gl_Position = MVP * vec4(pos, 1.0);
	color = pos.xzy;
}
