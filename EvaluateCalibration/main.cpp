

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


	float* m = value_ptr(T);

	for (int i = 0; i < 16; ++i)
		file >> m[i];


	/*
	cout << "Read matrix: \"" << filename << "\"\n";
	cout << T << endl;
	*/
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


	// parse command line here
	vector<pair<string, string>> filepairs;
	for (int i = 1; i < argc; i += 2)
	{
		filepairs.push_back(make_pair(argv[i], argv[i + 1]));
	}





	for (auto it = filepairs.begin(); it != filepairs.end(); ++it)
	{	
		SpimStack* ref = nullptr;

		try
		{
			const string stackFile(it->first);
			const string solutionFile(it->second);

			// load references (phantoms)
			ref = new SpimStackU16;
			ref->load(stackFile);

			// load their transforms
			const string transformFile = stackFile + ".registration.txt";
			ref->setTransform(loadRegistration(transformFile));


			cout << "Reference:  " << transformFile << endl;
			cout << "Dimensions: " << ref->getVoxelDimensions() * vec3(ref->getResolution()) << endl;

			// load solutions
			const mat4 solutionTransform = loadRegistration(solutionFile);
		
			// compare 
			const vector<vec3> bboxVertices = ref->getBBox().getVertices();
			

			// transform
			double aggregatedDistance = 0;
			for (int i = 0; i < 8; ++i)
			{

				vec4 reference = ref->getTransform() * vec4(bboxVertices[i], 1.f);
				vec4 solution = solutionTransform * vec4(bboxVertices[i], 1.f);
				
				double d = length(reference - solution);

				//cout << bboxVertices[i] << " -> " << reference << " - " << solution << " = " << d << endl;

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

	
#ifdef _WIN32
	system("pause");
#endif


	return 0;
}

