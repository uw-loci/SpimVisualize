// renders volume slices

#version 140

uniform isampler3D volumeTexture;


in vec3 texcoord;
in float zDistance;

uniform float sliceWeight; 

uniform int maxThreshold;
uniform int minThreshold;


out vec4 fragColor;

void main()
{

	int intensity = texture(volumeTexture, texcoord).r;

	float alpha = sliceWeight;// * float(intensity);
	vec3 color = vec3(float(intensity - minThreshold) / float(maxThreshold-minThreshold));

	fragColor = vec4(color, sliceWeight);
}