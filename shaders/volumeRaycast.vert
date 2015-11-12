
#version 120

varying vec3 rayOrigin;
varying vec3 rayDirection;

uniform mat4 inverseMVP = mat4(1.0);
uniform vec3 cameraPos = vec3(0.0);

void main()
{
	gl_Position = gl_Vertex; //* vec4(0.8, 0.8, 1.0, 1.0);


	// the vertex is in clip space. transform to world space again to calculate ray origin
	vec4 v = inverseMVP * gl_Vertex;
	v /= v.w;
	rayOrigin = v.xyz;
	rayDirection = rayOrigin - cameraPos;

}
