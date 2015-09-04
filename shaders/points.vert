
#version 330

in vec4 vertex;

uniform mat4 mvpMatrix;

out float intensity;

void main()
{

	gl_Position = mvpMatrix * vec4(vertex.xyz, 1.0);
	intensity = vertex.w;

}

