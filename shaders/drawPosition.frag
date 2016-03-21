#version 120

varying vec4 worldPosition;

void main()
{
	gl_FragColor = vec4(worldPosition.xyz, 1.0);

}