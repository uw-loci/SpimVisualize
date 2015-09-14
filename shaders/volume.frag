// renders volume slices

#version 120

uniform sampler3D volumeTexture;

uniform float minThreshold = 0.6;

varying vec3 texcoord;

uniform vec3 color;


void main()
{
	float intensity = texture3D(volumeTexture, texcoord).r;



	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);





	if (intensity < minThreshold)
		discard;


	gl_FragColor = vec4(mix(red, blu, 1.0 - intensity), 1.0);


	gl_FragColor = vec4(color, 0.5);
}