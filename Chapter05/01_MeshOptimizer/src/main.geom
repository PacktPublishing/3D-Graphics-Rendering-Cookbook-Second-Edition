//
#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
layout (location=0) in vec3 color[];
layout (location=0) out vec3 colors;
layout (location=1) out vec3 barycoords;

void main() {
	const vec3 bc[3] = vec3[](
		vec3(1.0, 0.0, 0.0),
		vec3(0.0, 1.0, 0.0),
		vec3(0.0, 0.0, 1.0)
	);
	for ( int i = 0; i < 3; i++ )	{
		gl_Position = gl_in[i].gl_Position;
		colors = color[i];
		barycoords = bc[i];
		EmitVertex();
	}
	EndPrimitive();
}
