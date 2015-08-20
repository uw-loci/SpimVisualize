#version 120

uniform sampler2D image;

// TODO: replace with transform function
uniform float intensity = 100.0;

varying vec2 texcoord;

void main()
{
	vec3 color = vec3(1.0, 1.0, 0.0);
	float alpha = texture2D(image, texcoord).r * intensity;

	if (alpha < 0.1)
		color = vec3(1.0, 0.0, 0.0);

	if (alpha < 0.1)
		discard;

	gl_FragColor = vec4(color, alpha);
}