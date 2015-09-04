#version 330

in float intensity;

out vec4 fragColor;

void main()
{
	vec3 red = vec3(1.0, 0.0, 0.0);
	vec3 blu = vec3(0.0, 0.0, 1.0);


	fragColor = vec4(mix(blu, red, intensity), 1.0);

}