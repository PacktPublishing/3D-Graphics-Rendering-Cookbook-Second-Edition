//
#version 460 core

layout (location=0) in vec3 colors;
layout (location=1) in vec3 barycoords;
layout (location=0) out vec4 out_FragColor;

float edgeFactor(float thickness) {
	vec3 a3 = smoothstep( vec3( 0.0 ), fwidth(barycoords) * thickness, barycoords);
	return min( min( a3.x, a3.y ), a3.z );
}

void main() {
	out_FragColor = vec4( mix( vec3(0.0), colors, edgeFactor(1.0) ), 1.0 );
};
