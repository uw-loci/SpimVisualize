#version 120

uniform sampler2D image;

// TODO: replace with transform function
uniform float intensity = 50.0;

varying vec2 texcoord;

void main()
{
	vec3 color = vec3(1.0, 1.0, 0.0);
	float alpha = texture2D(image, texcoord).r * intensity;

	gl_FragColor = vec4(color, alpha);
}