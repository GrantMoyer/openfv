// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- Rendering Library Header ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#ifndef RENDERING_LIBRARY
#define RENDERING_LIBRARY

#include "std_include.h"
#include "typedefs.h"

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;

class Scene {

 public:
    ~Scene() {

    }

    Scene();

    void create(double sx, int vx, double sy, int vy, double sz, int vz);
    void setRefractiveGeom(float zW, float n1, float n2, float n3, float t);

    void seedR();
    void seedParticles(vector< vector<double> > locations);
    void seedParticles(int num);    

    Mat getImg(int zv);

    Mat getParticles();
    vector<float> getRefGeom();
    int getRefFlag();
    double sigma();

 private:

    void createVolume();
    vector<voxel> getVoxels(int z);
    double f(double x, double y, double z);

    double sigma_;
    vector<double> xlims_, ylims_, zlims_;
    double sx_, sy_, sz_;
    int vx_, vy_, vz_;
    vector<double> voxelsX_, voxelsY_, voxelsZ_;

    Mat_<double> particles_;
    vector<voxel> volume_;

    int REF_FLAG;
    vector<float> geom_;

};

class Camera {

 public:
    ~Camera() {

    }

    Camera();

    void init(double f, int imsx, int imsy);
    void setScene(Scene scene);
    void setLocation(double x, double y, double z);
    void pointAt(double x, double y, double z);
    
    Mat render();

    Mat getP();
    Mat getC();

 private:

    Mat Rt();
    void project();
    double f(double x, double y);
    void img_refrac(Mat_<double> Xcam, Mat_<double> X, Mat_<double> &Xout);

    int imsx_, imsy_, cx_, cy_;
    double f_;

    Mat_<double> C_;
    Mat_<double> t_;
    Mat_<double> R_;
    Mat_<double> K_;
    Mat_<double> P_;

    // TODO: name these better
    Mat_<double> p_;
    Mat_<double> s_;

    Scene scene_;
    int REF_FLAG;
    vector<float> geom_;

    //private members here

};


#endif
