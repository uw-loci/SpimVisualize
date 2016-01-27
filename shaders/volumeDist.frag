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

uniform bool enableDiscard = true;
int value[VOLUMES];
vec3 colors[VOLUMES];

uniform float minThreshold;
uniform float maxThreshold;

uniform float sliceCount;

in vec4 vertex;
out vec4 fragColor;

void main()
{	

	// init colors here
	colors[0] = vec3(1.0, 0.0, 0.0);
	colors[1] = vec3(0.0, 1.0, 0.0);
	//colors[2] = vec3(0.0, 0.0, 1.0);

	/*
	if (VOLUMES > 1)
		colors[2] = vec3(0.0, 0.0, 1.0);
	if (VOLUMES > 2)
		colors[3] = vec3(1.0, 1.0, 0.0);
	if (VOLUMES > 3)
		colors[4] = vec3(0.0, 1.0, 1.0);
	*/


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

	
	vec3 color = vec3(0.0);
	for (int i = 0; i < VOLUMES; ++i)
	{
		if (value[i] > minThreshold)
			color += colors[i];
	}


	/*
	bool anyGreater = false;
	for (int i = 0; i < VOLUMES; ++i)
	{

		if (value[i] > minThreshold)
		{			
			color += colors[i];
			anyGreater = true;
		}

		if (enableDiscard && value[i] < minThreshold)
			discard;
	}

	if (!anyGreater)
		discard;
	*/

	// average value
	sum /= float(count);


	// scale value by contrast settings
	sum -= minThreshold;
	sum /= (maxThreshold - minThreshold);

	color *= sum;
	float alpha = float(count);

	fragColor = vec4(color, alpha);

}
