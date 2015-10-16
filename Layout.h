#pragma once

#include <glm/glm.hpp>

class ICamera;
struct AABB;

struct Viewport
{
	// window coordinates
	glm::ivec2		position, size;

	glm::vec3		color;

	enum ViewportName { ORTHO_X = 0, ORTHO_Y, ORTHO_Z, PERSPECTIVE = 3 } name;

	ICamera*		camera;
	bool			highlighted;

	inline bool isInside(const glm::ivec2& cursor) const
	{
		using namespace glm;
		const ivec2 rc = cursor - position;
		return all(greaterThanEqual(rc, ivec2(0))) && all(lessThanEqual(rc, size));
	}

	inline float getAspect() const
	{
		return (float)size.x / size.y;
	}


	void setup() const;
};


class ILayout
{
public:
	virtual ~ILayout() {}

	virtual Viewport* getActiveViewport() = 0;

	virtual void updateMouseMove(const glm::ivec2& coords) = 0;
	virtual void resize(const glm::ivec2& size) = 0;

	virtual ICamera* getPerspectiveCamera() = 0;

	virtual size_t getViewCount() const = 0;
	virtual Viewport* getView(unsigned int n) = 0;
};


class FourViewLayout : public ILayout
{
public:
	FourViewLayout(const glm::ivec2& size);

	virtual void resize(const glm::ivec2& size);
	virtual void updateMouseMove(const glm::ivec2& m);
	
	virtual Viewport* getActiveViewport();

	virtual ICamera* getPerspectiveCamera() { return views[3].camera; }

	inline size_t getViewCount() const { return 4; }
	inline Viewport* getView(unsigned int n) { return &views[n]; }


private:
	Viewport		views[4];
};

class SingleViewLayout : public ILayout
{
public:
	SingleViewLayout(const glm::ivec2& resolution);
	virtual ~SingleViewLayout();

	virtual Viewport* getActiveViewport();
	virtual void updateMouseMove(const glm::ivec2& coords);

	virtual void resize(const glm::ivec2& size);

	inline size_t getViewCount() const { return 1; }
	inline Viewport* getView(unsigned int n) { return &viewport; }

protected:
	Viewport			viewport;
};


class TopViewFullLayout : public SingleViewLayout
{
public:
	TopViewFullLayout(const glm::ivec2& resolution);

	inline ICamera* getPerspectiveCamera() { return nullptr; }

};

class PerspectiveFullLayout : public SingleViewLayout
{
public:
	PerspectiveFullLayout(const glm::ivec2& resolution);

	inline ICamera* getPerspectiveCamera() { return viewport.camera; }
};


