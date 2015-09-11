
#version 330

in vec3 vertex;

uniform mat4 mvpMatrix;
uniform mat4 transform = mat4(1.0);
void main()
{

	gl_Position = mvpMatrix * transform * vec4(vertex, 1.0);
}

