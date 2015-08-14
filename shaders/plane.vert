
#version 120

varying vec2 texcoord;

uniform mat4 mvpMatrix;
uniform mat4 transform;

void main()
{

	gl_Position = mvpMatrix * /* transform */ gl_Vertex;
	texcoord = (gl_Vertex.xy + vec2(1.0) )* 0.5;

}

