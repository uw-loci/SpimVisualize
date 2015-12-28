
#version 330

in vec3 vertexIn;
in vec3 colorIn;
in vec3 normalIn;

out vec3 color;

uniform mat4 mvpMatrix;
uniform mat4 transform = mat4(1.0);
void main()
{

	gl_Position = mvpMatrix * transform * vec4(vertexIn, 1.0);
	color = colorIn;
}

