
#version 120

uniform mat4 mvpMatrix = mat4(1.0);
uniform mat4 transform = mat4(1.0);


varying vec3 texcoord;
varying float zDistance;

void main()
{
	gl_Position = mvpMatrix * transform * vec4(gl_Vertex.xyz, 1.0);
	texcoord = gl_MultiTexCoord0.xyz;

	zDistance = gl_Position.z;
}

