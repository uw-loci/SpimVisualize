
#version 120

// far map offset
const float EPSILON = 0.0001;

varying vec4 vertex;

void main()
{
	vertex = gl_Vertex;// - vec4(0.0, 0.0, EPSILON, 0.0);
	gl_Position = vertex; //* vec4(0.8, 0.8, 1.0, 1.0);
}

