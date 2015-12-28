
#version 120

uniform mat4 mvpMatrix;
uniform mat4 transform;

varying vec3 color;

void main()
{
	gl_Position = mvpMatrix * transform * vec4(gl_Vertex.xyz, 1.0);
	color = gl_Color.rgb;
}

