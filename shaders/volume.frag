#version 130

uniform sampler3D volumeTexture;

uniform float minThreshold = 0.01;
uniform float maxThreshold = 0.2;

varying vec3 texcoord;


void main()
{
	float intensity = texture3D(volumeTexture, texcoord).r;



	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);



	if (intensity < minThreshold || intensity > maxThreshold)
		discard;


	vec4 color = vec4(mix(blu, red, (intensity - minThreshold) / (maxThreshold - minThreshold)), 1.0);


	color = vec4(intensity * 20.0);


	gl_FragColor = color;
	
}