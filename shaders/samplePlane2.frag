#version 130
#define VOLUMES 2

in vec4 worldPosition;

struct Volume
{
	isampler3D		volume;
	mat4			inverseTransform;
	vec3			resolution;
	vec3			scale;
	bool			enabled;
};

uniform Volume  	volumes[VOLUMES];


out vec4 fragColor;

void main()
{




	// total sum of all stacks
	float value = 0.0;

	// how many stacks were actually hit
	int counter = 0;

	for (int i = 0; i < VOLUMES; ++i)
	{

		if (!volumes[i].enabled)
			continue;

		// 1 -- transform worldPosition to volume coords
		vec3 sampleCoords = vec3(volumes[i].inverseTransform * worldPosition);
		sampleCoords /= volumes[i].scale;

		// 2 -- to normalized volume coords
		sampleCoords /= volumes[i].resolution;



		if (sampleCoords.x >= 0.0 && sampleCoords.x <= 1.0 &&
			sampleCoords.y >= 0.0 && sampleCoords.y <= 1.0 &&
			sampleCoords.z >= 0.0 && sampleCoords.z <= 1.0)
		{
			value += float(texture(volumes[i].volume, sampleCoords).r);
			counter += 1;
		}
	}

	// avoid division by 0
	counter = max(counter, 1);

	// calculate average value
	value /= float(counter);
	
	fragColor = vec4(worldPosition.xyz, value);

}