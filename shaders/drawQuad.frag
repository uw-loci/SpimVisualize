#version 110

varying vec2 texcoord;
uniform sampler2D colormap;

void main()
{
	vec3 color = texture2D(colormap, texcoord).rgb;

	//color = vec3(texcoord, 0.0);


	gl_FragColor = vec4(color, 1.0);
}