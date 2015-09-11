
#version 330

in vec3 vertex;

uniform mat4 mvpMatrix;

out float intensity;

void main()
{

	gl_Position = mvpMatrix * vec4(vertex.xyz, 1.0);
	intensity = vertex.w;

}

