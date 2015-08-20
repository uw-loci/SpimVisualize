
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <vector>

using namespace std;
using namespace cv;


void scaleValues(int, void*);

int scaleValue = 0;
int biasValue = 500;

char* trackbar_scale = "Scale value:";
char* trackbar_bias = "Bias value:";

Mat input, output;

const char* windowName = "Hello OpenCV";

int main(int argc, const char** argv)
{
	cout << "Hello OpenCV!\n";


	// 1 -- load image
	input = imread("e:/spim/OpenSPIM_tutorial/spim_TL05_Angle4.ome.tiff", CV_LOAD_IMAGE_ANYDEPTH);
	//input = imread("e:/taliesin_web.png");

	//cvtColor(input, input, CV_RGB2GRAY);


	/*
	Mat inputGray;
	
	GaussianBlur(input_gray, input_gray, Size(9, 9), 2, 2);

	// 2 -- feature detection
	vector<Vec3f> circles;
	HoughCircles(input_gray, circles, CV_HOUGH_GRADIENT, 1, input.rows / 8, 200, 100, 0, 0);
	for (size_t i = 0; i < circles.size(); ++i)
	{
		Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
		int radius = cvRound(circles[i][2]);
		// circle center
		circle(input, center, 3, Scalar(0, 255, 0), -1, 8, 0);
		// circle outline
		circle(input, center, radius, Scalar(0, 0, 255), 3, 8, 0);
	}
	std::cout << "Found " << circles.size() << " circles.\n";


	vector<Vec4i> lines;
	HoughLinesP(input_gray, lines, 1, CV_PI / 180, 50, 50, 10);
	for (size_t i = 0; i < lines.size(); i++)
	{
		Vec4i l = lines[i];
		line(input, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 1, CV_AA);
	}
	std::cout << "Found " << lines.size() << " lines.\n";
	*/

	
	namedWindow(windowName, WINDOW_AUTOSIZE);
	
	createTrackbar(trackbar_scale, windowName, &scaleValue, 1000, scaleValues);
	createTrackbar(trackbar_bias, windowName, &biasValue, 1000, scaleValues);

	scaleValues(0, 0);

	while (true)
	{
		int c = waitKey(20);
		if ((char)c == 27)
			exit(0);
	}



	return 0;
}


void scaleValues (int, void*)
{
	output = input * (float)scaleValue / 10.f;

	/*
	std::cout << "Input: " << input.rows << "x" << input.cols << std::endl;
	std::cout << "Output: " << output.rows << "x" << output.cols << std::endl;
	*/

	output = output + (biasValue - 500) * 10;

	imshow(windowName, output);
}
