
#version 120

varying vec2 texcoord;

uniform int maxThreshold;
uniform int minThreshold;

uniform sampler2D colormap;

const float maxWeight = 400.0;

void main()
{
	vec3 color = texture2D(colormap, texcoord).rgb;
	float weight = texture2D(colormap, texcoord).a;


	gl_FragColor = vec4(color, 1.0);
}