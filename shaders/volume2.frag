// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
	bool			enabled;
};

#define VOLUMES 3
uniform Volume volume[VOLUMES];

uniform float minThreshold;
uniform float maxThreshold;

in vec4 vertex;
out vec4 fragColor;

void main()
{
	// total intensity of the all visible volume texels at that fragment
	float intensity = 0.0;

	// weight of all visible volume texels at that fragment
	int weight = 0;
	for (int i = 0; i < VOLUMES; ++i)
	{
		if (!volume[i].enabled)
			continue;

		vec4 v = volume[i].inverseMVP * vertex;
		v /= v.w;

		vec3 worldPosition = v.xyz;

		vec3 texcoord = worldPosition - volume[i].bboxMin; // vec3(1344.0, 1024.0, 101.0);
		texcoord /= (volume[i].bboxMax - volume[i].bboxMin);
	
		float t = texture(volume[i].texture, texcoord).r;

		// check if the value is inside
		if (worldPosition.x > volume[i].bboxMin.x && worldPosition.x < volume[i].bboxMax.x &&
			worldPosition.y > volume[i].bboxMin.y && worldPosition.y < volume[i].bboxMax.y &&
			worldPosition.z > volume[i].bboxMin.z && worldPosition.z < volume[i].bboxMax.z)
		{

			//t -= minThreshold;

			intensity += t; 
			weight++;
		}


	}

	if (weight == 0)
		discard;

	// normalize value based on how many volumes it is in
	intensity /= weight;
	fragColor = vec4(vec3(intensity), 1.0);
}