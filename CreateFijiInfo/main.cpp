#include <string>
#include <vector>
#include <iostream>
#include <cassert>
#include <fstream>

#include <FreeImage.h>



using namespace std;


static inline string getBaseFilename(const string& filename)
{
	// remove path
	return filename.substr(filename.find_last_of("\\") + 1);
}

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		cerr << "Usage " << argv[0] << "<stack0> [<stack1> <stackN> ...]\n";

#ifdef _WIN32
		system("pause");
#endif

		return 1;
	}


	// assume we are operating on '.'
	vector<string> files;

	string path("./");
	
	// read all filenames
	for (int i = 1; i < argc; ++i)
	{
		string filename(argv[i]);


		if (i == 1)
			path = filename.substr(0, filename.find_last_of("\\"));

		files.push_back(filename);

		
	}
	
	path += "/registration/";






	for (auto f = files.begin(); f != files.end(); ++f)
	{

		try
		{

			// load image

			FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, f->c_str(), FALSE, TRUE);

			if (!fmb)
				throw std::runtime_error("Unable to open image \"" + *f + "\"!");

			assert(fmb);

			// get the dimensions
			int zSlices = FreeImage_GetPageCount(fmb);
			bool initialized = false;


			// read the image dimensions from the first slice
			FIBITMAP* bm = FreeImage_LockPage(fmb, 0);
			int w = FreeImage_GetWidth(bm);
			int h = FreeImage_GetHeight(bm);

			FreeImage_UnlockPage(fmb, bm, FALSE);

			cout << "[Stack] Loaded image stack: " << w << "x" << h << "x" << zSlices << endl;

			FreeImage_CloseMultiBitmap(fmb);


			const string fn = path + getBaseFilename(*f);

			ofstream dimFile(fn + ".dim");
			assert(dimFile.is_open());
			dimFile << "image width: " << w << endl;
			dimFile << "image height: " << h << endl;
			dimFile << "image depth: " << zSlices << endl;


			ofstream beadsFile(fn + ".beads.txt");
			assert(beadsFile.is_open());
			beadsFile << "ID	ViewID	Lx \tLy \tLz \tWx \tWy \tWz \tWeight \tDescCorr \tRansacCorr\n";


			ifstream regoFileIn(*f + ".registration.txt");
			ofstream regoFileOut(fn + ".registration");
			assert(regoFileIn.is_open());
			assert(regoFileOut.is_open());

			for (int i = 0; i < 4; ++i)
				for (int k = 0; k < 4; ++k)
				{
					float m = 0.f;
					regoFileIn >> m;
					regoFileOut << "m" << i << k << ": " << m << endl;
				}

			regoFileOut << "AffineModel3D\n";
			regoFileOut << endl;

			// note this should all be saved from prev runs!
			regoFileOut << "minError: 0\n";
			regoFileOut << "avgError: 0\n";
			regoFileOut << "maxError: 0\n";
			regoFileOut << endl;

			float zscaling = 1.f;
			regoFileOut << "z - scaling : " << zscaling << endl;
			
			
			regoFileOut << "Angle Specific Average Error : -1.0\n";
			regoFileOut << "Overlapping Views : " << files.size() << endl;;

			regoFileOut << "Num beads having true correspondences : 0\n";
			regoFileOut << "Sum of true correspondences pairs : 0\n";
			regoFileOut << "Num beads having correspondences candidates : 0\n";
			regoFileOut << "Sum of correspondences candidates pairs : 0\n";
			regoFileOut << endl;

			for (auto g = files.begin(); g != files.end(); ++g)
			{
				if (f != g)
				{
					regoFileOut << *g << " - Average Error: -1\n";
					regoFileOut << *g << " - Bead Correspondences: 3\n";
					regoFileOut << *g << " - Ransac Correspondences: 0\n";
					regoFileOut << endl;
				}
			}
		}
		catch (const runtime_error& e)
		{
			cerr << "[Error] " << e.what() << endl;
		}
	}



#ifdef _WIN32
	system("pause");
#endif

	return 0;
}