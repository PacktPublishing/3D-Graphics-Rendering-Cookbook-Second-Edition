//
#version 460 core

#include <data/shaders/GridParameters.h>

layout(push_constant) uniform PerFrameData {
	mat4 MVP;
	vec4 cameraPos;
	vec4 origin;
};

layout (location=0) out vec2 uv;
layout (location=1) out vec2 out_camPos;

const vec3 pos[4] = vec3[4](
	vec3(-1.0, 0.0, -1.0),
	vec3( 1.0, 0.0, -1.0),
	vec3( 1.0, 0.0,  1.0),
	vec3(-1.0, 0.0,  1.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

void main()
{
	int idx = indices[gl_VertexIndex];
	vec3 position = pos[idx] * gridSize;
	
	position.x += cameraPos.x;
	position.z += cameraPos.z;

	position += origin.xyz;

	out_camPos = cameraPos.xz;

	gl_Position = MVP * vec4(position, 1.0);
	uv = position.xz;
}
