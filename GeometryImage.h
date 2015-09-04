#pragma once

#include <vector>
#include <string>
#include <map>

#include <glm/glm.hpp>

typedef std::vector<glm::vec3> Pointcloud;


class GeometryImage
{
public:
	GeometryImage();
	GeometryImage(unsigned int width, unsigned int height);
	virtual ~GeometryImage();

	virtual void calculate(const Pointcloud& frame, bool POT=false);
	virtual void calculate(const Pointcloud& frame, const glm::vec3& bias, const glm::vec3& scale, bool POT = false);
	virtual void calculateDiff(const GeometryImage& a, const GeometryImage& b);
	
	virtual std::vector<glm::vec3> reconstructPoints() const;
	std::vector<glm::vec3> getPixelValues() const;
	std::vector<glm::vec3> getPixelIndex() const;

	virtual void save(const std::string& filename) const;
	virtual void load(const std::string& filename);

	void loadPPM(const std::string& filename);
	void savePPM(const std::string& filename) const;

	virtual void loadScale(const std::string& filename);
	virtual void saveScale(const std::string& filename) const;


	inline unsigned int getPointcount() const { return pointCount; }
	inline glm::ivec2 getResolution() const { return glm::ivec2(width, height); }

	void updateTexture();
	inline unsigned int getTexture() const { return texture;  }

	inline const glm::vec3& getScale() const { return scale; }
	inline const glm::vec3& getBias() const { return bias;  }
protected:
	struct Pixel
	{
		unsigned char	r, g, b, a;
	};

	unsigned int		width, height;
	std::vector<Pixel>	data;
	unsigned int		pointCount;


	void calculateDimensions(bool pot);


private:
	unsigned int		texture;

	// stores the bbox transform to scale it to [0..255]
	glm::vec3			bias, scale;


	GeometryImage(const GeometryImage&);

};

class MultiScaleGeometryImage : public GeometryImage
{
public:
	
	void addPoints(const std::vector<glm::vec3>& points);
	void addPoints(const std::vector<glm::vec3>& points, const glm::vec3& scale, const glm::vec3& bias);

	std::vector<glm::vec3> reconstructPoints() const;

	virtual void loadScale(const std::string& filename);
	virtual void saveScale(const std::string& filename) const;
	
	inline unsigned int getDataSize() const { return data.size();  }

private:
	struct ScaleBand
	{
		unsigned int	pointcount;
		glm::vec3		bias, scale;
	};

	std::vector<ScaleBand>	scaleInfo;


};