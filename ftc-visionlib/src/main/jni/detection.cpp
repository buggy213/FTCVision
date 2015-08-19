//
// Implements advanced object detection methods
//

#include <jni.h>
#include <vector>
#include <stdio.h>
#include <android/log.h>

#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/flann/flann.hpp"

using namespace std;
using namespace cv;

extern "C"
{

static const int DESIRED_FTRS = 200;

static Mat img_object;

static GridAdaptedFeatureDetector detector(new FastFeatureDetector(10, true), DESIRED_FTRS, 4, 4);
static BriefDescriptorExtractor extractor(32);
static BFMatcher matcher(NORM_HAMMING);

static std::vector<KeyPoint> keypoints_object;
static Mat descriptors_object;

JNIEXPORT void JNICALL Java_com_lasarobotics_vision_detection_Detection_analyzeObject(JNIEnv* jobject, jlong addrGrayObject) {

    //initModules_nonfree();

    img_object = *(Mat*)addrGrayObject;

    if( !img_object.data )
    { printf(" --(!) Error reading images "); return; }

    //-- Step 1: Detect the keypoints using SURF Detector

    //detect
    //detector = makePtr<DynamicAdaptedFeatureDetector>(DynamicAdaptedFeatureDetector(new FastAdjuster(20, true)));
    detector.detect( img_object, keypoints_object );

    //extract
    //extractor = FREAK::create();
    extractor.compute( img_object, keypoints_object, descriptors_object);

    __android_log_print(ANDROID_LOG_ERROR, "ftcvision", "Object ananlyzed!");
}

JNIEXPORT void JNICALL Java_com_lasarobotics_vision_detection_Detection_findObject(JNIEnv* jobject, jlong addrGrayObject, jlong addrGrayScene, jlong addrOutput) {

    Mat img_scene = *(Mat*)addrGrayScene;
    Mat img_matches = *(Mat*)addrOutput;

    if( !img_object.data || !img_scene.data )
    { printf(" --(!) Error reading images "); return; }

    // detect
    std::vector<KeyPoint> keypoints_scene;
    detector.detect( img_scene, keypoints_scene );

    // extract
    Mat descriptors_scene;
    extractor.compute( img_scene, keypoints_scene, descriptors_scene );

    if ((descriptors_object.cols != descriptors_scene.cols) ||
        (descriptors_object.type() != descriptors_scene.type()))
    { return; }

    // match
    std::vector<DMatch> matches;
    matcher.match(descriptors_object, descriptors_scene, matches);

    double max_dist = 0; double min_dist = 100;

    //-- Quick calculation of max and min distances between keypoints
    for( int i = 0; i < descriptors_object.rows; i++ )
    { double dist = matches[i].distance;
        if( dist < min_dist ) min_dist = dist;
        if( dist > max_dist ) max_dist = dist;
    }

    printf("-- Max dist : %f \n", max_dist );
    printf("-- Min dist : %f \n", min_dist );

    //-- Draw only "good" matches (i.e. whose distance is less than 3*min_dist )
    std::vector< DMatch > good_matches;

    for( int i = 0; i < descriptors_object.rows; i++ )
    { if( matches[i].distance < 3*min_dist )
        { good_matches.push_back( matches[i]); }
    }

    //drawMatches( img_object, keypoints_object, img_scene, keypoints_scene,
    //             good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
    //             vector<char>(), DrawMatchesFlags::DRAW_OVER_OUTIMG);

    //-- Localize the object
    std::vector<Point2f> obj;
    std::vector<Point2f> scene;

    for( int i = 0; i < good_matches.size(); i++ )
    {
        //-- Get the keypoints from the good matches
        obj.push_back( keypoints_object[ good_matches[i].queryIdx ].pt );
        scene.push_back( keypoints_scene[ good_matches[i].trainIdx ].pt );
    }

    if ((obj.size() < 4) || (scene.size() < 4))
    {
        return;
    }

    Mat H = findHomography( obj, scene, RANSAC );

    //-- Get the corners from the image_1 ( the object to be "detected" )
    std::vector<Point2f> obj_corners(4);
    obj_corners[0] = cvPoint(0,0);
    obj_corners[1] = cvPoint( img_object.cols, 0 );
    obj_corners[2] = cvPoint( img_object.cols, img_object.rows );
    obj_corners[3] = cvPoint( 0, img_object.rows );
    std::vector<Point2f> scene_corners(4);

    if(obj_corners.size() != scene_corners.size())
    {
        return;
    }
    if (H.cols != 3 || H.rows != 3) { return; }

    perspectiveTransform( obj_corners, scene_corners, H); //TODO this function throws libc fatal signal 11 (SIGSEGV)

    //-- Draw lines between the corners (the mapped object in the scene - image_2 )
    line( img_matches, scene_corners[0] + Point2f( img_object.cols, 0),
          scene_corners[1] + Point2f( img_object.cols, 0), Scalar(0, 255, 0), 4 );
    line( img_matches, scene_corners[1] + Point2f( img_object.cols, 0),
          scene_corners[2] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
    line( img_matches, scene_corners[2] + Point2f( img_object.cols, 0),
          scene_corners[3] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
    line( img_matches, scene_corners[3] + Point2f( img_object.cols, 0),
          scene_corners[0] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );

    //-- Show detected matches
    //imshow( "Good Matches & Object detection", img_matches );

    //return;*/
}

} //extern "C"