#include "StackTransformationSolver.h"

#include <cassert>
#include <algorithm>
#include <random>
#include <iostream>

#include <glm/gtx/transform2.hpp>

IStackTransformationSolver::IStackTransformationSolver() : currentSolution(-1)
{
}

bool IStackTransformationSolver::nextSolution()
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

void IStackTransformationSolver::resetSolutions()
{
	solutions.clear();
	currentSolution = -1;
}

bool IStackTransformationSolver::hasValidCurrentSolution() const
{
	return !solutions.empty() && (currentSolution >= 0) && (currentSolution < solutions.size());
}

void IStackTransformationSolver::recordCurrentScore(double s)
{
	if (hasValidCurrentSolution())
		solutions[currentSolution].score += s;
}

const IStackTransformationSolver::Solution& IStackTransformationSolver::getCurrentSolution() const
{
	if (!hasValidCurrentSolution())
		throw std::runtime_error("Solver has no valid current solution");
	
	return solutions[currentSolution];
}

const IStackTransformationSolver::Solution& IStackTransformationSolver::getBestSolution()
{
	assert(!solutions.empty());

	std::sort(solutions.begin(), solutions.end());
	return solutions.back();
}

void UniformSamplingSolver::createCandidateSolutions()
{
	using namespace glm;

	resetSolutions();

	std::mt19937 rng;
	
	// single axis alignment: dx,dy,dz, ry
	int transform = rng() % 4;

	
	// right now just check dx+dz
	if (transform == 1)
		transform = 0;
	if (transform == 3)
		transform = 2;

	std::cout << "[Uniform solver] Creating transformations for: " << std::endl;

	if (transform == 3)
	{
		/*
		//vec3 d = stacks[currentStack]->getBBox().getCentroid();
		vec3 d = interactionVolumes[currentVolume]->getBBox().getCentroid();
		mat4 T = glm::translate(vec3(d.x, 0.f, d.z));
		*/

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
	
	std::cout << "[Uniform solver] Created " << solutions.size() << " new candidate transforms.\n";
	currentSolution = 0;
}