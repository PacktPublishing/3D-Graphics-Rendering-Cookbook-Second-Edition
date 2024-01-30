//
#version 460 core

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
};

layout (location=0) in vec4 in_pos;
layout (location=1) in vec2 in_uv;
layout (location=2) in vec3 in_normal;

layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;

void main() {
  gl_Position = MVP * in_pos;

  uv = in_uv; 
  normal = in_normal;
}
