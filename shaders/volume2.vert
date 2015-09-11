
#version 120

uniform mat4 viewMatrix = mat4(1.0);
uniform mat4 projMatrix = mat4(1.0);
uniform mat4 transform = mat4(1.0);

uniform mat4 inverseMVP = mat4(1.0);

varying vec3 texcoord;
varying vec3 worldPosition;

// far map offset
const float EPSILON = 0.0001;

void main()
{
	vec4 vertex = gl_Vertex - vec4(0.0, 0.0, EPSILON, 0.0);

	vec4 v = inverseMVP * vertex;
	v /= v.w;
	worldPosition = v.xyz;



	gl_Position = vertex;// * vec4(0.8, 0.8, 1.0, 1.0);

}

