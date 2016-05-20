#version 120

uniform mat4 	planeTransform;
uniform float 	zPlane;
uniform vec2 	planeResolution;
uniform vec3 	planeScale;


varying vec4 	worldPosition;

void main()
{
	// clip space coords
	gl_Position = vec4(gl_Vertex.xy * vec2(2.0) - vec2(1.0), 0.0, 1.0);

	// world coords
	vec3 coords = vec3(planeResolution * gl_Vertex.xy, zPlane);
	coords *= planeScale;

	worldPosition = planeTransform * vec4(coords, 1.0);
}