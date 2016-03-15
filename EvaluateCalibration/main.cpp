

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

#include <glm/glm.hpp>

// goddamnit windows! :(
#undef near
#undef far


using namespace std;
using namespace glm;

int main(int argc, const char** argv)
{

	vector<mat4>	referenceTransforms;
	vector<mat4>	solutionTransforms;

	for (int i = 1; i < argc; ++i)
	{

		// load references



		// load solutions


	}


	// calculate difference based on bounding boxes here ...

	



#ifdef _WIN32
	system("pause");
#endif


	return 0;
}

