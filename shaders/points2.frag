#version 120

varying vec3 color;

void main()
{

	//color = vec3(1.0, 1.0, 0.0);asfs

	gl_FragColor = vec4(color,  1.0);
}