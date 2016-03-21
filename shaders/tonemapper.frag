
#version 120

varying vec2 texcoord;

uniform vec4 maxVal;
uniform vec4 minVal;
uniform float sliceCount;

uniform float minThreshold;
uniform float maxThreshold;

uniform sampler2D colormap;

uniform bool enableDiscard = true;

#define VOLUMES 3



void main()
{
	vec3 color = texture2D(colormap, texcoord).rgb;
	float val = texture2D(colormap, texcoord).a;

	if (val < minThreshold)
		discard;


	vec3 blu = vec3(0.0, 0.0, 0.8);
	vec3 red = vec3(0.8, 0.0, 0.0);

	float delta = (val - minThreshold) / (maxThreshold - minThreshold);
	color = mix(blu, red, delta);


	/*
	// max difference
	float diff = color.r / 20.0; // / 4000.0;
	color = vec3(diff);
	*/

	gl_FragColor = vec4(color, 1.0);






}