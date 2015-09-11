// renders volume slices

#version 130

uniform sampler3D volumeTexture;

uniform float minThreshold = 0.6;

varying vec3 texcoord;
varying vec3 worldPosition;

uniform vec3 bboxMax;
uniform vec3 bboxMin;

void main()
{
	float intensity = texture3D(volumeTexture, texcoord).r;


	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);


	vec3 color = vec3(intensity);
	

	color = mix(blu, red, texcoord.z);
	color = worldPosition;

	if (worldPosition.x > bboxMin.x && worldPosition.x < bboxMax.x &&
		worldPosition.y > bboxMin.y && worldPosition.y < bboxMax.y &&
		worldPosition.z > bboxMin.z && worldPosition.z < bboxMax.z)
		//color = vec3(0.0, 1.0, 0.0);
		color = normalize(worldPosition);
	else
		discard;

	gl_FragColor = vec4(color, 0.2);
}