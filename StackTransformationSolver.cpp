#include "StackTransformationSolver.h"

#include <cassert>
#include <algorithm>
#include <random>
#include <iostream>
#include <chrono>
#include <fstream>

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/transform.hpp>

#include "Framebuffer.h"
#include "InteractionVolume.h"

/// returns a uniform random variable in [-1..1]
static inline double rng_u(std::mt19937& rng)
{
	return ((double)rng() / rng.max() * 2) - 1;
}

/*
void IStackTransformationSolver::recordCurrentScore(Framebuffer* fbo)
{
	fbo->bind();
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	std::vector<glm::vec4> pixels(fbo->getWidth()*fbo->getHeight());
	glReadPixels(0, 0, fbo->getWidth(), fbo->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
	fbo->disable();

	double value = 0;

	for (size_t i = 0; i < pixels.size(); ++i)
	{
		glm::vec3 color(pixels[i]);
		value += glm::dot(color, color);
	}

	value /= (fbo->getWidth()*fbo->getHeight());

	std::cout << "[Solver] Read back score: " << value << std::endl;
	glReadBuffer(GL_BACK);

	recordCurrentScore(value);
}
*/

glm::mat4 IStackTransformationSolver::createRotationMatrix(float angle, const InteractionVolume* v)
{
	using namespace glm;
	mat4 matrix(1.f);
	// this rotates the volume around its center
	matrix = translate(mat4(1.f), v->getBBox().getCentroid());
	matrix = rotate(matrix, angle, glm::vec3(0, 1, 0));
	matrix = translate(matrix, v->getBBox().getCentroid() * -1.f);

	return matrix;
}


void UniformSamplingSolver::initialize(const InteractionVolume* v)
{
	resetSolution();
	
	createCandidateSolutions(v);
	
	if (!solutions.empty())
		currentSolution = 0;

	history.reset();
}

void UniformSamplingSolver::resetSolution()
{
	solutions.clear();
	currentSolution = -1;
}

bool UniformSamplingSolver::nextSolution()
{
	if (solutions.empty())
		return false;

	++currentSolution;
	if (currentSolution == (int)solutions.size())
	{
		// reset to last valid solution
		--currentSolution;
		return false;
	}

	return true;
}

bool UniformSamplingSolver::hasValidCurrentSolution() const
{
	return !solutions.empty() && (currentSolution >= 0) && (currentSolution < (int)solutions.size());
}

void UniformSamplingSolver::recordCurrentScore(double s)
{
	if (hasValidCurrentSolution())
	{
		std::cout << "[Solver] Recording score of " << s << " for current solution.\n";
		solutions[currentSolution].score = s;
	
		history.add(s);
	}
}

const IStackTransformationSolver::Solution& UniformSamplingSolver::getCurrentSolution() const
{
	if (!hasValidCurrentSolution())
		throw std::runtime_error("Solver has no valid current solution");
	
	return solutions[currentSolution];
}

const IStackTransformationSolver::Solution& UniformSamplingSolver::getBestSolution()
{
	if (solutions.empty())
		throw std::runtime_error("Solver has no valid solutions!");
		
	// remove all solutions that do not have a valid id. we can do that through the current solution?
	size_t oldSize = solutions.size();
	if (currentSolution < (int)solutions.size() - 1)
		solutions.resize(currentSolution);
	std::cout << "[Debug] Removed " << oldSize - solutions.size() << " unused solutions.\n";
	

	std::cout << "[Debug] Solutions: ";
	for (size_t i = 0; i < solutions.size(); ++i)
		std::cout << solutions[i].score << std::endl;
	std::cout << "[Debug] --------------\n";

	std::cout << "[Debug] Sorting " << solutions.size() << " solutions ... \n";
	std::sort(solutions.begin(), solutions.end());
	

	std::cout << "[Debug] Solutions: ";
	for (size_t i = 0; i < solutions.size(); ++i)
		std::cout << solutions[i].score << std::endl;
	std::cout << "[Debug] --------------\n";


	std::cout << "[Debug] Worst: " << solutions.front().score << ", best: " << solutions.back().score << std::endl;
	
	return solutions.back();
}


void DXSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;
	std::mt19937 rng((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dx = (float)x / 2.f;
		Solution s;
		s.score = 0;
		s.id = x;
		s.matrix = translate(vec3(dx, 0.f, 0.f));

		solutions.push_back(s);
	}

	std::cout << "[DX Solver] Created " << solutions.size() << " candidate solutions.\n";
}

void DYSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;
	std::mt19937 rng((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dy = (float)x / 2.f;
		Solution s;
		s.score = 0;
		s.id = x;
		s.matrix = translate(vec3(0.f, dy, 0.f));

		solutions.push_back(s);
	}


	std::cout << "[DY Solver] Created " << solutions.size() << " candidate solutions.\n";
}

void DZSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;
	std::mt19937 rng((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dz = (float)x / 2.f;
		Solution s;
		s.score = 0;
		s.id = x;
		s.matrix = translate(vec3(0.f, 0.f, dz));

		solutions.push_back(s);
	}


	std::cout << "[DZ Solver] Created " << solutions.size() << " candidate solutions.\n";
}

void RYSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;

	// try different rotations
	for (int r = -20; r <= 20; ++r)
	{
		float a = radians((float)r / 5.f);

		Solution s;
		s.id = r;
		s.score = 0;

		//s.matrix = createRotationMatrix(a, v);

		/*
		mat4 I = v->getTransform();
		mat4 T = translate(v->getBBox().getCentroid());

		// simplified from: I*T* rot * I-1*I
		mat4 R = I * T * rotate(a, glm::vec3(0,1,0)) * inverse(T);
		s.matrix = R;
		*/

		//mat4 I = v->getTransform();
		mat4 I = translate(v->getTransformedBBox().getCentroid());
		mat4 R = rotate(a, vec3(0, 1, 0));

		s.matrix = I * R * inverse(I);
		solutions.push_back(s);

	}


	std::cout << "[RY Solver] Created " << solutions.size() << " candidate solutions.\n";
}

void RandomRotationSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;

	// creates a random point on a sphere
	// see: http://math.stackexchange.com/questions/114135/how-can-i-pick-a-random-point-on-the-surface-of-a-sphere-with-equal-distribution
	std::mt19937 rng(time(0));
	float phi = 2.f * glm::pi<float>() * (float)rng() / rng.max();
	float theta = acos(2*((float)rng()/rng.max()) - 1.f);


	vec3 axis;
	axis.x = cos(theta) *sin(phi);
	axis.y = sin(theta)*sin(phi);
	axis.z = cos(theta);


	// try different rotations
	for (int r = -20; r <= 20; ++r)
	{
		float a = radians((float)r / 5.f);

		Solution s;
		s.id = r;
		s.score = 0;

		s.matrix = translate(v->getTransform(), v->getBBox().getCentroid());
		s.matrix = rotate(s.matrix, a, axis);
		s.matrix = translate(s.matrix, v->getBBox().getCentroid() * -1.f);

		//s.matrix = rotate(a, vec3(0, 1, 0));
		solutions.push_back(s);

	}


}


void UniformScaleSolver::createCandidateSolutions(const InteractionVolume* v)
{
	for (int i = -20; i <= 20; ++i)
	{

		// from -0.5 to +0.5

		float factor = 1.f + (float)i / 40.f;

	
		Solution s;
		s.id = i;
		s.score = 0;

		s.matrix = translate(v->getTransform(), v->getBBox().getCentroid());
		s.matrix = scale(s.matrix, glm::vec3(factor));
		s.matrix = translate(s.matrix, v->getBBox().getCentroid() * -1.f);


		solutions.push_back(s);

	}

}


MultiDimensionalHillClimb::MultiDimensionalHillClimb(const std::vector<InteractionVolume*>& v) : volumes(v), solutionCounter(0)
{
}


void MultiDimensionalHillClimb::initialize(const InteractionVolume* v)
{
	// initialize rng
	rng = std::mt19937((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());;

	resetSolution();
	history.reset();

	// ignore this
	//currentVolume = v;
	currentVolume = nullptr;
	createPotentialSolutions();
}

void MultiDimensionalHillClimb::resetSolution()
{
	potentialSolutions.clear();
	bestSolution.score = 0;
	bestSolution.matrix = glm::mat4(1.f);
	currentVolume = nullptr;
}

bool MultiDimensionalHillClimb::nextSolution()
{
	potentialSolutions.pop_back();
	if (potentialSolutions.empty())
	{
		std::cout << "[Hillclimb] Best score in last run was " << bestSolution.score << std::endl;

		// select a new active volume
		if (volumes.empty())
			throw std::runtime_error("MDHillclimb has got an empty set of potential volumes!");

    	currentVolume = 0;
		while (!currentVolume)
		{
			currentVolume = volumes[rng() % volumes.size()];
			if (currentVolume->locked)
				currentVolume = 0;
		}

		std::cout << "[Hillclimb] Selected new volume: " << currentVolume << std::endl;


		createPotentialSolutions();
	}
	return true;
}

const IStackTransformationSolver::Solution& MultiDimensionalHillClimb::getCurrentSolution() const
{
	if (potentialSolutions.empty())
		return bestSolution;
	else
		return potentialSolutions.back();
}

void MultiDimensionalHillClimb::recordCurrentScore(double score)
{
	if (score > bestSolution.score)
	{
		bestSolution = potentialSolutions.front();
		history.add(score);
	}
}

void MultiDimensionalHillClimb::createPotentialSolutions()
{
	using namespace glm;

	int mode = rng() % 4;

	std::cout << "[Hillclimb] Mode: " << mode << std::endl;
	
	if (mode == 0)
	{
		for (int i = -10; i <= 10; ++i)
		{
			float f = (float)i / 5.f;

			Solution s;
			s.id = solutionCounter++;
			s.matrix = translate(vec3(f, 0, 0));
			s.score = 0;
			potentialSolutions.push_back(s);
		}
	}
	else if (mode == 1)
	{
		for (int i = -10; i <= 10; ++i)
		{
			float f = (float)i / 5.f;

			Solution s;
			s.id = solutionCounter++;
			s.matrix = translate(vec3(0, f, 0));
			s.score = 0;
			potentialSolutions.push_back(s);
		}
	}
	else if (mode == 2)
	{
		for (int i = -10; i <= 10; ++i)
		{
			float f = (float)i / 5.f;

			Solution s;
			s.id = solutionCounter++;
			s.matrix = translate(vec3(0, 0, f));
			s.score = 0;
			potentialSolutions.push_back(s);
		}
	}
	else if (mode == 3)
	{
		if (!currentVolume)
			throw std::runtime_error("No current interaction volume set for the multidim hillclimb!");

		for (int i = -10; i <= 10; ++i)
		{
			float f = radians((float)i / 5.f);

			Solution s;
			s.id = solutionCounter++;
			s.matrix = createRotationMatrix(f, currentVolume);

			s.score = 0;
			potentialSolutions.push_back(s);
		}
	}

	std::cout << "[Hillclimb] " << potentialSolutions.size() << " new solutions in queue.\n";
}

void SimulatedAnnealingSolver::initialize(const InteractionVolume* v)
{
	temp = 1;
	cooling = 0.998;

	currentSolution.score = 0;
	currentSolution.matrix = v->getTransform();
	bestSolution = currentSolution;

	iteration = 0;
	currentVolume = v;

	// initialize rng
	rng = std::mt19937((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());;

	history.reset();
}

void SimulatedAnnealingSolver::resetSolution()
{
	currentVolume = nullptr;

	currentSolution.score = 0;
	currentSolution.matrix = glm::mat4(1.f);
	
	bestSolution = currentSolution;
}

bool SimulatedAnnealingSolver::nextSolution()
{
	const double EPS = 0.0001;
	if (temp > EPS)
	{

		modifyCurrentSolution();
		currentSolution.score = 0;
		temp *= cooling;

	
		std::cout << "[Solver] Temp: " << temp << ", iteration: " << iteration << std::endl;
		++iteration;

		return true;
	}

	return false;
}

void SimulatedAnnealingSolver::recordCurrentScore(double s)
{
	currentSolution.score = s;
	history.add(s);

	if (s < bestSolution.score)
	{
		bestSolution = currentSolution;
		std::cout << "[Debug] Accepted better solution with score " << s << std::endl;
	}
	else
	{
		double d = exp((currentSolution.score - bestSolution.score) / temp);
		if ((double)rng() / rng.max() < d)
		{
			bestSolution = currentSolution;
			std::cout << "[Debug] Accepted worse solution with score " << s << std::endl;
		}
	}
}

void SimulatedAnnealingSolver::modifyCurrentSolution()
{
	using namespace glm;

	if (!currentVolume)
		throw std::runtime_error("Simulated annealing solver has no current volume!");

	float f = 0.5f;
	if (rng() % 2 == 1)
		f *= -1.f;

	// randomly choose an operation to modify the current matrix with
	int mode = rng() % 4;
	if (mode == 0)
	{
		mat4 T = translate(vec3(f, 0, 0));
		currentSolution.matrix = T * currentSolution.matrix;
	}
	else if (mode == 1)
	{
		mat4 T = translate(vec3(0, f, 0));
		currentSolution.matrix = T * currentSolution.matrix;
	}
	else if (mode == 2)
	{
		mat4 T = translate(vec3(0, f, 0));
		currentSolution.matrix = T * currentSolution.matrix;
	}
	else if (mode == 3)
	{
		mat4 T = createRotationMatrix(radians(f), currentVolume);
		currentSolution.matrix = T * currentSolution.matrix;
	}


	currentSolution.id = iteration;
	currentSolution.score = 0;
}



void ParameterSpaceMapping::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;
	
	solutions.clear();

	for (int z = -128; z <= 128; z += 2)
	{

		for (int x = -128; x <= 128; x += 2)
		{

			float dx = (float)x; // 2.f;
			float dz = (float)z; // / 2.f;
			Solution s;
			s.score = 0;
			s.id = (z + 128) * 257 + x + 128;
			s.matrix = translate(vec3(dx, 0.f, dz));

			solutions.push_back(s);
		}

	}
	std::cout << "[Parameterspace Solver] Created " << solutions.size() << " candidate solutions.\n";
}
const IStackTransformationSolver::Solution& ParameterSpaceMapping::getBestSolution() 
{
	std::ofstream file("e:/temp/output.csv");
	assert(file.is_open());

	file << "# x, z, score\n";
	for (size_t i = 0; i < solutions.size(); ++i)
		file << solutions[i].matrix[3][0] << ", " << solutions[i].matrix[3][2] << ", " << solutions[i].score << std::endl;



	return UniformSamplingSolver::getBestSolution();
}

std::vector<glm::vec4> ParameterSpaceMapping::getSolutions() const
{
	std::vector<glm::vec4> result;

	double maxScore = 0; 
	double minScore = std::numeric_limits<double>::max();
	for (size_t i = 0; i < solutions.size(); ++i)
	{
		maxScore = std::max(maxScore, solutions[i].score);
		minScore = std::min(minScore, solutions[i].score);
	}
	
	for (size_t i = 0; i < solutions.size(); ++i)
	{
		glm::vec4 s = solutions[i].matrix[i];
		s.a = solutions[i].score;

		s.a -= minScore;
		s.a /= (maxScore - minScore);

		result.push_back(s);
	}

	return std::move(result);
}
