

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "AABB.h"
#include "SpimStack.h"
#include "TinyStats.h"

// goddamnit windows! :(
#undef near
#undef far


using namespace std;
using namespace glm;

static mat4 loadRegistration(const string& filename)
{

	ifstream file(filename);

	if (!file.is_open())
		throw runtime_error("Unable to open file \"" + filename + "\"");

	mat4 T(1.f);


	for (int i = 0; i < 16; ++i)
	{
		string buffer;
		getline(file, buffer);
				
#ifdef _WIN32
		int result = sscanf_s(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(T)[i]);
#else
		int result = sscanf(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(T)[i]);
#endif
		
	}

	return T;
}

int main(int argc, const char** argv)
{
	
	// records the individual and aggregate error
	TinyHistory<double> error;


	ofstream output("e:/temp/comparison.csv");
	output << "# stack, transform, solution, distance\n";

	if (argc < 3)
	{
		cout << "Usage: " << argv[0] << " <inputStack> <solutionTransform> [<inputStack> <solutionTransform>] [...]\n";

#ifdef _WIN32
		system("pause");
#endif
		return 1;
	}

	for (int i = 1; i < argc; i += 2)
	{	
		SpimStack* ref = nullptr;

		try
		{
			string stackFile(argv[i+0]);


			// load references (phantoms)
			ref = new SpimStackU16;
			ref->load(stackFile);

			// load their transforms
			string transformFile = stackFile + ".registration.txt";
			ref->loadRegistration(transformFile);


			cout << "Reference:  " << transformFile << endl;
			cout << "Dimensions: " << ref->getVoxelDimensions() * vec3(ref->getResolution()) << endl;

			// load solutions
			string solutionFile(argv[i + 1]);
			mat4 solutionTransform = loadRegistration(solutionFile);
			
			cout << "Solution: " << solutionFile << endl;

			// compare 
			vector<vec3> bboxVertices = ref->getBBox().getVertices();



			// transform
			double aggregatedDistance = 0;
			for (int i = 0; i < 8; ++i)
			{
				vec4 reference = ref->getTransform() * vec4(bboxVertices[i], 1.f);
				vec4 solution = solutionTransform * vec4(bboxVertices[i], 1.f);

				double d = length(reference - solution);

				cout << "Vertex[" << i << "] distance: " << d << endl;
				aggregatedDistance += d;
			}

			aggregatedDistance /= 8;

			error.add(aggregatedDistance);

			cout << "Mean distance: " << aggregatedDistance << endl;
			
			output << stackFile << ", " << transformFile << ", " << solutionFile << ", " << aggregatedDistance << endl;


		}
		catch (const runtime_error& e)
		{
			cerr << e.what() << endl;
		}


		delete ref;
	}


	// calculate difference based on bounding boxes here ...

	



#ifdef _WIN32
	system("pause");
#endif


	return 0;
}

