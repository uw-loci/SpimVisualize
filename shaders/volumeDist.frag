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

in vec4 vertex;
out vec4 fragColor;

void main()
{
	vec3 color;


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

		// check if the value is inside
		if (worldPosition.x > volume[i].bboxMin.x && worldPosition.x < volume[i].bboxMax.x &&
			worldPosition.y > volume[i].bboxMin.y && worldPosition.y < volume[i].bboxMax.y &&
			worldPosition.z > volume[i].bboxMin.z && worldPosition.z < volume[i].bboxMax.z) 
		{
			++count;

			sum += float(t) / VOLUMES;

		}


	}

	if (count == VOLUMES && sum > 120.0)
	{
		vec3 red = vec3(1.0, 0.0, 0.0);
		vec3 green = vec3(0.0, 1.0, 0.0);

		color = mix(green, red, sum/200.0);


	}
	else
		discard;



	float alpha = 1.0 / sliceCount;	
	fragColor = vec4(color, sliceCount);


}