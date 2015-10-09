// renders volume slices

#version 130

uniform sampler3D volumeTexture;

uniform int minThreshold = 200;

varying vec3 texcoord;

uniform vec3 color;
uniform float sliceWeight = 1.0;


void main()
{
	float intensity = texture(volumeTexture, texcoord).r;



	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);



/*
	if (intensity < float(minThreshold) / 65535)
		discard;
*/
	
	gl_FragColor = vec4(mix(red, blu, 1.0 - intensity), 1.0);

	float alpha = intensity/sliceWeight;

	gl_FragColor = vec4(color, alpha);
}