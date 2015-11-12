// renders volume slices

#version 130

in vec3 rayDirection;
in vec3 rayOrigin;


const float stepSize = 0.1;
uniform float maxRayDist;


struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
	mat4			inverseTransform;
	bool			enabled;
};

#define VOLUMES 2
uniform Volume volume[VOLUMES];

out vec4 fragColor;


void main()
{

	vec3 color;


	// total sum of all stuff along the ray
	float sum = 0.0;

	vec3 rayDirNormalized = normalize(rayDirection);

	int iterations = 0;

	float t = 0.0;
	while(t < maxRayDist)
	{
		t += 0.01;

		// world position along the ray
		vec3 position = rayOrigin + rayDirNormalized * t;


		for (int i = 0; i < VOLUMES; ++i)
		{
			if (!volume[i].enabled)
				continue;

			// transform into volume space
			vec4 pos_vol = volume[i].inverseTransform * vec4(position, 1.0);
			pos_vol /= pos_vol.w;

			vec3 v = pos_vol.xyz;

			vec3 texcoord = v - volume[i].bboxMin; 
			texcoord /= (volume[i].bboxMax - volume[i].bboxMin);
		
			int t = texture(volume[i].texture, texcoord).r;

			// check if the value is inside
			if (v.x > volume[i].bboxMin.x && v.x < volume[i].bboxMax.x &&
				v.y > volume[i].bboxMin.y && v.y < volume[i].bboxMax.y &&
				v.z > volume[i].bboxMin.z && v.z < volume[i].bboxMax.z) 
			{
				sum += float(t) / VOLUMES;

				++iterations;
			}


		}

		color = position;

	}

/*
	color = vec3(sum * 200.0);
	color = vec3(float(iterations)/2);
*/

	float alpha = 1.0 / 2.0;
	fragColor = vec4(color, alpha);


}

