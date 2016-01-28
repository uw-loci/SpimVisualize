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

uniform int currentVolume = 0;

uniform bool enableDiscard = true;
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

	
	int diffValue = 0;
	bool anyGreater = false;
	for (int i = 0; i < VOLUMES; ++i)
	{

		if (value[i] > minThreshold)
		{			
			diffValue += (value[i] - value[0]);
			anyGreater = true;
		}

		if (enableDiscard && value[i] < minThreshold)
			discard;
	}

	if (!anyGreater)
		discard;



	// normalize diffvalue
	float normalDiff = diffValue / sliceCount;



	vec3 color = vec3(normalDiff, sum, sum/sliceCount);

	/*
	color.r /= 200.0;
	color.r /= 11.0;
	*/

	fragColor = vec4(color, float(count));

}
