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
#define STEPS 1000
#define STEP_LENGTH 2.0
#define MAX_DISTANCE 2000

uniform Volume volume[VOLUMES];


uniform sampler2D	rayStart;
uniform sampler2D	rayEnd;


uniform float		minThreshold;
uniform float		maxThreshold;

uniform mat4		inverseMVP;

in vec2 texcoord;
out vec4 fragColor;


void main()
{
	vec4 finalValue = vec4(0.0);





	// create ray
	if (texture(rayEnd, texcoord).a > 0)
	{

		vec3 rayOrigin; // = nearPlane.xyz;
		vec3 rayDirection; // = farPlane.xyz;


		if (texture(rayStart, texcoord).a > 0)
		{
			rayOrigin = texture(rayStart, texcoord).xyz;
			rayDirection = texture(rayEnd, texcoord).xyz;
			
		}
		else
		{

			vec4 nearPlane = vec4(texcoord * 2.0 - vec2(1.0), -1.0, 1.0);
			vec4 farPlane = vec4(texcoord * 2.0 - vec2(1.0), 1.0, 1.0);

			nearPlane = inverseMVP * nearPlane;
			nearPlane /= nearPlane.w;

			farPlane = inverseMVP * farPlane;
			farPlane /= farPlane.w;

			rayOrigin = nearPlane.xyz;
			rayDirection = farPlane.xyz;

		}


		rayDirection -= rayOrigin;

		rayDirection = normalize(rayDirection);
		rayDirection *= STEP_LENGTH;


		//rayDirection /= float(STEPS);


		float maxValue = 0.0;

		float distanceTravelled = 0.0;

		vec3 worldPosition = rayOrigin;
		for (int i = 0; i < STEPS; ++i)
		{
			if (distanceTravelled > MAX_DISTANCE)
				break;


			float value = 0.0;
			
			
			// check all volumes
			vec3 volPosition = vec3(volume[0].inverseTransform * vec4(worldPosition, 1.0));

			if (volPosition.x > volume[0].bboxMin.x && volPosition.x < volume[0].bboxMax.x &&
				volPosition.y > volume[0].bboxMin.y && volPosition.y < volume[0].bboxMax.y &&
				volPosition.z > volume[0].bboxMin.z && volPosition.z < volume[0].bboxMax.z) 
			{

				vec3 volCoord = volPosition - volume[0].bboxMin; 
				volCoord /= (volume[0].bboxMax - volume[0].bboxMin);


				value = texture(volume[0].texture, volCoord).r;
				//values[v] = 1;
			}
			else
				value = 0.0;

			maxValue = max(maxValue, value);


			worldPosition += rayDirection;
			distanceTravelled += STEP_LENGTH;
		}



		float val = (maxValue - minThreshold) / (maxThreshold - minThreshold);



		finalValue = vec4(vec3(val), 1.0);
		//finalValue = vec4(rayDirection, 1.0);

		//finalValue = vec4(rayOrigin, 1.0);
		//finalValue = vec4(1.0);

	}

	fragColor = finalValue;


	//fragColor = vec4(texcoord, 0.0, 1.0);
}

