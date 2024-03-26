//

layout (location=0) in vec2 uvs;
layout (location=1) in vec3 barycoords;
layout (location=2) in vec3 normal;

layout (location=0) out vec4 out_FragColor;

float edgeFactor(float thickness) {
	vec3 a3 = smoothstep( vec3( 0.0 ), fwidth(barycoords) * thickness, barycoords);
	return min( min( a3.x, a3.y ), a3.z );
}

void main() {
	float NdotL = clamp(dot(normalize(normal), normalize(vec3(-1,1,-1))), 0.5, 1.0);

	vec4 color = vec4(1.0, 1.0, 1.0, 1.0) * NdotL;

	out_FragColor = mix( vec4(0.1), color, edgeFactor(1.0) );
};
