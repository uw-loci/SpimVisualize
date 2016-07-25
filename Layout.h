#pragma once

#include <glm/glm.hpp>

#include <boost/utility.hpp>

class ICamera;
struct AABB;

class Viewport : boost::noncopyable
{
public:
	// window coordinates
	glm::ivec2		position, size;

	glm::vec3		color;
	
	std::string		name;

	ICamera*		camera;
	bool			highlighted;


	inline glm::vec2 getRelativeCoords(const glm::ivec2& coords) const
	{
		using namespace glm;
		ivec2 rc = coords - position;
		rc.y = size.y - rc.y;
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

	void maximizeView(const AABB& bbox);


	void setup() const;
	void drawBorder() const;
	void drawTitle() const;
};


class ILayout : boost::noncopyable
{
public:
	virtual ~ILayout() {}

	virtual Viewport* getActiveViewport() = 0;

	virtual void updateMouseMove(const glm::ivec2& coords) = 0;
	virtual void resize(const glm::ivec2& size) = 0;
	
	virtual size_t getViewCount() const = 0;
	virtual Viewport* getView(unsigned int n) = 0;

	virtual void panActiveViewport(const glm::vec2& delta) = 0;

	virtual void maximizeView(const AABB& bbox) = 0;

	virtual bool isSingleView() const = 0;

	virtual void centerCamera(const glm::vec3& tgt) = 0;

	/*
	virtual void saveState(const std::string& filename) const = 0;
	virtual void loadState(const std::string& filename) = 0;
	*/
};

class SwitchableLayoutContainer : public ILayout
{
public:
	SwitchableLayoutContainer(ILayout* a, ILayout* b);
	virtual ~SwitchableLayoutContainer();


	inline void switchLayouts() { currentLayout = 1 - currentLayout; }
	inline ILayout* getCurrentLayout() { return layouts[currentLayout]; }
	
	virtual Viewport* getActiveViewport() { return layouts[currentLayout]->getActiveViewport(); }

	virtual void updateMouseMove(const glm::ivec2& coords) { layouts[currentLayout]->updateMouseMove(coords); }
	virtual void resize(const glm::ivec2& size) { layouts[currentLayout]->resize(size); }

	virtual size_t getViewCount() const { return layouts[currentLayout]->getViewCount(); }
	virtual Viewport* getView(unsigned int n) { return layouts[currentLayout]->getView(n); }

	virtual void panActiveViewport(const glm::vec2& delta) { layouts[currentLayout]->panActiveViewport(delta); }

	virtual void maximizeView(const AABB& bbox) { layouts[currentLayout]->maximizeView(bbox); }

	virtual bool isSingleView() const { return layouts[currentLayout]->isSingleView(); }

	virtual void centerCamera(const glm::vec3& tgt) { layouts[0]->centerCamera(tgt); layouts[1]->centerCamera(tgt); }

private:
	ILayout*		layouts[2];
	int				currentLayout;

};


class FourViewLayout : public ILayout
{
public:
	FourViewLayout(const glm::ivec2& size);

	virtual void resize(const glm::ivec2& size);
	virtual void updateMouseMove(const glm::ivec2& m);
	
	virtual Viewport* getActiveViewport();

	inline size_t getViewCount() const { return 4; }
	inline Viewport* getView(unsigned int n) { return &views[n]; }

	inline bool isSingleView() const { return false; }

	virtual void panActiveViewport(const glm::vec2& delta);
	virtual void maximizeView(const AABB& bbox);

	virtual void centerCamera(const glm::vec3& tgt);

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

	inline void maximizeView(const AABB& bbox) { viewport.maximizeView(bbox); }

	virtual void panActiveViewport(const glm::vec2& delta);

	inline bool isSingleView() const { return true; }

	virtual void centerCamera(const glm::vec3& tgt);

protected:
	Viewport			viewport;
};


class TopViewFullLayout : public SingleViewLayout
{
public:
	TopViewFullLayout(const glm::ivec2& resolution);
};

class OrthoViewFullLayout : public SingleViewLayout
{
public:
	OrthoViewFullLayout(const glm::ivec2& resolution, const std::string& name);
};

class PerspectiveFullLayout : public SingleViewLayout
{
public:
	PerspectiveFullLayout(const glm::ivec2& resolution);
};

class ContrastEditLayout : public ILayout
{
public:
	ContrastEditLayout(const glm::ivec2& resolution);
	virtual ~ContrastEditLayout();

	virtual Viewport* getActiveViewport();
	virtual void updateMouseMove(const glm::ivec2& coords);

	virtual void resize(const glm::ivec2& size);

	inline size_t getViewCount() const { return 2; };
	inline Viewport* getView(unsigned int n) { return &views[n]; }
	
	virtual void maximizeView(const AABB& bbox);
	virtual void panActiveViewport(const glm::vec2& delta);

	inline bool isSingleView() const { return false; }

protected:
	Viewport		views[2];
};
