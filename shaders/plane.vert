
#version 120

varying vec2 texcoord;

uniform mat4 mvpMatrix;
uniform mat4 transform;

uniform float width, height;

void main()
{

	vec4 v = gl_Vertex * 0.5;
	v.x *= width;
	v.y *= height;

	gl_Position = mvpMatrix *transform * v;
	texcoord = (gl_Vertex.xy + vec2(1.0) )* 0.5;

}

