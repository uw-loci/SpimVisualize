

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


struct SuperSimpleStack
{
	AABB			bbox;
	mat4			reference;
	mat4			solution;

	// calculates the mean error for all bbox vertices
	double calculateError() const
	{
		double e = 0;

		vector<vec3> vertices = bbox.getVertices();
		for (int i = 0; i < 8; ++i)
		{
			vec4 v(vertices[i], 1.f);
			vec4 r = reference * v;
			vec4 s = solution* v;

			e += glm::distance(r, s);
		}

		return e / 8;
	}


	void load(const string& basefile, int i)
	{
		string stackFile = basefile + to_string(i) + ".tiff";
		string referenceFile = basefile + to_string(i) + ".transform.txt";
		string solutionFile = stackFile + ".registration.txt";

		SpimStack* stack = SpimStack::load(stackFile);
		assert(stack);
		
		bbox = stack->getBBox();
		reference = loadRegistration(referenceFile);
		solution = loadRegistration(solutionFile);
		
		delete stack;
	}

	void transform(const mat4& m)
	{
		reference = m * reference;
		solution = m * solution;
	}

};





int main(int argc, const char** argv)
{	
	vector<SuperSimpleStack> solutions;
	
	// load the data
	for (int i = 1; i <= 4; ++i)
	{
		try
		{
			SuperSimpleStack stack;
			stack.load("e:/spim/phantom/phantom_", i);

			solutions.push_back(stack);
		}
		catch (const runtime_error& e)
		{
			cerr << "[Error:] " << e.what() << endl;
		}
	}


	if (solutions.empty())
	{
		cerr << "[Error] No valid solutions found!\n";
		

#ifdef _WIN32
		system("pause");
#endif
		
		
		exit(1);
	}

	// offset all transformation in relation to the first
	const mat4 roi = inverse(solutions[0].reference);
	for (size_t i = 0; i < solutions.size(); ++i)
	{
		solutions[i].transform(roi);
	

		cout << "Reference  [" << i << "]:" << solutions[i].reference << endl;
		cout << "Solution   [" << i << "]:" << solutions[i].solution << endl;
	}
	

	// records the individual and aggregate error
	TinyHistory<double> error;

	for (size_t i = 0; i < solutions.size(); ++i)
	{
		double e = solutions[i].calculateError();
		cout << "Error      [" << i << "]:" << e << endl;

		error.add(e);
	}

	cout << "Mean error [" << error.getMean() << "]:" << endl;
	cout << "Mean RMS   [" << error.getRMS() << "]:" << endl;








#ifdef _WIN32
	system("pause");
#endif


	return 0;
}

