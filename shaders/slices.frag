// renders volume slices

#version 140

uniform isampler3D volumeTexture;


in vec3 texcoord;
in float zDistance;

uniform float sliceWeight; 

uniform float minThreshold;

out vec4 fragColor;

void main()
{

	float intensity = texture(volumeTexture, texcoord).r;

	float alpha = intensity - minThreshold;
	alpha *= sliceWeight;

	vec3 color = vec3(intensity);


	fragColor = vec4(color, alpha);
}