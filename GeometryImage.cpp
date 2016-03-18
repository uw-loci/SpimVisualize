#include "GeometryImage.h"

#include "stb_image.h"
#include "AABB.h"

#include <iostream>
#include <cassert>
#include <fstream>
#include <stdexcept>
#include <cstring>

//#include <glm/gtx/io.hpp>
#include "glmIO.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>


#ifndef NO_GRAPHICS
#include <GL/glew.h>
#endif

using namespace glm;
using namespace std;


static const glm::vec3& getRandomColor(unsigned int n)
{
	static std::vector<glm::vec3> pool;
	if (pool.empty() || n >= pool.size())
	{
		for (unsigned int i = 0; i < (n + 1) * 2; ++i)
		{
			float r = (float)rand() / RAND_MAX;
			float g = (float)rand() / RAND_MAX;
			float b = 1.f - (r + g);
			pool.push_back(glm::vec3(r, g, b));
		}
	}

	return pool[n];
}



static inline unsigned int getNextPOTSize(unsigned int x)
{
	unsigned int y = 2;
	while (x > y)
		y *= 2;

	return y;
}


GeometryImage::GeometryImage() : width(0), height(0), pointCount(0), texture(0), bias(0.f), scale(1.f)
{
}


GeometryImage::GeometryImage(unsigned int w, unsigned int h) : width(w), height(h), pointCount(0), texture(0), bias(0.f), scale(1.f)
{
}

GeometryImage::~GeometryImage()
{
#ifndef NO_GRAPHICS
	glDeleteTextures(1, &texture);

#endif
}

void GeometryImage::calculate(const Pointcloud& p, bool pot)
{
	// calculate the transform to normalize the point cloud
	AABB bbox;
	bbox.calculate(p);
	this->calculate(p, bbox.min, glm::vec3(1.f) / bbox.getSpan(), pot);
}

void GeometryImage::calculate(const Pointcloud& points, const glm::vec3& bias, const glm::vec3& scale, bool pot)
{
	this->bias = bias;
	this->scale = scale;
	this->pointCount = (unsigned int)points.size();

	calculateDimensions(pot);


	data.clear();
	data.reserve(width*height);

	for (size_t i = 0; i < points.size(); ++i)
	{
		const glm::vec3& pt = points[i];

		// this brings the point to [0 ... 1]
		glm::vec3 p = (pt - bias) * scale;

		Pixel px;
		px.r = (unsigned char)(std::roundf(p.r * 255.f));
		px.g = (unsigned char)(std::roundf(p.g * 255.f));
		px.b = (unsigned char)(std::roundf(p.b * 255.f));
		px.a = 255;

		data.push_back(px);
	}

	// pad the data with black
	for (size_t i = points.size(); i < width*height; ++i)
	{
		Pixel px;
		px.r = px.g = px.b = 0;
		px.a = 255;
		data.push_back(px);
	}


	updateTexture();
}

void GeometryImage::load(const std::string& filename)
{

	std::cout << "[GeoImg] Loading geometry image \"" << filename << "\" ...\n";
	
	if (filename.substr(filename.find_last_of(".")) == ".ppm")
		return loadPPM(filename);

	int w, h, c;
	stbi_uc* img = stbi_load(filename.c_str(), &w, &h, &c, 4);

	width = w;
	height = h;
	data.resize(w*h);
	pointCount = (unsigned int)data.size();

	memcpy(reinterpret_cast<void*>(&data[0]), img, sizeof(Pixel)*w*h);

	stbi_image_free(img);
	updateTexture();
}

void GeometryImage::loadPPM(const std::string& filename)
{
	std::cout << "[GeoImg] Loading geometrymap \"" << filename << "\" ... \n";


	std::ifstream file(filename.c_str(), std::ios::binary);
	assert(file.is_open());


	std::string header[3];
	int hCounter = 0;
	while (hCounter < 3)
	{
		//safeGetline( file, str );

		char tmp[128];
		memset(tmp, 0, 128);
		for (int i = 0; i < 128; ++i)
		{
			file.read(&tmp[i], 1);
			if (tmp[i] == '\n')
				break;
		}

		if (hCounter == 0)
			assert(tmp[0] == 'P' && tmp[1] == '6');

		if (tmp[0] != '#')
			header[hCounter++] = std::string(tmp);
	}

	width = 0, height = 0;
	//sscanf(header[1].c_str(), "%u %u", &width, &height);

#ifdef _WIN32
	sscanf_s(header[1].c_str(), "%u %u", &width, &height);
#else
	sscanf(header[1].c_str(), "%u %u", &width, &height);
#endif
	unsigned int size = width*height * 3;

	std::cout << "[GeoImg] Resolution: " << width << "x" << height << ", size: " << size << std::endl;




	/*
	// ffmpeg off-by-one header
	char c = file.peek();
	if (c == '\n')
	{
		file.read(&c, 1);
		std::cout << "[DEBUG] ffmpeg off-by-1\n";
	}
	*/

	std::vector<unsigned char> temp(size, (unsigned char)0);
	file.read(reinterpret_cast<char*>(&temp[0]), temp.size());
	//std::cout << "[DEBUG] Read " << file.gcount() << " bytes.\n";

	// expand the data from rgb to rgba
	data.clear();
	data.resize(width*height);

	for (size_t i = 0; i < width*height; ++i)
	{
		Pixel& p = data[i];
		const unsigned char* d = &temp[i * 3];

		p.r = d[0];
		p.g = d[1];
		p.b = d[2];
		p.a = 255;
	}

	updateTexture();
}

void GeometryImage::save(const std::string& filename) const
{
	if (filename.substr(filename.find_last_of(".")) == ".ppm")
	{
		savePPM(filename);
	}
	else
	{
		stbi_write_png(filename.c_str(), width, height, 4, reinterpret_cast<const void*>(&data[0]), 0);
	}

	/*
	if (saveScale)
	{
		// write scales here
		std::ofstream scalefile(filename + "_scale.txt");
		scalefile << "# numpoints, bias[3], scale[3]\n";
		scalefile << pointCount << ", " << bias.x << "," << bias.y << "," << bias.z << "," << scale.x << "," << scale.y << "," << scale.z << std::endl;
	}
	*/
}

void GeometryImage::savePPM(const std::string& filename) const
{

	// discard alpha bit
	std::vector<unsigned char> temp; 
	temp.reserve(width*height * 3);
	for (size_t i = 0; i < data.size(); ++i)
	{
		const Pixel& p = data[i];
		temp.push_back(p.r);
		temp.push_back(p.g);
		temp.push_back(p.b);
	}

	// pad the data
	while (temp.size() < width*height*3)
		temp.push_back(0);


	
	std::ofstream file(filename.c_str(), std::ios::trunc | std::ios::binary);
	assert(file.is_open());
	char header[256];
#ifdef _WIN32
	sprintf_s(header, "P6\n%u %u\n255\n", width, height);
#else
	sprintf(header, "P6\n%u %u\n255\n", width, height);
#endif	
	file.write(header, strlen(header));
	file.write(reinterpret_cast<const char*>(&temp[0]), temp.size());
	file.close();
}


void GeometryImage::calculateDiff(const GeometryImage& a, const GeometryImage& b)
{
	if (a.width != b.width || a.height != b.height)
		throw std::runtime_error("Dimensions of images do not match!");

	width = a.width;
	height = a.height;

	bias = (a.bias - b.bias);
	scale = (a.scale - b.scale);

	data.resize(width*height);

	for (size_t i = 0; i < data.size(); ++i)
	{
		const Pixel& pa = a.data[i];
		const Pixel& pb = b.data[i];

		Pixel& p = data[i];
		p.r = pa.r - pb.r;
		p.g = pa.g - pb.g;
		p.b = pa.b - pb.b;
		p.a = std::min(pa.a, pb.a);
	}

	pointCount = a.pointCount;
}


std::vector<glm::vec3> GeometryImage::reconstructPoints() const
{
	Pointcloud points;
	points.reserve(width*height);

	const vec3 iscale = vec3(1.f) / scale;

	for (size_t i = 0; i < std::min((unsigned int)data.size(), pointCount); ++i)
	{
		const GeometryImage::Pixel& p = data[i];

		// reached the padding
		if (p.a < 255)
			break;

		vec3 v(p.r, p.g, p.b);
		v *= (1.f / 255.f);
		v *= iscale;
		v += bias;

		points.push_back(v);
	}

	return points;
}

std::vector<glm::vec3> GeometryImage::getPixelValues() const
{
	Pointcloud colors;
	colors.reserve(width*height);
	for (size_t i = 0; i < std::min((unsigned int)data.size(), pointCount); ++i)
	{
		const GeometryImage::Pixel& p = data[i];
		// reached the padding
		if (p.a < 255)
			break;

		vec3 v(p.r, p.g, p.b);
		colors.push_back(v / 255.f);
	}

	return colors;
}

std::vector<glm::vec3> GeometryImage::getPixelIndex() const
{
	Pointcloud indices;
	indices.reserve(width*height);

	for (size_t i = 0; i < std::min((unsigned int)data.size(), pointCount); ++i)
	{
		int k = (int)(((float)i*8.f) / data.size());
		indices.push_back(getRandomColor(k));
	}

	return indices;
}

void GeometryImage::loadScale(const std::string& filename)
{
	ifstream file(filename.c_str());
	assert(file.is_open());

	while (!file.eof())
	{
		string line;
		getline(file, line);
		vector<string> tokens;
		boost::split(tokens, line, boost::is_any_of(" "));

		if (!tokens.empty())
		{
			if (tokens[0] == "Points")
			{
				pointCount = atoi(tokens[1].c_str());
			}
			else if (tokens[0] == "Bias")
			{
				bias.x = (float)atof(tokens[1].c_str());
				bias.y = (float)atof(tokens[2].c_str());
				bias.z = (float)atof(tokens[3].c_str());
			}
			else if (tokens[0] == "Scale")
			{
				scale.x = (float)atof(tokens[1].c_str());
				scale.y = (float)atof(tokens[2].c_str());
				scale.z = (float)atof(tokens[3].c_str());
			}
		}
		
	}

	cout << "[GeoImg] Loaded scale file \"" << filename << "\"; " << pointCount << " points.\n";
	cout << "[GeoImg] Bias: " << bias << ", scale: " << scale << endl;
}

void GeometryImage::saveScale(const std::string& filename) const
{
	ofstream file(filename.c_str());
	file << "Points " << pointCount << endl;
	file << "Bias " << bias.x << " " << bias.y << " " << bias.z << endl;
	file << "Scale " << scale.x << " " << scale.y << " " << scale.z << endl;
}

void GeometryImage::updateTexture()
{
#ifndef NO_GRAPHICS
	std::cout << "[GeoImg] Creating texture ";
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
	std::cout << texture << endl;
#else
	texture = 0;
#endif
}

void GeometryImage::calculateDimensions(bool pot)
{
	// calculate width/height
	this->width = (unsigned int)std::floor(sqrtf((float)pointCount));

	// make width divisible by two for yuv420 etc
	if (width % 2)
		width += 1;

	// ffmpeg seems to have problems with some movie sizes ... 
	//std::cout << "[Debug] Geoimg width: " << width << " -> ";
	width = getNextPOTSize(width);
	//std::cout << width << std::endl;

	this->height = (unsigned int)std::ceil((float)pointCount / this->width);

	// make height divisible by two for yuv420 ... 
	if (height % 2)
		height += 1;


	// increase width/height to POT look
	if (pot)
	{
		std::cout << "[Debug] Calculating POT image size ... " << width << "x" << height << " -> ";
		width = getNextPOTSize(width);
		height = getNextPOTSize(height);
		std::cout << width << "x" << height << endl;
	}

	//cout << "[Debug] Geometry image " << f.getPointCount() << " -> " << this->width << "x" << this->height << std::endl;

}

void MultiScaleGeometryImage::addPoints(const std::vector<glm::vec3>& points)
{
	AABB bbox;
	bbox.calculate(points);
	this->addPoints(points, bbox.min, glm::vec3(1.f) / bbox.getSpan());
}


void MultiScaleGeometryImage::addPoints(const std::vector<glm::vec3>& points, const glm::vec3& bias, const glm::vec3& scale)
{
	ScaleBand band;
	band.bias = bias;
	band.scale = scale;
	band.pointcount = (unsigned int)points.size();
	scaleInfo.push_back(band);

	for (size_t i = 0; i < points.size(); ++i)
	{
		const glm::vec3& pt = points[i];

		// this brings the point to [0 ... 1]
		glm::vec3 p = (pt - bias) * scale;

		Pixel px;
		px.r = (unsigned char)(std::roundf(p.r * 255.f));
		px.g = (unsigned char)(std::roundf(p.g * 255.f));
		px.b = (unsigned char)(std::roundf(p.b * 255.f));
		px.a = 255;

		data.push_back(px);
	}
	
	pointCount += (unsigned int)points.size();

	this->calculateDimensions(false);

}

std::vector<glm::vec3> MultiScaleGeometryImage::reconstructPoints() const
{

	vector<vec3> points;
	points.reserve(pointCount);

	unsigned int pixel = 0;
	for (size_t i = 0; i < scaleInfo.size(); ++i)
	{
		const ScaleBand& band = scaleInfo[i];

		for (unsigned int k = 0; k < band.pointcount; ++k, ++pixel)
		{
			const Pixel& p = data[pixel];
			if (p.a < 255)
				continue;

			vec3 v(p.r, p.g, p.b);
			v *= (1.f / 255.f);
			v /= band.scale;
			v += band.bias;

			points.push_back(v);
		}
	}
	
	return points;
}

void MultiScaleGeometryImage::saveScale(const string& filename) const
{
	ofstream file(filename.c_str());
	assert(file.is_open());

	file << "Bands " << scaleInfo.size() << endl;
	for (size_t i = 0; i < scaleInfo.size(); ++i)
	{
		const ScaleBand& band = scaleInfo[i];

		file << "Points " << band.pointcount << endl;
		file << "Bias " << band.bias.x << " " << band.bias.y << " " << band.bias.z << endl;
		file << "Scale " << band.scale.x << " " << band.scale.y << " " << band.scale.z << endl;
	}
}
void MultiScaleGeometryImage::loadScale(const string& filename)
{
	ifstream file(filename.c_str());
	assert(file.is_open());

	unsigned int bandcount = 0;
	ScaleBand band;

	pointCount = 0;
	
	while (!file.eof())
	{
		string line;
		getline(file, line);
		vector<string> tokens;
		boost::split(tokens, line, boost::is_any_of(" "));

		if (!tokens.empty())
		{
			if (tokens[0] == "Bands")
				bandcount = atoi(tokens[1].c_str());

			if (tokens[0] == "Points")
			{
				band.pointcount = atoi(tokens[1].c_str());
				pointCount += (unsigned int)band.pointcount;
			}
			else if (tokens[0] == "Bias")
			{
				band.bias.x = (float)atof(tokens[1].c_str());
				band.bias.y = (float)atof(tokens[2].c_str());
				band.bias.z = (float)atof(tokens[3].c_str());
			}
			else if (tokens[0] == "Scale")
			{
				band.scale.x = (float)atof(tokens[1].c_str());
				band.scale.y = (float)atof(tokens[2].c_str());
				band.scale.z = (float)atof(tokens[3].c_str());

				// scale should always be the last entry
				scaleInfo.push_back(band);

			}
		}

	}

	assert(scaleInfo.size() == bandcount);

	std::cout << "[GeoImg] Loaded scale file \"" << filename << "\"; " << pointCount << " points.\n";
	std::cout << "[GeoImg] Bands: " << scaleInfo.size() << endl;
}

