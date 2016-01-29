// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
	bool			enabled;
};

// the #define VOLUMES should be set externally
#ifndef VOLUMES
#define VOLUMES 2
#endif

uniform Volume volume[VOLUMES];

int value[VOLUMES];

uniform float minThreshold;
uniform float maxThreshold;

uniform float sliceCount;

in vec4 vertex;
out vec4 fragColor;

void main()
{	

	// count of the number volumes this pixel is contained int
	int count = 0;

	// total intensity of all volumes
	float sum = 0.0;

	for (int i = 0; i < VOLUMES; ++i)
	{
		if (!volume[i].enabled)
			continue;

		vec4 v = volume[i].inverseMVP * vertex;
		v /= v.w;

		vec3 worldPosition = v.xyz;

		vec3 texcoord = worldPosition - volume[i].bboxMin; 
		texcoord /= (volume[i].bboxMax - volume[i].bboxMin);
	
		int t = texture(volume[i].texture, texcoord).r;

		value[i] = 0;

		// check if the value is inside
		if (worldPosition.x > volume[i].bboxMin.x && worldPosition.x < volume[i].bboxMax.x &&
			worldPosition.y > volume[i].bboxMin.y && worldPosition.y < volume[i].bboxMax.y &&
			worldPosition.z > volume[i].bboxMin.z && worldPosition.z < volume[i].bboxMax.z) 
		{			

			if (float(t) >= minThreshold)
			{
				value[i] = t;
				++count;
				sum += float(t);
			}
		}
	

	}


	if (count == 0)
		discard;

	

	/*
	bool anyGreater = false;
	for (int i = 0; i < VOLUMES; ++i)
	{

		if (value[i] > minThreshold)
		{			
			diffValue += (value[i] - value[0]);
			anyGreater = true;
		}

		if (value[i] < minThreshold)
			discard;
	}

	if (!anyGreater)
		discard;
	*/


float mean = 0.0;
#if (VOLUMES == 2)
	mean = value[1];
#endif

#if (VOLUMES == 3)
	mean = float(value[1] + value[2]) / 2.0;
#endif

#if (VOLUMES == 4)
	mean = float(value[1] + value[2] + value[3]) / 3.0;
#endif

#if (VOLUMES == 5)
	float(value[1] + value[2] + value[3] + value[4]) / 4.0;
#endif
	
	float diffValue = mean - value[0];

	vec3 color = vec3(diffValue, sum, 0.0);


	/*
	// average value
	sum /= float(count);


	// scale value by contrast settings
	sum -= minThreshold;
	sum /= (maxThreshold - minThreshold);

	color *= sum;
	*/

	color /= 200.0;
	color /= 11.0;

	fragColor = vec4(color, count);
}
