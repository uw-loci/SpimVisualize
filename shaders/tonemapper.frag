
#version 120

varying vec2 texcoord;

uniform float maxThreshold;
uniform float minThreshold;
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

	/*

	// normalize along ray length
	float intensity = color.r / weight;

	intensity -= minThreshold;

	if (intensity < 0.0 && enableDiscard)
		discard;

	intensity /= (maxThreshold - minThreshold);



	vec3 blu = vec3(0.0, 0.0, 0.8);
	vec3 red = vec3(0.8, 0.0, 0.0);



	if (intensity < 0.01)
		color = vec3(0.0, 1.0, 0.0);

	*/

	gl_FragColor = vec4(color / float(weight), 1.0 / float(weight));






}