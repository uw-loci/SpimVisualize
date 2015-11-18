// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
	bool			enabled;
};

#define VOLUMES 5
uniform Volume volume[VOLUMES];

uniform float sliceCount = 100;
uniform float sliceWeight;

uniform bool enableDiscard = true;
int value[VOLUMES];

vec3 colors[VOLUMES];

in vec4 vertex;
out vec4 fragColor;

void main()
{
	vec3 color = vec3(0.0);


	// init colors here
	colors[0] = vec3(1.0, 0.0, 0.0);
	colors[1] = vec3(0.0, 1.0, 0.0);
	if (VOLUMES > 1)
		colors[2] = vec3(0.0, 0.0, 1.0);
	if (VOLUMES > 2)
		colors[3] = vec3(1.0, 1.0, 0.0);
	if (VOLUMES > 3)
		colors[4] = vec3(0.0, 1.0, 1.0);


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

	bool anyGreater = false;
	for (int i = 0; i < VOLUMES; ++i)
	{

		if (value[i] > THRESHOLD)
		{
			//color[i] += 1.0;
			
			color += colors[i];
			anyGreater = true;
		}

		if (enableDiscard && value[i] < THRESHOLD)
			discard;
	}

	if (!anyGreater)
		discard;

	color = normalize(color);


	float alpha = 5.0 / sliceCount;	
	fragColor = vec4(color, alpha);


}
