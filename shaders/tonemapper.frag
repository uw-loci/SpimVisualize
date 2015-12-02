
#version 120

varying vec2 texcoord;

uniform float maxThreshold;
uniform float minThreshold;

uniform sampler2D colormap;

#define VOLUMES 2

void main()
{
	vec3 color = texture2D(colormap, texcoord).rgb;
	float weight = texture2D(colormap, texcoord).a;


	float avgWeight = weight / VOLUMES;
	float w = (avgWeight - minThreshold) / (maxThreshold - minThreshold);

	w = avgWeight / (maxThreshold - minThreshold);

	w = weight / 0.5;



	vec3 blu = vec3(0.0, 0.0, 0.8);
	vec3 red = vec3(0.8, 0.0, 0.0);
	color = mix(blu, red, w);


	gl_FragColor = vec4(color, 1.0);



	if (w < 0.1)
		discard;//color =vec3(0.0);







}