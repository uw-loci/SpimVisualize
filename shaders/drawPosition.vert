
#version 120

uniform mat4 transform = mat4(1.0);
uniform mat4 mvp = mat4(1.0);

varying vec4 worldPosition;

void main()
{
	worldPosition = transform * gl_Vertex;
	gl_Position = mvp * worldPosition;

}

