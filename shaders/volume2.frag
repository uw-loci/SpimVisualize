// renders volume slices

#version 130

struct Volume
{
	isampler3D		texture;
	vec3			bboxMin, bboxMax;
	mat4			inverseMVP;
};

uniform Volume volume[3];

uniform float sliceCount = 100;
uniform float minThreshold = 0.6;

uniform float sliceWeight;

varying vec4 vertex;

void main()
{
	vec3 color = vec3(0.0);
	float intensity = 0.0;

	for (int i = 0; i < 1; ++i)
	{
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
			intensity += float(t) / 65535.0;
			color += worldPosition;
		
		}
	}


	color = normalize(color);

	/*
	// normalize the texcoords based on the bbox
	vec3 texcoord = worldPosition - bboxMin; // vec3(1344.0, 1024.0, 101.0);
	texcoord /= (bboxMax - bboxMin);

	float intensity = texture3D(volumeTexture, texcoord).r;

	intensity *= 100.0;

	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);


	vec3 color = vec3(intensity);

	
	if (worldPosition.x > bboxMin.x && worldPosition.x < bboxMax.x &&
		worldPosition.y > bboxMin.y && worldPosition.y < bboxMax.y &&
		worldPosition.z > bboxMin.z && worldPosition.z < bboxMax.z) 
	{
			//color = vec3(0.0, 1.0, 0.0);
		
		color = vec3(intensity);
		color = normalize(worldPosition);
		color = normalize(worldPosition) * intensity;
		color = (texcoord);
		

	}
	else
		discard;
	*/
	
	gl_FragColor = vec4(color, intensity);
}