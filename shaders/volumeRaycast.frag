// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	mat4			inverseTransform;
	mat4			transform;
	vec3			bboxMin, bboxMax;
};

#define VOLUMES 2
#define STEPS 500
#define STEP_LENGTH 5.0

uniform Volume volume[VOLUMES];

uniform sampler2D	rayStart;

uniform float		minThreshold;
uniform float		maxThreshold;

uniform mat4		inverseMVP;

in vec2 texcoord;
out vec4 fragColor;


void main()
{
	vec4 finalValue = vec4(0.0);



	// create ray
	if (texture(rayStart, texcoord).a > 0)
	{

		vec3 rayOrigin = texture(rayStart, texcoord).xyz;
		vec4 nearPlane = vec4(texcoord * 2.0 - vec2(1.0), -1.0, 1.0);
		nearPlane = inverseMVP * nearPlane;
		nearPlane /= nearPlane.w;
		vec3 rayDestination = nearPlane.xyz;

		float maxDistance = length(rayDestination - rayOrigin);
		vec3 rayDirection = (rayDestination - rayOrigin) /maxDistance; // = farPlane.xyz;
		rayDirection *= STEP_LENGTH;


		//rayDirection /= float(STEPS);


		float maxValue = 0.0;
		float meanValue = 0.0;
		float distanceTravelled = 0.0;

		vec3 worldPosition = rayOrigin;
		for (int i = 0; i < STEPS; ++i)
		{
			if (distanceTravelled >= maxDistance)
				break;


			float value[VOLUMES];
			

			for (int v = 0; v < VOLUMES; ++v)
			{

				// check all volumes
				vec3 volPosition = vec3(volume[v].inverseTransform * vec4(worldPosition, 1.0));

				if (volPosition.x > volume[v].bboxMin.x && volPosition.x < volume[v].bboxMax.x &&
					volPosition.y > volume[v].bboxMin.y && volPosition.y < volume[v].bboxMax.y &&
					volPosition.z > volume[v].bboxMin.z && volPosition.z < volume[v].bboxMax.z) 
				{

					vec3 volCoord = volPosition - volume[v].bboxMin; 
					volCoord /= (volume[v].bboxMax - volume[v].bboxMin);

					value[v] = texture(volume[v].texture, volCoord).r;
				}
				else
					value[v] = 0.0;

			}


			float mean = 0.0;
			for (int v = 0; v < VOLUMES; ++v)
			{
				// calcualte the max
				maxValue = max(maxValue, value[v]);
				mean += value[v];
			}

			mean /= float(VOLUMES);
			meanValue = max(meanValue, mean);


			worldPosition += rayDirection;
			distanceTravelled += STEP_LENGTH;

		}


		float val = (maxValue - minThreshold) / (maxThreshold - minThreshold);
		//val = (meanValue - minThreshold) / (maxThreshold - minThreshold);
				

		finalValue = vec4(vec3(val), 1.0);

		
	
		//finalValue = vec4(1.0);

	}

	fragColor = finalValue;

	//fragColor = texture(rayEnd, texcoord);
	//fragColor = vec4(texcoord, 0.0, 1.0);
}

