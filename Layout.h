#pragma once

#include <glm/glm.hpp>

#include <boost/utility.hpp>

class ICamera;
struct AABB;

struct Viewport : boost::noncopyable
{
	// window coordinates
	glm::ivec2		position, size;

	glm::vec3		color;

	enum ViewportName { ORTHO_X = 0, ORTHO_Y, ORTHO_Z, PERSPECTIVE, CONTRAST_EDITOR, PERSPECTIVE_ALIGNMENT } name;

	ICamera*		camera;
	bool			highlighted;


	inline glm::vec2 getRelativeCoords(const glm::ivec2& coords) const
	{
		using namespace glm;
		const ivec2 rc = coords - position;
		return vec2(rc) / vec2(size);
	}


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


class ILayout : boost::noncopyable
{
public:
	virtual ~ILayout() {}

	virtual Viewport* getActiveViewport() = 0;

	virtual void updateMouseMove(const glm::ivec2& coords) = 0;
	virtual void resize(const glm::ivec2& size) = 0;

	virtual ICamera* getPerspectiveCamera() = 0;

	virtual size_t getViewCount() const = 0;
	virtual Viewport* getView(unsigned int n) = 0;

	virtual void panActiveViewport(const glm::vec2& delta) = 0;

	/*
	virtual void saveState(const std::string& filename) const = 0;
	virtual void loadState(const std::string& filename) = 0;
	*/
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


	virtual void panActiveViewport(const glm::vec2& delta);

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


	virtual void panActiveViewport(const glm::vec2& delta);


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

class ContrastEditLayout : public ILayout
{
public:
	ContrastEditLayout(const glm::ivec2& resolution);
	virtual ~ContrastEditLayout();

	virtual Viewport* getActiveViewport();
	virtual void updateMouseMove(const glm::ivec2& coords);

	virtual void resize(const glm::ivec2& size);

	virtual ICamera* getPerspectiveCamera() { return views[0].camera; }
	inline size_t getViewCount() const { return 2; };
	inline Viewport* getView(unsigned int n) { return &views[n]; }
	

	virtual void panActiveViewport(const glm::vec2& delta);

protected:
	Viewport		views[2];
};


class AlignVolumesLayout : public ILayout
{
public:
	AlignVolumesLayout(const glm::ivec2& resolution);
	virtual ~AlignVolumesLayout();


	virtual Viewport* getActiveViewport();
	virtual void updateMouseMove(const glm::ivec2& coords);

	virtual void resize(const glm::ivec2& size);

	virtual ICamera* getPerspectiveCamera() { return views[0].camera; }
	inline size_t getViewCount() const { return 2; };
	inline Viewport* getView(unsigned int n) { return &views[n]; }


	virtual void panActiveViewport(const glm::vec2& delta);



protected:
	Viewport		views[2];
};
