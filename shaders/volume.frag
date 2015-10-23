// renders volume slices

#version 130

uniform isampler3D volumeTexture;

uniform int beadThreshold = 150;

varying vec3 texcoord;

uniform vec3 color;
uniform float sliceWeight = 1.0; 


uniform vec3 viewDir;

uniform int minVal = 80;
uniform int maxVal = 120;

void main()
{


	int intensity = texture(volumeTexture, texcoord).r;


	// this is the contrast operation -- bring the value into a valid range
	float value = float(intensity - minVal) / float(maxVal - minVal);





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

	if (intensity > beadThreshold)
		color = vec3(1.0, 1.0, .0);

	gl_FragColor = vec4(color, alpha);



}