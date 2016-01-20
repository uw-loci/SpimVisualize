#pragma once

#include <glm/glm.hpp>
#include <boost/utility.hpp>

#include <random>
#include <vector>

class Framebuffer;
class InteractionVolume;

class IStackTransformationSolver : boost::noncopyable
{
public:
	struct Solution
	{
		// transformation matrix proposed
		glm::mat4			matrix;

		// score -- calculated externally?
		double				score;

		unsigned long		id;

		inline bool operator < (const Solution& rhs) const { return score < rhs.score; }
	};

	virtual ~IStackTransformationSolver() {};

	/// initializes a new run of the solver
	virtual void initialize(const InteractionVolume* v) = 0;
	/// resets all previous solutions
	virtual void resetSolution() = 0;
	/// returns true if another solution should be tested
	virtual bool nextSolution() = 0;

	/// records the image/occlusion score for the current solution
	virtual void recordCurrentScore(double score) = 0;
	/// recordsthe image/occlusion score for the current solution
	virtual void recordCurrentScore(Framebuffer* fbo);

	/// returns the current solution to be tested
	virtual const Solution& getCurrentSolution() const = 0;
	/// returns the best solution found so far
	virtual const Solution& getBestSolution() = 0;
};

/// tests a uniform range of different transformations in TX,TY,TZ, RY
class UniformSamplingSolver : public IStackTransformationSolver
{
public:

	virtual void initialize(const InteractionVolume* v);
	virtual void resetSolution();
	virtual bool nextSolution();

	virtual void recordCurrentScore(double score);

	virtual const Solution& getCurrentSolution() const;
	virtual const Solution& getBestSolution();

protected:
	std::vector<Solution>		solutions;
	int							currentSolution;

	void createCandidateSolutions(const InteractionVolume* v);
	void assertValidCurrentSolution();

	bool hasValidCurrentSolution() const;
 };

class SimulatedAnnealingSolver : public IStackTransformationSolver
{
public:

	virtual void initialize(const InteractionVolume* v);
	virtual void resetSolution();
	virtual bool nextSolution();

	virtual void recordCurrentScore(double score);

	inline const Solution& getCurrentSolution() const { return currentSolution; }
	inline const Solution& getBestSolution() { return bestSolution; }

	inline double getTemp() const { return temp; }

private:
	Solution			currentSolution, bestSolution;
	double				temp, cooling;

	std::mt19937		rng;
	unsigned int		iteration;

	void modifyCurrentSolution();

};