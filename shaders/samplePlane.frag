#version 130

in vec4 worldPosition;




uniform isampler3D 	volume;
uniform mat4 		inverseVolumeTransform;
uniform vec3 		volumeResolution;
uniform vec3		volumeScale;

out vec4 fragColor;

void main()
{




	// 1 -- transform worldPosition to volume coords
	vec3 sampleCoords = vec3(inverseVolumeTransform * worldPosition);
	sampleCoords /= volumeScale;

	// 2 -- to normalized volume coords
	sampleCoords /= volumeResolution;

	float value = 0.0;

	if (sampleCoords.x >= 0.0 && sampleCoords.x <= 1.0 &&
		sampleCoords.y >= 0.0 && sampleCoords.y <= 1.0 &&
		sampleCoords.z >= 0.0 && sampleCoords.z <= 1.0)
	{
		value = float(texture(volume, sampleCoords).r);
	}

	
	fragColor = vec4(worldPosition.xyz, value);

}