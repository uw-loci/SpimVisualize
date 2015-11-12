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

uniform float sliceCount;
uniform float sliceWeight;

uniform int minThreshold;
uniform int maxThreshold;


in vec4 vertex;
out vec4 fragColor;

void main()
{
	vec3 color;

	// total intensity of the all visible volume texels at that fragment
	int intensity = 0;

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





			vec3 localColor = vec3(1.0, 0.0, 0.0);
			if (i == 1)
				localColor = vec3(0.0, 1.0, 0.0);
		
			color += localColor * intensity;

		}


	}


	intensity /= weight;

	//color *= intensity;

	/*
	vec3 color = normalize(vec3(float(intensity)));

	if (intensity > maxThreshold)
		vec3 color = vec3(0.0, 1.0, 0.0);
	if (intensity < minThreshold)
		vec3 color = vec3(1.0, 0.0, 0.0);
	*/

	float alpha = 4.0 / sliceCount;

	float density = float(intensity - minThreshold) / float(maxThreshold - minThreshold);
	alpha *= density;
	
	fragColor = vec4(color, alpha);
}