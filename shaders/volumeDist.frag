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

uniform bool enableDiscard = true;
int value[VOLUMES];

in vec4 vertex;
out vec4 fragColor;

void main()
{
	vec3 color = vec3(0.0);


	// count of the number volumes this pixel is contained int
	int count = 0;


	// total sum of all points
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


			value[i] = t;

			++count;

			sum += float(t);

		}
	

	}



	if (count == 0)
		discard;


	const int THRESHOLD = 115;
	if (value[0] > THRESHOLD)
		color = vec3(1.0, 0.0, 0.0);

	if (value[1] > THRESHOLD)
		color += vec3(0.0, 1.0, 0.0);

	if (value[0] < THRESHOLD && value[1] < THRESHOLD)
		discard;




	if (enableDiscard)
	{

	if (value[0] < THRESHOLD || value[1] < THRESHOLD)
		discard;


	}

	float alpha = 2.0 / sliceCount;	
	fragColor = vec4(color, alpha);


}
