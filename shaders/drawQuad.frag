#version 120

uniform sampler2D colormap;

varying vec2 texcoord;

void main()
{


	vec3 color = texture2D(colormap, texcoord).rgb;
	
	//color = vec3(1.0, 0.0, 1.0);

	gl_FragColor = vec4(color, 1.0);
}