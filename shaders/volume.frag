// renders volume slices

#version 140

uniform isampler3D volumeTexture;


in vec3 texcoord;

uniform float sliceWeight; 

uniform vec3 color;

uniform vec3 viewDir;

uniform int minThreshold;
uniform int maxThreshold;

out vec4 fragColor;

void main()
{


	int intensity = texture(volumeTexture, texcoord).r;


	// this is the contrast operation -- bring the value into a valid range
	float value = float(intensity - minThreshold) / float(maxThreshold - minThreshold);



	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);


	float alpha = value * max(sliceWeight, 0.1);


	vec3 v = viewDir * 2.0;


	fragColor = vec4(color, alpha);


}