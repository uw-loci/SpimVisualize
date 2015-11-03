// renders volume slices

#version 130

uniform isampler3D volumeTexture;


varying vec3 texcoord;

uniform vec3 color;
uniform float sliceWeight = 1.0; 


uniform vec3 viewDir;

uniform int minThreshold;
uniform int maxThreshold;

void main()
{


	int intensity = texture(volumeTexture, texcoord).r;


	// this is the contrast operation -- bring the value into a valid range
	float value = float(intensity - minThreshold) / float(maxThreshold - minThreshold);



	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);



/*
	if (intensity < float(minThreshold) / 65535)
		discard;

	gl_FragColor = vec4(mix(red, blu, 1.0 - intensity), 1.0);

	float alpha = intensity/sliceWeight;
*/


	float alpha = value * 0.01;// * sliceWeight;



	vec3 color = vec3(value);// * viewDir;

	gl_FragColor = vec4(color, alpha);



}