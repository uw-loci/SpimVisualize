// renders volume slices

#version 120

uniform sampler3D volumeTexture;

uniform float sliceCount = 100;
uniform float minThreshold = 0.6;

varying vec3 worldPosition;

uniform vec3 bboxMax;
uniform vec3 bboxMin;

void main()
{
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
		worldPosition.z > bboxMin.z && worldPosition.z < bboxMax.z &&
		intensity > minThreshold)
	{
			//color = vec3(0.0, 1.0, 0.0);
		
		color = vec3(intensity);
		color = normalize(worldPosition);
		color = normalize(worldPosition) * intensity;
		color = (texcoord);
		

	}
	else
		discard;
	


	gl_FragColor = vec4(color, intensity / sliceCount);
}