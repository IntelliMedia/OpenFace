///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, Carnegie Mellon University and University of Cambridge,
// all rights reserved.
//
// ACADEMIC OR NON-PROFIT ORGANIZATION NONCOMMERCIAL RESEARCH USE ONLY
//
// BY USING OR DOWNLOADING THE SOFTWARE, YOU ARE AGREEING TO THE TERMS OF THIS LICENSE AGREEMENT.  
// IF YOU DO NOT AGREE WITH THESE TERMS, YOU MAY NOT USE OR DOWNLOAD THE SOFTWARE.
//
// License can be found in OpenFace-license.txt

//     * Any publications arising from the use of this software, including but
//       not limited to academic journal and conference publications, technical
//       reports and manuals, must cite at least one of the following works:
//
//       OpenFace: an open source facial behavior analysis toolkit
//       Tadas Baltru�aitis, Peter Robinson, and Louis-Philippe Morency
//       in IEEE Winter Conference on Applications of Computer Vision, 2016  
//
//       Rendering of Eyes for Eye-Shape Registration and Gaze Estimation
//       Erroll Wood, Tadas Baltru�aitis, Xucong Zhang, Yusuke Sugano, Peter Robinson, and Andreas Bulling 
//       in IEEE International. Conference on Computer Vision (ICCV),  2015 
//
//       Cross-dataset learning and person-speci?c normalisation for automatic Action Unit detection
//       Tadas Baltru�aitis, Marwa Mahmoud, and Peter Robinson 
//       in Facial Expression Recognition and Analysis Challenge, 
//       IEEE International Conference on Automatic Face and Gesture Recognition, 2015 
//
//       Constrained Local Neural Fields for robust facial landmark detection in the wild.
//       Tadas Baltru�aitis, Peter Robinson, and Louis-Philippe Morency. 
//       in IEEE Int. Conference on Computer Vision Workshops, 300 Faces in-the-Wild Challenge, 2013.    
//
///////////////////////////////////////////////////////////////////////////////
// FaceTrackingVid.cpp : Defines the entry point for the console application for tracking faces in videos.

// Libraries for landmark detection (includes CLNF and CLM modules)
#include "LandmarkCoreIncludes.h"
#include "DeviceEnumerator.h"
#include "GazeEstimation.h"
#include "FaceAnalyser.h"
#include "Face_utils.h"

// OpenCV includes
#include <opencv2/videoio/videoio.hpp>  // Video write
#include <opencv2/videoio/videoio_c.h>  // Video write
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <SequenceCapture.h>
#include <Visualizer.h>
#include <VisualizationUtils.h>

// Boost includes
#include <filesystem.hpp>
#include <filesystem/fstream.hpp>


#define INFO_STREAM( stream ) \
std::cout << stream << std::endl

#define WARN_STREAM( stream ) \
std::cout << "Warning: " << stream << std::endl

#define ERROR_STREAM( stream ) \
std::cout << "Error: " << stream << std::endl

static void printErrorAndAbort( const std::string & error )
{
    std::cout << error << std::endl;
    abort();
}

#define FATAL_STREAM( stream ) \
printErrorAndAbort( std::string( "Fatal error: " ) + stream )

using namespace std;

// Global Vars :v 

bool detection_success=false;
cv::Vec6d pose_estimate;
cv::Point3f gazeDirection0(0, 0, -1);
cv::Point3f gazeDirection1(0, 0, -1);

double au_intensity[50];
double au_presence[50];
// To store data in C++
char* au_intensity_name[20];
char* au_presence_name[20];
// To pass to Unity (C#)
char au_intensity_name_concat[20 * 5];
char au_presence_name_concat[20 * 5];

float landmark_doubleArray[136];
float landmark_doubleArray3D[204];

cv::Mat_<float> landmarks_2D;
cv::Mat_<float> landmarks_3D;

bool isChangeCam = false;
bool isRunning = true;
DeviceEnumerator de;
std::map<int, Device> video_devices = de.getVideoDevicesMap();
int device_id = 0;

// DLL FNs

extern "C"
{
	__declspec(dllexport) int __stdcall getAUIntensity(void** ppDoubleArrayReceiver)
	{
		*ppDoubleArrayReceiver = (void*)au_intensity;
		return 0;
	}
}

extern "C"
{
	__declspec(dllexport) int __stdcall getAUIntensityName(char* intensityString)
	{
		strcpy(au_intensity_name_concat, au_intensity_name[0]);
		for (int i = 1; i < 20; i++)
		{
			strcat(au_intensity_name_concat, au_intensity_name[i]);
		}

		strcpy(intensityString, au_intensity_name_concat);
		return 0;
	}
}

extern "C"
{
	__declspec(dllexport) int __stdcall getAUPresence(void** ppDoubleArrayReceiver)
	{
		*ppDoubleArrayReceiver = (void*)au_presence;
		return 0;
	}
}

extern "C"
{
	__declspec(dllexport) int __stdcall getAUPresenceName(char* presenceString)
	{
		strcpy(au_presence_name_concat, au_presence_name[0]);
		//*ppStringArrayReceiver = (void*)au_presence_name;
		for (int i = 1; i < 20; i++)
		{
			strcat(au_presence_name_concat, au_presence_name[i]);
		}

		strcpy(presenceString, au_presence_name_concat);
		return 0;
	}
}

extern "C" __declspec(dllexport) int __stdcall getXYZ(void** ppDoubleArrayReceiver)
{
	*ppDoubleArrayReceiver = (void*)landmark_doubleArray3D;
	return 0;
}

extern "C" __declspec(dllexport) int __stdcall getXY(void** ppDoubleArrayReceiver)
{
	*ppDoubleArrayReceiver = (void*)landmark_doubleArray;
	return 0;
}

extern "C"
{
	__declspec(dllexport) int __stdcall getdetection_success() {
		return detection_success;
	}
}

extern "C"
{
	__declspec(dllexport) float __stdcall get_pose1()
	{
		return pose_estimate[3];
	}
}

extern "C"
{
	__declspec(dllexport) float __stdcall get_pose2()
	{
		return pose_estimate[4];
	}
}

extern "C"
{
	__declspec(dllexport) float __stdcall get_pose3()
	{
		return pose_estimate[5];
	}
}

extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze1()
	{
		return gazeDirection0.x;
	}
}
extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze2()
	{
		return gazeDirection0.y;
	}
}
extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze3()
	{
		return gazeDirection0.z;
	}
}
extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze4()
	{
		return gazeDirection1.x;
	}
}
extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze5()
	{
		return gazeDirection1.y;
	}
}
extern "C"
{
	__declspec(dllexport) float __stdcall get_gaze6()
	{
		return gazeDirection1.z;
	}
}

extern "C"
{
	__declspec(dllexport) void __stdcall change_camera()
	{
		isChangeCam = true;
	}
}

vector<string> get_arguments(int argc, char **argv)
{

	vector<string> arguments;

	for(int i = 0; i < argc; ++i)
	{
		arguments.push_back(string(argv[i]));
	}
	return arguments;
}

extern "C"
{
	__declspec(dllexport) void __stdcall quit()
	{
		isRunning = false;
	}
}

// The Main Function 

extern "C"
{
	__declspec(dllexport) int __stdcall main ()
{
	int argcc = 3;
	char* argvv[4];

	// FaceLandmarkVid.exe Need to be changed : I hope I don't forget about it :v
	// Device is 0 for laptop webcam, it can get 1...

	for (auto const &device : video_devices) {
		if (device.second.deviceName.find("Logitech") != string::npos)
		{
			device_id = device.first;
		}
	}

	// Get a exe version of CPP with the changed path
	argvv[0] = "";
	argvv[1] = "-device";
	argvv[2] = (char*)to_string(device_id).c_str();
	argvv[3] = NULL;

	vector<string> arguments = get_arguments(argcc, argvv);

	// no arguments: output usage
	if (arguments.size() == 1)
	{
		cout << "For command line arguments see:" << endl;
		cout << " https://github.com/TadasBaltrusaitis/OpenFace/wiki/Command-line-arguments";
		return 0;
	}

	LandmarkDetector::FaceModelParameters det_parameters(arguments);

	// The modules that are being used for tracking
	LandmarkDetector::CLNF face_model(det_parameters.model_location);

	// AUs
	FaceAnalysis::FaceAnalyserParameters face_analysis_params(arguments);
	FaceAnalysis::FaceAnalyser face_analyser(face_analysis_params);

	// Open a sequence
	Utilities::SequenceCapture sequence_reader;

	// A utility for visualizing the results --	vis_track;vis_hog;vis_align;vis_aus;
	Utilities::Visualizer visualizer(true, true, true, true);

	// Tracking FPS for visualization
	Utilities::FpsTracker fps_tracker;
	fps_tracker.AddFrame();

	int sequence_number = 0;
	for (int i = 0; i < 20; i++)
	{
		au_presence_name[i] = new char[5];
		au_intensity_name[i] = new char[5];
	}

	while (isRunning) // this is not a for loop as we might also be reading from a webcam
	{
		// The sequence reader chooses what to open based on command line arguments provided
		if (!sequence_reader.Open(arguments))
			break;

		INFO_STREAM("Device or file opened");
		cv::Mat captured_image = sequence_reader.GetNextFrame();
		INFO_STREAM("Starting tracking");
		while (!captured_image.empty() && isRunning) // this is not a for loop as we might also be reading from a webcam
		{
			if (isChangeCam)
			{
				if (device_id == 0)
				{
					argvv[2] = "1";
					device_id = 1;
				}
				else if (device_id == 1)
				{
					argvv[2] = "0";
					device_id = 0;
				}

				arguments = get_arguments(argcc, argvv);

				face_model.Reset();
				face_analyser.Reset();

				LandmarkDetector::FaceModelParameters det_parameters(arguments);
				LandmarkDetector::CLNF face_model(det_parameters.model_location);

				// AUs
				FaceAnalysis::FaceAnalyserParameters face_analysis_params(arguments);
				FaceAnalysis::FaceAnalyser face_analyser(face_analysis_params);
				
				// A utility for visualizing the results
				Utilities::Visualizer visualizer(true, true, true, true);

				// Tracking FPS for visualization
				Utilities::FpsTracker fps_tracker;
				fps_tracker.AddFrame();

				if (!sequence_reader.Open(arguments))
					break;

				isChangeCam = false;
			}

			// Reading the images
			cv::Mat_<uchar> grayscale_image = sequence_reader.GetGrayFrame();

			// The actual facial landmark detection / tracking
			detection_success = LandmarkDetector::DetectLandmarksInVideo(captured_image, face_model, det_parameters, grayscale_image);

			// Gaze tracking, absolute gaze direction
			// If tracking succeeded and we have an eye model, estimate gaze
			if (detection_success && face_model.eye_model)
			{
				GazeAnalysis::EstimateGaze(face_model, gazeDirection0, sequence_reader.fx, sequence_reader.fy, sequence_reader.cx, sequence_reader.cy, true);
				GazeAnalysis::EstimateGaze(face_model, gazeDirection1, sequence_reader.fx, sequence_reader.fy, sequence_reader.cx, sequence_reader.cy, false);

				// Fill the XY position to send it to C# :D

				face_model.pdm.CalcShape2D(landmarks_2D, face_model.params_local, face_model.params_global);

				for (int i = 0; i < 136; i++) {
					landmark_doubleArray[i] = landmarks_2D.at<float>(i);
				}

				face_model.pdm.CalcShape3D(landmarks_3D, face_model.params_local);

				for (int i = 0; i < 204; i++) {
					landmark_doubleArray3D[i] = landmarks_3D.at<float>(i);
				}

				/*
				cv::Mat_<float> landmarks_3D;
				face_model.pdm.CalcShape3D(landmarks_3D, face_model.params_local);

				cout << "0: ";
				for (int i = 0; i < 204; i++)
				{
					cout << landmarks_3D.at<float>(i) << " - ";
				}
				cout << endl;

				cv::Mat_<float> landmarks_2D;
				face_model.pdm.CalcShape2D(landmarks_2D, face_model.params_local, face_model.params_global);

				cout << endl << "1: ";
				for (int i = 0; i < 136; i++)
				{
					cout << landmarks_2D.at<float>(i) << " - ";
				}
				cout << endl;
				*/
			}

			// Work out the pose of the head from the tracked model
			pose_estimate = LandmarkDetector::GetPose(face_model, sequence_reader.fx, sequence_reader.fy, sequence_reader.cx, sequence_reader.cy);

			cv::Mat sim_warped_img;
			cv::Mat_<double> hog_descriptor; int num_hog_rows = 0, num_hog_cols = 0;

			face_analyser.AddNextFrame(captured_image, face_model.detected_landmarks, face_model.detection_success, sequence_reader.time_stamp, true);
			face_analyser.GetLatestAlignedFace(sim_warped_img);
			face_analyser.GetLatestHOG(hog_descriptor, num_hog_rows, num_hog_cols); 

			std::vector<std::pair<std::string, double>> currentAUsIntensity = face_analyser.GetCurrentAUsReg();
			std::vector<std::pair<std::string, double>> currentAUsPresence = face_analyser.GetCurrentAUsClass();
			for (int i = 0; i < currentAUsPresence.size(); i++)
			{
				au_presence[i] = (double)currentAUsPresence[i].second;
				strcpy(au_presence_name[i], currentAUsPresence[i].first.c_str());
				// au_presence_name[i][4] = '\n';
			}

			for (int i = 0; i < currentAUsIntensity.size(); i++)
			{
				au_intensity[i] = (double)currentAUsIntensity[i].second;
				strcpy(au_intensity_name[i], currentAUsIntensity[i].first.c_str());
				// au_intensity_name[i][4] = '\n';
			}

			// Keeping track of FPS
			fps_tracker.AddFrame();

			// Displaying the tracking visualizations
			visualizer.SetImage(captured_image, sequence_reader.fx, sequence_reader.fy, sequence_reader.cx, sequence_reader.cy);
			visualizer.SetObservationLandmarks(face_model.detected_landmarks, face_model.detection_certainty, face_model.GetVisibilities());
			visualizer.SetObservationPose(pose_estimate, face_model.detection_certainty);
			visualizer.SetObservationGaze(gazeDirection0, gazeDirection1, LandmarkDetector::CalculateAllEyeLandmarks(face_model), LandmarkDetector::Calculate3DEyeLandmarks(face_model, sequence_reader.fx, sequence_reader.fy, sequence_reader.cx, sequence_reader.cy), face_model.detection_certainty);
			visualizer.SetObservationActionUnits(face_analyser.GetCurrentAUsReg(), face_analyser.GetCurrentAUsClass());
			visualizer.SetFps(fps_tracker.GetFPS());
			// detect key presses (due to pecularities of opencv, you can get it when displaying images)
			char character_press = visualizer.ShowObservation();

			// restart the tracker
			//if (character_press == 'r')
			//{
			//	face_model.Reset();
			//}
			// quit the application
			//else if (character_press == 'q')
			//{
			//	face_model.Reset();
			//	return(0);
			//}

			// Grabbing the next frame in the sequence
			captured_image = sequence_reader.GetNextFrame();

		}
		
		// Reset the model, for the next video
		face_analyser.Reset();
		face_model.Reset();

		sequence_number++;

	}
	isRunning = true;
	return 0;
}
}
