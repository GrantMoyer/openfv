//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//                           License Agreement
//                For Open Source Flow Visualization Library
//
// Copyright 2013-2017 Abhishek Bajpayee
//
// This file is part of OpenFV.
//
// OpenFV is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License version 2 as published by the Free Software Foundation.
//
// OpenFV is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License version 2 for more details.
//
// You should have received a copy of the GNU General Public License version 2 along with
// OpenFV. If not, see https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html.

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
#include "refocusing.h"
#include "serialization.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>

using namespace std;
using namespace cv;

/*!
  Class with functions to create a synthetic particle seeded volume.
 */
class Scene {

 public:
    ~Scene() {

    }

    Scene();

    /*! Create a scene with given size specifications
      \param sx Size of volume in physical units in x direction
      \param sy Size of volume in physical units in y direction
      \param sz Size of volume in physical units in z direction
      \param gpu Flag to use GPU or not. If turned on, this will run all calculations using a Scene object on a GPU.
    */
    void create(double sx, double sy, double sz, int gpu);

#ifndef WITHOUT_CUDA
    //! Set gpu flag explicitly
    void setGpuFlag(int gpu);
#endif

    //! Set flag to seed particles in a cylindrical region in the specified bounds
    void setCircVolFlag(int flag);

    //! Render all voxels of the volume
    void renderVolume(int xv, int yv, int zv);
    void renderVolumeCPU(int xv, int yv, int zv);
    void renderVolumeCPU2(int xv, int yv, int zv);

#ifndef WITHOUT_CUDA
    void renderVolumeGPU(int xv, int yv, int zv);
    void renderVolumeGPU2(int xv, int yv, int zv);
#endif

    //! Set the standard deviation of particles in each direction
    void setParticleSigma(double sx, double sy, double sz);
    /*! Set geometry of scene if refractive interfaces have to be used
      \param zW The z location of the front of glass wall
      \param n1 Refractive index of air
      \param n2 Refractive index of glass
      \param n3 Refractive index of water
      \param t Thickness of glass wall
    */
    void setRefractiveGeom(float zW, float n1, float n2, float n3, float t);
    void setActiveFrame(int frame);
    int getActiveFrame() { return frame_; }

    void seedR();
    void seedAxes();
    void seedFromFile(string path);
    void seedParticles(vector< vector<double> > locations);
    /*! Randomly seed particles in a scene
      \param num Number of particles to seed
      \param factor Portion of volume to be seeded with particles in each direction
    */
    void seedParticles(int num, double factor);

    /*! Propagate particles in a scene using a user defined velocity function over time
      \param func Function of form func(x, y, z, t) which returns new particle location for a particle at
      (x, y, z) when propagated over time t
      \param t Time over which to propagate particles
    */
    void propagateParticles(vector<double> (*func)(double, double, double, double), double t);

    //! Get a slice of volume with index z_ind (based on the size of volume sz and number of voxels zv).
    Mat getSlice(int z_ind);
    //! Get the entire rendered volume
    vector<Mat> getVolume();

    Mat getParticles();
    vector<float> getRefGeom();
    int getRefFlag();
    vector<int> getVoxelGeom();
    vector<double> getSceneGeom();
    double sigma();
    int getNumParticles() { return trajectory_[0].cols; }

    void temp();

    void dumpStack(string);

  private:

    // Function to serialize and save Scene object
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & sigmax_ & sigmay_ & sigmaz_;
        ar & xlims_ & ylims_ & zlims_;
        ar & sx_ & sy_ & sz_;
        ar & vx_ & vy_ & vz_;
        ar & voxelsX_, voxelsY_, voxelsZ_;
        // ar & particles_;
        ar & trajectory_;
        // ar & volumeGPU_;
        ar & volumesGPU_;
        // ar & volumeCPU_;
        ar & volumesCPU_;
        ar & GPU_FLAG;
        ar & REF_FLAG;
        ar & CIRC_VOL_FLAG;
        ar & geom_;
        ar & frame_;
    }

    double f(double x, double y, double z);

    double sigmax_, sigmay_, sigmaz_;
    double dthresh_;
    vector<double> xlims_, ylims_, zlims_;
    // size of volume in units ([mm] mostly)
    double sx_, sy_, sz_;
    // size of volume in voxels
    int vx_, vy_, vz_;
    vector<double> voxelsX_, voxelsY_, voxelsZ_;

    Mat_<double> particles_;
    vector< Mat_<double> > trajectory_;
    vector<Mat> volumeGPU_, volumeCPU_;
    vector< vector<Mat> > volumesGPU_, volumesCPU_;

    int frame_; // active frame

#ifndef WITHOUT_CUDA
    gpu::GpuMat gx, gy;
    gpu::GpuMat tmp1, tmp2, tmp3, tmp4;
    gpu::GpuMat slice;
#endif

    int REF_FLAG;
    int GPU_FLAG;
    int CIRC_VOL_FLAG;
    vector<float> geom_;

};

/*!
  Class to create synthetic cameras and render images of a scene
*/
class Camera {

 public:
    ~Camera() {

    }

    Camera();

    /*! Initialize a camera
      \param f Focal length of camera [in?]
      \param imsx Image size in pixels in x direction
      \param imsy Image size in pixels in y direction
      \param gpu Flag to use GPU or not
    */
    void init(double f, int imsx, int imsy, int gpu);
    //! Attach Scene object to camera
    void setScene(Scene &scene);
    //! Set location of camera
    void setLocation(double x, double y, double z);
    //! Point camera at point
    void pointAt(double x, double y, double z);

    void setCustomParticleSigma(double sigma);

    void setRefShift(double shift) { LOG(INFO) << "Setting ref_shift to " << shift; ref_shift_ = shift; }

    //! Render image of attached scene
    Mat render();
    void renderCPU();

#ifndef WITHOUT_CUDA
    void renderGPU();
#endif

    //! Get camera matrix of camera
    Mat getP();
    //! Get location of camera
    Mat getC();

 private:

    Mat Rt();
    void project();
    double f(double x, double y);
    void img_refrac(Mat_<double> Xcam, Mat_<double> X, Mat_<double> &Xout);

    int imsx_, imsy_, cx_, cy_;
    double f_;
    double ref_shift_;

    Mat_<double> C_;
    Mat_<double> t_;
    Mat_<double> R_;
    Mat_<double> K_;
    Mat_<double> P_;

    // Rendered image
    Mat render_;

    // TODO: name these better
    Mat_<double> p_;
    Mat_<double> s_;

#ifndef WITHOUT_CUDA
    gpu::GpuMat gx, gy, tmp1, tmp2, img;
#endif

    Scene scene_;
    int REF_FLAG;
    int GPU_FLAG;
    vector<float> geom_;

    // Expert mode stuff
    int CUSTOM_PARTICLE_SIGMA;
    double custom_sigma_;

};

/*
class benchmark {

 public:
    ~benchmark() {

    }

    benchmark() {}

    void benchmarkSA(Scene scene, saRefocus refocus);
    double calcQ(double thresh, int mult, double mult_exp);

 private:

    Scene scene_;
    saRefocus refocus_;

};
*/

#endif
