#version 120

uniform sampler3D volumeTexture;

varying vec3 texcoord;

void main()
{
	float intensity = texture3D(volumeTexture, texcoord).r;

	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);


	if (intensity < 0.001)
		discard;

	if (intensity > 0.002)
		discard;


	vec4 color = vec4(mix(blu, red, intensity), 1.0);


	gl_FragColor = color;
	
}