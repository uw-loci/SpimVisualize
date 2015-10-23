// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
	bool			enabled;
};

#define VOLUMES 2
uniform Volume volume[VOLUMES];

uniform float sliceCount = 100;
uniform float sliceWeight;

uniform int minVal = 70;
uniform int maxVal = 100;

uniform int beadThreshold = 150;

in vec4 vertex;
out vec4 fragColor;

void main()
{
	// total intensity of the all visible volume texels at that fragment
	int intensity = 0;


	bool isBead = false;;

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
	
		int t = texture(volume[i].texture, texcoord).r;

		// check if the value is inside
		if (worldPosition.x > volume[i].bboxMin.x && worldPosition.x < volume[i].bboxMax.x &&
			worldPosition.y > volume[i].bboxMin.y && worldPosition.y < volume[i].bboxMax.y &&
			worldPosition.z > volume[i].bboxMin.z && worldPosition.z < volume[i].bboxMax.z) 
		{
			intensity += t; 
			//color += worldPosition;
			weight++;

			if (t > beadThreshold)
				isBead = true;
		}
	}


	intensity /= weight;
	vec3 color = normalize(vec3(float(intensity)));


	float alpha = 1.0 / sliceCount;

	float density = float(intensity - minVal) / float(maxVal - minVal);
	alpha *= density;

	// draw all potential beads brightly yellowS
	if (isBead)
	{

		color = vec3(1.0, 1.0, 0.0);
		alpha = 1.0;
	}
	
	fragColor = vec4(color, alpha);
}