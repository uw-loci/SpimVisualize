#include "StackTransformationSolver.h"

#include <cassert>
#include <algorithm>
#include <random>
#include <iostream>
#include <chrono>

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>

#include "Framebuffer.h"
#include "InteractionVolume.h"

/// returns a uniform random variable in [-1..1]
static inline double rng_u(std::mt19937& rng)
{
	return ((double)rng() / rng.max() * 2) - 1;
}


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

	std::cout << "[Solver] Read back score: " << value << std::endl;
	glReadBuffer(GL_BACK);

	recordCurrentScore(value);
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
	if (currentSolution == solutions.size())
	{
		// reset to last valid solution
		--currentSolution;
		return false;
	}

	return true;
}

bool UniformSamplingSolver::hasValidCurrentSolution() const
{
	return !solutions.empty() && (currentSolution >= 0) && (currentSolution < solutions.size());
}

void UniformSamplingSolver::recordCurrentScore(double s)
{
	if (hasValidCurrentSolution())
	{
		std::cout << "[Solver] Recording score of " << s << " for current solution.\n";
		solutions[currentSolution].score += s;
	
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
	assert(!solutions.empty());

	// remove all solutions that do not have a valid id. we can do that through the current solution?
	size_t oldSize = solutions.size();
	if (currentSolution < solutions.size() - 1)
		solutions.resize(currentSolution);
	std::cout << "[Debug] Removed " << oldSize - solutions.size() << " unused solutions.\n";
	
	std::cout << "[Debug] Sorting " << solutions.size() << " solutions ... \n";
	std::sort(solutions.begin(), solutions.end());
	
	std::cout << "[Debug] Worst: " << solutions.front().score << ", best: " << solutions.back().score << std::endl;
	
	return solutions.back();
}

void UniformSamplingSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;

	std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());;
	
	/*

	// single axis alignment: dx,dy,dz, ry
	int transform = rng() % 4;

	std::cout << "[Uniform solver] Creating transformations for: " << std::endl;

	if (transform == 3)
	{
	
		// rotation
		for (int a = -100; a <= 100; ++a)
		{
			Solution s;
			s.score = 0;
			s.id = a;
			s.matrix = glm::rotate(radians((float)a / 10.f), vec3(0, 1, 0));

			solutions.push_back(s);
		}
	}
	else
	{
		for (int x = -100; x <= 100; ++x)
		{
			float dx = (float)x / 5.f;
			vec3 v(0.f);
			v[transform] = dx;

			Solution s;
			s.score = 0;
			s.id = x;
			s.matrix = translate(v);
			
			solutions.push_back(s);
		}
	}
	*/

	// try different rotations
	for (int r = -100; r <= 100; ++r)
	{
		float a = radians((float)r / 10.f);

		Solution s;
		s.id = r;
		s.score = 0;

		s.matrix = translate(mat4(1.f), v->getBBox().getCentroid());
		s.matrix = rotate(s.matrix, a, glm::vec3(0, 1, 0));
		s.matrix = translate(s.matrix, v->getBBox().getCentroid() * -1.f);

		//s.matrix = rotate(a, vec3(0, 1, 0));
		solutions.push_back(s);

	}


	std::cout << "[Uniform solver] Created " << solutions.size() << " new candidate transforms.\n";
	currentSolution = 0;
}


void DXSolver::createCandidateSolutions(const InteractionVolume* v)
{
	using namespace glm;
	std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dx = (float)x / 5.f;
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
	std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dy = (float)x / 5.f;
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
	std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());;

	solutions.clear();

	for (int x = -20; x <= 20; ++x)
	{
		float dz = (float)x / 5.f;
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

		s.matrix = translate(mat4(1.f), v->getBBox().getCentroid());
		s.matrix = rotate(s.matrix, a, glm::vec3(0, 1, 0));
		s.matrix = translate(s.matrix, v->getBBox().getCentroid() * -1.f);

		//s.matrix = rotate(a, vec3(0, 1, 0));
		solutions.push_back(s);

	}


	std::cout << "[RY Solver] Created " << solutions.size() << " candidate solutions.\n";
}


void MultiDimensionalHillClimb::initialize(const InteractionVolume* v)
{
	// initialize rng
	rng = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());;

	resetSolution();
	history.reset();

	currentVolume = v;
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
		bestSolution = potentialSolutions.back();
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

			// this rotates the volume around its center
			s.matrix = translate(mat4(1.f), currentVolume->getBBox().getCentroid());
			s.matrix = rotate(s.matrix, f, glm::vec3(0, 1, 0));
			s.matrix = translate(s.matrix, currentVolume->getBBox().getCentroid() * -1.f);

			s.score = 0;
			potentialSolutions.push_back(s);
		}
	}

	std::cout << "[Hillclimb] " << potentialSolutions.size() << " new solutions in queue.\n";
}

void SimulatedAnnealingSolver::initialize(const InteractionVolume* v)
{
	temp = 1.f;
	
	currentSolution.score = 0;
	currentSolution.matrix = v->transform;
	bestSolution = currentSolution;

	iteration = 0;

	// initialize rng
	rng = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());;

	history.reset();
}

void SimulatedAnnealingSolver::resetSolution()
{

	currentSolution.score = 0;
	currentSolution.matrix = glm::mat4(1.f);
	
	bestSolution = currentSolution;
}

bool SimulatedAnnealingSolver::nextSolution()
{
	const float EPS = 0.0001f;
	if (temp > EPS)
	{
		++iteration;

		modifyCurrentSolution();
		currentSolution.score = 0;
		temp *= cooling;


		std::cout << "[Solver] Temp: " << temp << std::endl;
		
		return true;
	}

	return false;
}

void SimulatedAnnealingSolver::recordCurrentScore(double s)
{
	currentSolution.score = s;
	history.add(s);

	if (s > bestSolution.score)
		bestSolution = currentSolution;
	else
	{
		double d = exp((currentSolution.score - bestSolution.score) / temp);
		if ((double)rng() / rng.max() < d)
			bestSolution = currentSolution;
	}
}

void SimulatedAnnealingSolver::modifyCurrentSolution()
{
	using namespace glm;
	
	// randomly choose an operation to modify the current matrix with

	mat4 T(1.f);

	currentSolution.matrix = T * currentSolution.matrix;

	currentSolution.id = iteration;
}