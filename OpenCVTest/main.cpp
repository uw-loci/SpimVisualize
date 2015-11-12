
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <vector>

using namespace std;
using namespace cv;


void circleValues(int, void*);
void sliceValue(int, void*);

int param1 = 100;
int param2 = 200;

int slice = 0;

char* param1_name = "Upper Canny threshold";
char* param2_name = "Center threshold";
char* slice_name = "Slice";

Mat input, output;

const char* windowName = "Hello OpenCV";

int main(int argc, const char** argv)
{
	cout << "Hello OpenCV!\n";


	// 1 -- load image
	input = imread("e:/spim/test_beads/spim_TL01_Angle0.ome.tiff", CV_LOAD_IMAGE_ANYDEPTH);
	input.convertTo(input, CV_8U);
	
	
	namedWindow(windowName, WINDOW_AUTOSIZE);
	
	createTrackbar(param1_name, windowName, &param1, 1000, circleValues);
	createTrackbar(param2_name, windowName, &param2, 1000, circleValues);
	createTrackbar(slice_name, windowName, &slice, 1000, sliceValue);

	circleValues(0, 0);

	while (true)
	{
		int c = waitKey(20);
		if ((char)c == 27)
			exit(0);
	}



	return 0;
}


void circleValues(int, void*)
{
	GaussianBlur(input, output, Size(9, 9), 2, 2);
	vector<Vec3f> circles;

	if (param1 < 8)
		param1 = 8;

	if (param2 < param1)
		param2 = param1;

	HoughCircles(output, circles, CV_HOUGH_GRADIENT, 1, 2, param1, param2, 0, 0);
	
	cout << "detectes " << circles.size() << " circles.\n";

	for (size_t i = 0; i < circles.size(); i++)
	{
		Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
		int radius = cvRound(circles[i][2]);
		// circle center
		circle(output, center, 3, Scalar(0, 255, 0), -1, 8, 0);
		// circle outline
		circle(output, center, radius, Scalar(0, 0, 255), 1, 8, 0);
	}
	imshow(windowName, output);

}

void sliceValue(int, void*)
{


	circleValues(0,0);
}