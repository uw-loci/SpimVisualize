
#version 120

varying vec2 texcoord;

uniform vec4 maxVal;
uniform vec4 minVal;
uniform float sliceCount;

uniform sampler2D colormap;

uniform bool enableDiscard = true;

#define VOLUMES 3



void main()
{
	vec3 color = texture2D(colormap, texcoord).rgb;
	float weight = texture2D(colormap, texcoord).a;


	if (weight == 0.0)
		discard;

	vec3 blu = vec3(0.0, 0.0, 0.8);
	vec3 red = vec3(0.8, 0.0, 0.0);



	float val = color.r;

	color = mix(blu, red, val / 2.0 + 0.5);


	gl_FragColor = vec4(color, 1.0 / float(weight));






}