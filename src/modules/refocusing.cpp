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
// --- Refocusing Library ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#include <GL/glfw.h>

#include "refocusing.h"
#include "tools.h"

using namespace std;
using namespace cv;
using namespace libtiff;

saRefocus::saRefocus() {

    LOG(INFO)<<"Refocusing object created in expert mode";
    LOG(INFO)<<"Note: requires manual tweaking of parameters!";

    GPU_FLAG=1;
    GPU_MATS_UPLOADED=false;
    REF_FLAG=0;
    CORNER_FLAG=1;
    MTIFF_FLAG=0;
    INVERT_Y_FLAG=0;
    EXPERT_FLAG=1;
    STDEV_THRESH=0;
    SINGLE_CAM_DEBUG=0;
    mult_=0;
    minlos_=0;
    nlca_=0;
    nlca_fast_=0;
    nlca_win_ = 32;
    delta_ = 0.1;
    frames_.push_back(0);
    num_cams_ = 0;
    IMG_REFRAC_TOL = 1E-9;
    MAX_NR_ITERS = 20;
    BENCHMARK_MODE = 0;
    INT_IMG_MODE = 0;

    z_ = 0; dz_ = 0.1;
    xs_ = 0; ys_ = 0; zs_ = 0; dx_ = 0.1; dy_ = 0.1;
    rx_ = 0; ry_ = 0; rz_ = 0; drx_ = 1.0; dry_ = 1.0; drz_ = 1.0;
    cxs_ = 0; cys_ = 0; czs_ = 0;
    crx_ = 0; cry_ = 0; crz_ = 0;

    initGLFW();
}

saRefocus::saRefocus(int num_cams, double f) {

    LOG(INFO)<<"Refocusing object created in expert mode";
    LOG(INFO)<<"Note: requires manual tweaking of parameters!";

    GPU_FLAG=1;
    GPU_MATS_UPLOADED=false;
    REF_FLAG=0;
    CORNER_FLAG=0;
    MTIFF_FLAG=0;
    INVERT_Y_FLAG=0;
    EXPERT_FLAG=1;
    mult_=0;
    minlos_=0;
    frames_.push_back(0);
    num_cams_ = num_cams;
    scale_ = f;
    IMG_REFRAC_TOL = 1E-9;
    MAX_NR_ITERS = 20;
    BENCHMARK_MODE = 0;
    INT_IMG_MODE = 0;

    initGLFW();
}

saRefocus::saRefocus(refocus_settings settings):
    GPU_FLAG(settings.use_gpu), CORNER_FLAG(settings.hf_method), MTIFF_FLAG(settings.mtiff), mult_(settings.mult), minlos_(settings.minlos), nlca_(settings.nlca), nlca_fast_(settings.nlca_fast), weighting_mode_(settings.weighting), ALL_FRAME_FLAG(settings.all_frames), start_frame_(settings.start_frame), end_frame_(settings.end_frame), skip_frame_(settings.skip), RESIZE_IMAGES(settings.resize_images), rf_(settings.rf), UNDISTORT_IMAGES(settings.undistort) {

#ifdef WITHOUT_CUDA
    if (GPU_FLAG)
        LOG(FATAL)<<"OpenFV was compiled without GPU support! Switch GPU option to OFF.";
#endif

    GPU_MATS_UPLOADED=false;
    STDEV_THRESH = 1;
    IMG_REFRAC_TOL = 1E-9;
    MAX_NR_ITERS = 20;
    BENCHMARK_MODE = 0;
    INT_IMG_MODE = 0;
    SINGLE_CAM_DEBUG = 0;

    imgs_read_ = 0;
    read_calib_data(settings.calib_file_path);

    if (mult_ + minlos_ + nlca_ + nlca_fast_ > 1)
        LOG(FATAL) << "Multiple reconstructions options (mult, minlos, nlca, nlca_fast) cannot be ON!";
    
    nlca_win_ = settings.nlca_win;
    delta_ = settings.delta;
    mult_exp_ = settings.mult_exp;

    if (nlca_fast_) {
        LOG(WARNING) << "Make sure the input images are well normalized and particle peak values are close to 1 for fast NLCA to work well!";
    }

    if (MTIFF_FLAG) {

        if(!ALL_FRAME_FLAG) {

        for (int i=start_frame_; i<=end_frame_; i+=skip_frame_+1)
            frames_.push_back(i);
        }
        read_imgs_mtiff(settings.images_path);

    } else {

        read_imgs(settings.images_path);

    }

    if (nlca_ || nlca_fast_)
        if (num_cams_ != 4)
            LOG(FATAL) << "NLCA and fast NLCA modes are currently only supported for 4 cameras!";

#ifndef WITHOUT_CUDA
    if (GPU_FLAG) {
        initializeGPU();
    }
#endif

    z_ = 0; dz_ = 0.1;
    xs_ = 0; ys_ = 0; zs_ = 0; dx_ = 0.1; dy_ = 0.1;
    rx_ = 0; ry_ = 0; rz_ = 0; drx_ = 1.0; dry_ = 1.0; drz_ = 1.0;
    cxs_ = 0; cys_ = 0; czs_ = 0;
    crx_ = 0; cry_ = 0; crz_ = 0;

    initGLFW();
}

void saRefocus::initGLFW() {
    if (!glfwInit()) {
        LOG(FATAL) << "FAILED TO INITIALIZE GLFW";
        exit(1);
    }
    context = glfwOpenWindow(1, 1, 0, 0, 0, 0, 0, 0, GLFW_WINDOW);
    int major, minor, rev;
    glfwGetGLVersion(&major, &minor, &rev);
    LOG(INFO)<<"INITIALIZED GLFW, OPENGL VERSION "
             << major << '.' << minor << '-' << rev << "...";
}

saRefocus::~saRefocus() {
    glfwTerminate();
    LOG(INFO)<<"TERMINATED GLFW!";
}

void saRefocus::read_calib_data(string path) {

    ifstream file;

    file.open(path.c_str());
    if(file.fail())
        LOG(FATAL)<<"Could not open calibration file! Terminating...";

    LOG(INFO)<<"LOADING CALIBRATION DATA...";

    string time_stamp;
    getline(file, time_stamp);
    VLOG(3)<<time_stamp;

    double avg_reproj_error_;
    file>>avg_reproj_error_;

    file>>img_size_.width;
    file>>img_size_.height;
    file>>scale_;

    file>>num_cams_;
    fact_ = Scalar(1/double(num_cams_));

    string cam_name;

    for (int n=0; n<num_cams_; n++) {

        for (int i=0; i<2; i++) getline(file, cam_name);
        VLOG(3)<<"cam_names_["<<n<<"] = "<<cam_name<<endl;
        // Clean \r at end of cam_name
        if (cam_name[cam_name.length()-1] == '\r') {
            VLOG(3)<<"Seems like calibration file was created in a windows env. Removing carriage return at end.";
            cam_name.erase(cam_name.length()-1);
        }
        cam_names_.push_back(cam_name);

        Mat_<double> P_mat = Mat_<double>::zeros(3,4);
        for (int i=0; i<3; i++) {
            for (int j=0; j<4; j++) {
                file>>P_mat(i,j);
            }
        }
        P_mats_.push_back(P_mat);
        VLOG(3)<<"P_mat["<<n<<"]"<<endl<<P_mat<<endl;

        Mat_<double> loc = Mat_<double>::zeros(3,1);
        for (int i=0; i<3; i++)
            file>>loc(i,0);

        VLOG(3)<<"cam_locations_["<<n<<"]"<<endl<<loc<<endl;
        cam_locations_.push_back(loc);

    }

    file>>REF_FLAG;
    if (REF_FLAG) {
        VLOG(1)<<"Calibration is refractive";
        file>>geom[0]; file>>geom[4]; file>>geom[1]; file>>geom[2]; file>>geom[3];
    } else {
        VLOG(1)<<"Calibration is pinhole";
    }

    // checking for camera name clashes
    for (int i=0; i<num_cams_; i++) {
        for (int j=0; j<num_cams_; j++) {
            if (i==j)
                continue;
            else
                if (cam_names_[i].compare(cam_names_[j]) == 0)
                    LOG(FATAL) << "Camera name clash detected! cam_name[" << i << "] is same as cam_name[" << j << "]";
        }
    }
                    

    VLOG(1)<<"DONE READING CALIBRATION DATA";

}

void saRefocus::read_imgs(string path) {

    DIR *dir;
    struct dirent *ent;

    string dir1(".");
    string dir2("..");
    string temp_name;
    string img_prefix = "";

    Mat image, fimage;

    vector<string> img_names;

    if(!imgs_read_) {

        LOG(INFO)<<"READING IMAGES TO REFOCUS...";

        VLOG(1)<<"UNDISTORT_IMAGES flag is "<<UNDISTORT_IMAGES;
        
        int size = 0;
        for (int i=0; i<num_cams_; i++) {

            VLOG(1)<<"Camera "<<i+1<<" of "<<num_cams_<<"..."<<endl;

            string path_tmp;
            vector<Mat> refocusing_imgs_sub;

            path_tmp = path+cam_names_[i]+"/";

            if (!boost::filesystem::is_directory(path_tmp))
                LOG(FATAL) << "Directory for camera " << cam_names_[i] << " does not exist!";

            int hidden=0;
            dir = opendir(path_tmp.c_str());
            while(ent = readdir(dir)) {
                temp_name = ent->d_name;
                if (temp_name.compare(dir1)) {
                    if (temp_name.compare(dir2)) {
                        if (temp_name[0] != '.') {
                            string path_img = path_tmp+temp_name;
                            img_names.push_back(path_img);
                        } else {
                            hidden=1;
                        }
                    }
                }
            }

            if (hidden)
                LOG(WARNING) << "Camera folders seem to contain hidden files (filenames starting with '.')!";

            // validate number of images in folder not 0
            if (img_names.size() == 0)
                LOG(FATAL) << "No images in " << cam_names_[i] << "!";

            sort(img_names.begin(), img_names.end());

            // check if number of frames in camera folders
            // equal or not
            if (i==0)
                img_names_ = img_names;
            else
                if (img_names.size() != img_names_.size())
                    LOG(FATAL) << "Number of images in camera folder for " << cam_names_[i] << " not equal to images in folder for " << cam_names_[0] << "! They must be same in order to ensure syncing.";

            // check if names of corresponding images are same or not
            for (int f=0; f<img_names.size(); f++)
                if (strcmp(explode(img_names_[f].c_str(), '/').back().c_str(), explode(img_names[f].c_str(), '/').back().c_str()) != 0)
                    LOG(FATAL) << "Name of image " << f << " (" << img_names[f] << ") in camera folder for " << cam_names_[i] << " not same as corresponding image (" << img_names_[f] << ") in camera folder for " << cam_names_[0] << "! This could be because image names in camera folders are not the same or they do not naturally sort well.";

            int begin;
            int end;
            int skip;

            if(ALL_FRAME_FLAG) {
                begin = 0;
                end = img_names.size();
                skip = 0;
            } else {
                begin = start_frame_;
                end = end_frame_+1;
                skip = skip_frame_;
                if (end>img_names.size()) {
                    LOG(FATAL)<<"End frame is greater than number of frames in " << cam_names_[i] << "!";
                    end = img_names.size();
                }
            }

            for (int j=begin; j<end; j+=skip+1) {

                VLOG(1)<<j<<": "<<img_names.at(j)<<endl;
                image = imread(img_names.at(j), 0);

                if (j==begin) {
                    img_size_ = Size(image.cols, image.rows);
                    updateHinv();
                }

                Mat image2;
                if (UNDISTORT_IMAGES) {
                    fisheye::undistortImage(image, image2, K_mats_[i], dist_coeffs_[i], K_mats_[i]);
                } else {
                    image2 = image.clone();
                }

                refocusing_imgs_sub.push_back(image2);
                if (i==0) {
                    frames_.push_back(j);
                }

            }
            img_names.clear();

            imgs.push_back(refocusing_imgs_sub);
            path_tmp = "";

            VLOG(1)<<"done!\n";
            imgs_read_ = 1;

        }

        generate_stack_names();
        initializeRefocus();

        VLOG(1)<<"DONE READING IMAGES"<<endl;
    } else {
        LOG(INFO)<<"Images already read!"<<endl;
    }

}

void saRefocus::read_imgs_mtiff(string path) {

    LOG(INFO)<<"READING IMAGES TO REFOCUS...";

    DIR *dir;
    struct dirent *ent;

    string dir1(".");
    string dir2("..");
    string temp_name;

    vector<string> img_names;

    dir = opendir(path.c_str());
    while(ent = readdir(dir)) {
        temp_name = ent->d_name;
        if (temp_name.compare(dir1)) {
            if (temp_name.compare(dir2)) {
                if (temp_name.compare(temp_name.size()-3,3,"tif") == 0) {
                    string img_name = path+temp_name;
                    img_names.push_back(img_name);
                }
            }
        }
    }

    sort(img_names.begin(), img_names.end());

    if (img_names.size() != num_cams_)
        LOG(FATAL) << "Number of mtiff files in " << path << " not equal to the number of cameras in the calibration file!";

    LOG(WARNING) << "Camera names from calibration file are not automatically matched to names of mtiff files! Please ensure the following mappings are correct:";
    for (int i=0; i<num_cams_; i++)
        LOG(INFO) << cam_names_[i] << " -> " << img_names[i];

    VLOG(1)<<"mtiff files in path:";
    vector<mtiffReader> tiffs;
    int size = 0;
    for (int i=0; i<img_names.size(); i++) {
        VLOG(1)<<img_names[i]<<endl;
        mtiffReader tiff(img_names[i]);
        VLOG(2)<<tiff.num_frames()<<" frames in file.";

        // check if number of frames in mtiff files
        // equal or not
        if (i==0)
            size = tiff.num_frames();
        else
            if (tiff.num_frames() != size)
                LOG(WARNING) << "Number of frames in " << img_names[i] << " not equal to frames in " << img_names[0] << "! Corresponding frames will be read in order from beginning. Syncing might be off.";

        tiffs.push_back(tiff);
    }

    if (ALL_FRAME_FLAG) {
        VLOG(1)<<"READING ALL FRAMES...";
        for (int i=0; i<tiffs[0].num_frames(); i++)
            frames_.push_back(i);
    }

    VLOG(1)<<"Reading images...";
    for (int n=0; n<img_names.size(); n++) {

        VLOG(1)<<"Camera "<<n+1<<"...";

        if (frames_.back() > tiffs[n].num_frames())
            LOG(FATAL) << "End frame greater than the number of frames in " << img_names[n] << "!";

        vector<Mat> refocusing_imgs_sub;
        int count=0;
        for (int f=0; f<frames_.size(); f++) {
            Mat img = tiffs[n].get_frame(frames_.at(f));
            refocusing_imgs_sub.push_back(img.clone());
            count++;
        }

        imgs.push_back(refocusing_imgs_sub);
        VLOG(1)<<"done! "<<count<<" frames read.";

    }

    initializeRefocus();

    VLOG(1)<<"DONE READING IMAGES"<<endl;

}

void saRefocus::CPUliveView() {

    //initializeCPU();

    if (CORNER_FLAG) {
        LOG(INFO)<<"Using corner based homography fit method..."<<endl;
    } else {
        LOG(INFO)<<"Using full refractive calculation method..."<<endl;
    }

    active_frame_ = 0; thresh_ = 0;

    namedWindow("Live View", CV_WINDOW_AUTOSIZE);

    if (REF_FLAG) {
        if (CORNER_FLAG) {
            CPUrefocus_ref_corner(1, active_frame_);
        } else {
            CPUrefocus_ref(1, active_frame_);
        }
    } else {
        CPUrefocus(1, active_frame_);
    }

    double dthresh = 5/255.0;
    double tlimit = 1.0;
    double mult_exp_limit = 1.0;
    double mult_thresh = 0.01;

    while( 1 ){
        int key = cvWaitKey(10);
        VLOG(3)<<"Key press: "<<(key & 255)<<endl;

        if ( (key & 255)!=255 ) {

            if ( (key & 255)==83 ) {
                z_ += dz_;
            } else if( (key & 255)==81 ) {
                z_ -= dz_;
            } else if( (key & 255)==82 ) {
                if (mult_) {
                    if (mult_exp_<mult_exp_limit)
                        mult_exp_ += mult_thresh;
                } else {
                    if (thresh_<tlimit)
                        thresh_ += dthresh;
                }
            } else if( (key & 255)==84 ) {
                if (mult_) {
                    if (mult_exp_>0)
                        mult_exp_ -= mult_thresh;
                } else {
                    if (thresh_>0)
                        thresh_ -= dthresh;
                }
            } else if( (key & 255)==46 ) { // >
                if (active_frame_<imgs[0].size()-1) {
                    active_frame_++;
                }
            } else if( (key & 255)==44 ) { // <
                if (active_frame_>0) {
                    active_frame_--;
                }
            } else if( (key & 255)==119 ) { // w
                rx_ += 1;
            } else if( (key & 255)==113 ) { // q
                rx_ -= 1;
            } else if( (key & 255)==115 ) { // s
                ry_ += 1;
            } else if( (key & 255)==97 ) {  // a
                ry_ -= 1;
            } else if( (key & 255)==120 ) { // x
                rz_ += 1;
            } else if( (key & 255)==122 ) { // z
                rz_ -= 1;
            } else if( (key & 255)==114 ) { // r
                xs_ += 1;
            } else if( (key & 255)==101 ) { // e
                xs_ -= 1;
            } else if( (key & 255)==102 ) { // f
                ys_ += 1;
            } else if( (key & 255)==100 ) { // d
                ys_ -= 1;
            } else if( (key & 255)==118 ) { // v
                zs_ += 1;
            } else if( (key & 255)==99 ) {  // c
                zs_ -= 1;
            } else if( (key & 255)==117 ) { // u
                crx_ += 1;
            } else if( (key & 255)==121 ) { // y
                crx_ -= 1;
            } else if( (key & 255)==106 ) { // j
                cry_ += 1;
            } else if( (key & 255)==104 ) { // h
                cry_ -= 1;
            } else if( (key & 255)==109 ) { // m
                crz_ += 1;
            } else if( (key & 255)==110 ) { // n
                crz_ -= 1;
            } else if( (key & 255)==32 ) {
                mult_ = (mult_+1)%2;
            } else if( (key & 255)==27 ) {  // ESC
                cvDestroyAllWindows();
                break;
            }

            // Call refocus function
            if(REF_FLAG) {
                if (CORNER_FLAG) {
                    CPUrefocus_ref_corner(1, active_frame_);
                } else {
                    CPUrefocus_ref(1, active_frame_);
                }
            } else {
                CPUrefocus(1, active_frame_);
            }

        }

        /*if ( (key & 255)!=255 ) {

            if ( (key & 255)==83 ) {
                z_ += dz;
            } else if( (key & 255)==81 ) {
                z_ -= dz;
            } else if( (key & 255)==82 ) {
                if (thresh<tlimit) {
                    thresh += dthresh;
                }
            } else if( (key & 255)==84 ) {
                if (thresh>0) {
                    thresh -= dthresh;
                }
            } else if( (key & 255)==46 ) {
                if (active_frame_<array_all.size()) {
                    active_frame_++;
                }
            } else if( (key & 255)==44 ) {
                if (active_frame_<array_all.size()) {
                    active_frame_--;
                }
            } else if( (key & 255)==27 ) {
                break;
            }

            // Call refocus function
            if(REF_FLAG) {
                if (CORNER_FLAG) {
                    CPUrefocus_ref_corner(z_, thresh, 1, active_frame_);
                } else {
                    CPUrefocus_ref(z_, thresh, 1, active_frame_);
                }
            } else {
                CPUrefocus(z_, thresh, 1, active_frame_);
            }

            }*/

    }

}

void saRefocus::generate_stack_names() {

    VLOG(1) << "Generating names of folders in which stacks will be saved...";

    for (int i=0; i<img_names_.size(); i++) {
        string img_name = explode(img_names_[i].c_str(), '/').back();
        string stack_name = explode(img_name.c_str(), '.')[0];
        VLOG(1) << img_name << " --> " << stack_name;
        stack_names_.push_back(stack_name);
    }

}

void saRefocus::initializeRefocus() {

    // This functions converts any incoming datatype images to
    // CV_32F ranging between 0 and 1
    // Note this assumes that black and white pixel value depends
    // on the datatype
    // TODO: add ability to handle more data types

    int type = imgs[0][0].type();

    for (int i=0; i<imgs.size(); i++) {
        for (int j=0; j<imgs[i].size(); j++) {

            Mat img;
            switch(type) {

            case CV_8U:
                if (INT_IMG_MODE) {
                    break;
                }
                if (i==0 && j==0) {
                    VLOG(3)<<"Converting images from CV_8U type to CV_32F type...";
                }
                imgs[i][j].convertTo(img, CV_32F);
                img /= 255.0;
                imgs[i][j] = img.clone();
                break;

            case CV_16U:
                if (i==0 && j==0) {
                    VLOG(3)<<"Converting images from CV_16U type to CV_32F type...";
                }
                imgs[i][j].convertTo(img, CV_32F);
                img /= 65535.0;
                imgs[i][j] = img.clone();
                break;

            case CV_32F:
                if (i==0 && j==0) {
                    VLOG(3)<<"Images already CV_32F type...";
                }
                break;

            case CV_64F:
                if (i==0 && j==0) {
                    VLOG(3)<<"Converting images from CV_64F type to CV_32F type...";
                }
                imgs[i][j].convertTo(img, CV_32F);
                imgs[i][j] = img.clone();
                break;

            }

        }
    }

    if (weighting_mode_ > 0)
        weight_images();

    // preprocess();

}

void saRefocus::initializeCPU() {

    // stuff

}

Mat saRefocus::refocus(double z, double rx, double ry, double rz, double thresh, int frame) {

    z_ = z;
    rx_ = rx;
    ry_ = ry;
    rz_ = rz;
    if (STDEV_THRESH) {
        thresh_ = thresh;
    } else {
        thresh_ = thresh/255.0;
    }

    if (REF_FLAG) {
        if (CORNER_FLAG) {

#ifndef WITHOUT_CUDA
            if (GPU_FLAG) {
                GPUrefocus_ref_corner(0, frame);
            }
#endif

            if (!GPU_FLAG) {
                CPUrefocus_ref_corner(0, frame);
            }

        } else {

#ifndef WITHOUT_CUDA
            if (GPU_FLAG) {
                GPUrefocus_ref(0, frame);
            }
#endif

            if (!GPU_FLAG) {
                CPUrefocus_ref(0, frame);
            }

        }
    } else {

#ifndef WITHOUT_CUDA
        if (GPU_FLAG) {
            GPUrefocus(0, frame);
        }
#endif

        if (!GPU_FLAG) {
            CPUrefocus(0, frame);
        }

    }

    return(result_);

}

#ifndef WITHOUT_CUDA

// ---GPU Refocusing Functions Begin--- //

// initialize default GPU
void saRefocus::initializeGPU() {

    if (!EXPERT_FLAG) {

        LOG(INFO)<<"INITIALIZING GPU..."<<endl;

        VLOG(1)<<"CUDA Enabled GPU Devices: "<<gpu::getCudaEnabledDeviceCount;

        gpu::DeviceInfo gpuDevice(gpu::getDevice());

        VLOG(1)<<"---"<<gpuDevice.name()<<"---"<<endl;
        VLOG(1)<<"Total Memory: "<<(gpuDevice.totalMemory()/pow(1024.0,2))<<" MB";

    }

    if (REF_FLAG)
        if (!CORNER_FLAG)
            uploadToGPU_ref();

}

// initialize specified GPU
void saRefocus::initializeSpecificGPU(int gpu) {
    
    LOG(WARNING) << "Explicitly setting GPU to device number " << gpu << ". This is an expert function!";

    gpu::setDevice(gpu);
    
    if (REF_FLAG)
        if (!CORNER_FLAG)
            uploadToGPU_ref();

}

// TODO: Right now this function just starts uploading images
//       without checking if there is enough free memory on GPU
//       or not.
void saRefocus::uploadAllToGPU() {

    if (!EXPERT_FLAG) {
        gpu::DeviceInfo gpuDevice(gpu::getDevice());
        double free_mem_GPU = gpuDevice.freeMemory()/pow(1024.0,2);
        VLOG(1)<<"Free Memory before: "<<free_mem_GPU<<" MB";
    }

    if (!GPU_MATS_UPLOADED) {

        Mat blank(img_size_.height, img_size_.width, CV_32F, Scalar(0));
        blank_.upload(blank);
        Mat blank_int(img_size_.height, img_size_.width, CV_8UC1, Scalar(0));
        blank_int_.upload(blank_int);

        for (int i=0; i<num_cams_; i++) {
            warped_.push_back(blank_.clone());
            warped2_.push_back(blank_.clone());
        }

        GPU_MATS_UPLOADED = true;
    
    }

    VLOG(1)<<"Uploading all frames to GPU...";
    for (int i=0; i<imgs[0].size(); i++) {
        for (int j=0; j<num_cams_; j++) {
            temp.upload(imgs[j][i]);
            array.push_back(temp.clone());
        }
        array_all.push_back(array);
        array.clear();
    }

    if (!EXPERT_FLAG) {
        gpu::DeviceInfo gpuDevice(gpu::getDevice());
        VLOG(1)<<"Free Memory after: "<<(gpuDevice.freeMemory()/pow(1024.0,2))<<" MB";
    }

}

void saRefocus::uploadSingleToGPU(int frame) {

    VLOG(1)<<"Uploading frame " << frame << " to GPU...";

    if (!GPU_MATS_UPLOADED) {

        Mat blank(img_size_.height, img_size_.width, CV_32F, Scalar(0));
        blank_.upload(blank);
        Mat blank_int(img_size_.height, img_size_.width, CV_8UC1, Scalar(0));
        blank_int_.upload(blank_int);

        for (int i=0; i<num_cams_; i++) {
            warped_.push_back(blank_.clone());
            warped2_.push_back(blank_.clone());
        }

        GPU_MATS_UPLOADED = true;
    
    }

    array_all.clear();
    array.clear();
    for (int j=0; j<num_cams_; j++) {
        temp.upload(imgs[j][frame]);
        array.push_back(temp.clone());
    }
    array_all.push_back(array);
    array.clear();

}

void saRefocus::uploadToGPU_ref() {

    VLOG(1)<<"Uploading data required by full refocusing method to GPU...";

    Mat_<float> D = Mat_<float>::zeros(3,3);
    D(0,0) = scale_; D(1,1) = scale_;
    D(0,2) = img_size_.width*0.5; D(1,2) = img_size_.height*0.5;
    D(2,2) = 1;
    Mat Dinv = D.inv();

    float hinv[6];
    hinv[0] = Dinv.at<float>(0,0); hinv[1] = Dinv.at<float>(0,1); hinv[2] = Dinv.at<float>(0,2);
    hinv[3] = Dinv.at<float>(1,0); hinv[4] = Dinv.at<float>(1,1); hinv[5] = Dinv.at<float>(1,2);

    float locations[9][3];
    float pmats[9][12];
    for (int i=0; i<9; i++) {
        for (int j=0; j<3; j++) {
            locations[i][j] = cam_locations_[i].at<double>(j,0);
            for (int k=0; k<4; k++) {
                pmats[i][j*4+k] = P_mats_[i].at<double>(j,k);
            }
        }
    }

    uploadRefractiveData(hinv, locations, pmats, geom);

    Mat blank(img_size_.height, img_size_.width, CV_32F, float(0));
    xmap.upload(blank); ymap.upload(blank);
    temp.upload(blank); temp2.upload(blank);
    // refocused.upload(blank);

    for (int i=0; i<9; i++) {
        xmaps.push_back(xmap.clone());
        ymaps.push_back(ymap.clone());
    }

    VLOG(1)<<"done!";

}

void saRefocus::GPUrefocus(int live, int frame) {

    int curve = 0;
    Mat_<double> x = Mat_<double>::zeros(img_size_.height, img_size_.width);
    Mat_<double> y = Mat_<double>::zeros(img_size_.height, img_size_.width);
    Mat xm, ym;

    if (INT_IMG_MODE)
        refocused = blank_int_.clone();
    else
        refocused = blank_.clone();

    Mat H, trans;

    for (int i=0; i<num_cams_; i++) {

        if (curve) {
            calc_refocus_map(x, y, i); x.convertTo(xm, CV_32FC1); y.convertTo(ym, CV_32FC1); xmap.upload(xm); ymap.upload(ym);
            gpu::remap(array_all[frame][i], warped_[i], xmap, ymap, INTER_LINEAR);
        } else {
            calc_refocus_H(i, H);
            gpu::warpPerspective(array_all[frame][i], warped_[i], H, img_size_);
            if (SINGLE_CAM_DEBUG) {
                Mat single_cam_img(warped_[i]);
                cam_stacks_[i].push_back(single_cam_img.clone());
            }
        }

        if (mult_) {
            gpu::pow(warped_[i], mult_exp_, warped2_[i]);
            if (i>0)
                gpu::multiply(refocused, warped2_[i], refocused);
            else
                refocused = warped2_[i].clone();
	} else if (minlos_) {
            if (i>0)
                gpu::min(refocused, warped_[i], refocused);
            else
                refocused = warped_[i].clone();
        } else if (!nlca_ && !nlca_fast_) {
            gpu::multiply(warped_[i], fact_, warped2_[i]);
            gpu::add(refocused, warped2_[i], refocused);
        }

    }

    if (nlca_)
        gpu_calc_nlca_image(warped_, refocused, img_size_.height, img_size_.width, nlca_win_, delta_);
    else if (nlca_fast_)
        gpu_calc_nlca_image_fast(warped_, refocused, img_size_.height, img_size_.width, delta_);
    else
        if (!BENCHMARK_MODE)
            threshold_image(refocused);

    refocused.download(refocused_host_);

    if (live)
        liveViewWindow(refocused_host_);

    //refocused_host_.convertTo(result, CV_8U);
    result_ = refocused_host_.clone();

}

void saRefocus::GPUrefocus_ref(int live, int frame) {

    if (INT_IMG_MODE)
        refocused = blank_int_.clone();
    else
        refocused = blank_.clone();

    for (int i=0; i<num_cams_; i++) {

        gpu_calc_refocus_map(xmap, ymap, z_, i, img_size_.height, img_size_.width);
        gpu::remap(array_all[frame][i], warped_[i], xmap, ymap, INTER_LINEAR);

        if (i==0) {
            Mat M;
            xmap.download(M); // writeMat(M, "../temp/xmap.txt");
            ymap.download(M); // writeMat(M, "../temp/ymap.txt");
        }

        gpu::multiply(warped_[i], fact_, warped2_[i]);
        gpu::add(refocused, warped2_[i], refocused);

    }

    if (!BENCHMARK_MODE)
        threshold_image(refocused);

    refocused.download(refocused_host_);

    if (live)
        liveViewWindow(refocused_host_);

    result_ = refocused_host_.clone();

}

void saRefocus::GPUrefocus_ref_corner(int live, int frame) {

    if (INT_IMG_MODE)
        refocused = blank_int_.clone();
    else
        refocused = blank_.clone();

    Mat H;

    for (int i=0; i<num_cams_; i++) {

        calc_ref_refocus_H(i, H);
        gpu::warpPerspective(array_all[frame][i], warped_[i], H, img_size_);

        if (SINGLE_CAM_DEBUG) {
            Mat single_cam_img(warped_[i]);
            cam_stacks_[i].push_back(single_cam_img.clone());
        }

        if (mult_) {
            gpu::pow(warped_[i], mult_exp_, warped2_[i]);
            if (i>0)
                gpu::multiply(refocused, warped2_[i], refocused);
            else
                refocused = warped2_[i].clone();
        } else if (minlos_) {
            if (i>0)
                gpu::min(refocused, warped_[i], refocused);
            else
                refocused = warped_[i].clone();
        } else if (!nlca_ && !nlca_fast_) {
            gpu::multiply(warped_[i], fact_, warped2_[i]);
            gpu::add(refocused, warped2_[i], refocused);
        }

    }

    if (nlca_)
        gpu_calc_nlca_image(warped_, refocused, img_size_.height, img_size_.width, nlca_win_, delta_);
    else if (nlca_fast_)
        gpu_calc_nlca_image_fast(warped_, refocused, img_size_.height, img_size_.width, delta_);
    else
        if (!BENCHMARK_MODE)
            threshold_image(refocused);

    refocused.download(refocused_host_);

    if (live)
        liveViewWindow(refocused_host_);

    result_ = refocused_host_.clone();

}

void saRefocus::cb_mult(int state, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->mult_ = state;
    if (state) {
        ref->minlos_ = 0;
        ref->nlca_ = 0;
        ref->nlca_fast_ = 0;
    }
    ref->updateLiveFrame();

}

void saRefocus::cb_mlos(int state, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->minlos_ = state;
    if (state) {
        ref->mult_ = 0;
        ref->nlca_ = 0;
        ref->nlca_fast_ = 0;
    }
    ref->updateLiveFrame();

}

void saRefocus::cb_nlca(int state, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->nlca_ = state;
    if (state) {
        ref->nlca_fast_ = 0;
        ref->minlos_ = 0;
        ref->mult_ = 0;
    }
    ref->updateLiveFrame();

}

void saRefocus::cb_nlca_fast(int state, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->nlca_fast_ = state;
    if (state) {
        ref->minlos_ = 0;
        ref->mult_ = 0;
        ref->nlca_ = 0;
    }
    ref->updateLiveFrame();

}

void saRefocus::cb_frames(int frame, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->active_frame_ = frame;
    ref->updateLiveFrame();

}

void saRefocus::cb_dz_p1(int val, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->dz_ = 0.1;
    ref->updateLiveFrame();

}

void saRefocus::cb_dz_1(int val, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->dz_ = 1.0;
    ref->updateLiveFrame();

}

void saRefocus::cb_dz_10(int val, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->dz_ = 10.0;
    ref->updateLiveFrame();

}

void saRefocus::cb_dz_100(int val, void* userdata) {

    saRefocus* ref = reinterpret_cast<saRefocus*>(userdata);
    ref->dz_ = 100.0;
    ref->updateLiveFrame();

}

void saRefocus::GPUliveView() {

    // uploading all frames so that navigation in time
    // is possible in real time
    uploadAllToGPU();

    if (REF_FLAG) {
        if (CORNER_FLAG) {
            LOG(INFO)<<"Using corner based homography fit method..."<<endl;
        } else {
            LOG(INFO)<<"Using full refractive calculation method..."<<endl;
        }
    } else {
        LOG(INFO)<<"Using pinhole refocusing..."<<endl;
    }

    active_frame_ = 0;
    thresh_ = 0;
    double dthresh, tulimit, tllimit;
    if (STDEV_THRESH) {
        dthresh = 0.1;
        tulimit = 5.0;
        tllimit = -1.0;
    } else {
        dthresh = 5/255.0;
        tulimit = 1.0;
        tllimit = 0.0;
    }
    double mult_exp_limit = 5.0;
    double mult_thresh = 0.01;
    double ddelta = 0.01;

    namedWindow("Live View", CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO | CV_GUI_EXPANDED);

    if (array_all.size()-1)
        createTrackbar("Frame", "Live View", &active_frame_, array_all.size()-1, cb_frames, this);

    createButton("Multiplicative", cb_mult, this, CV_CHECKBOX);
    createButton("MLOS", cb_mlos, this, CV_CHECKBOX);
    createButton("NLCA", cb_nlca, this, CV_CHECKBOX);
    createButton("Fast NLCA", cb_nlca_fast, this, CV_CHECKBOX);

    createButton("dz = 0.1", cb_dz_p1, this, CV_RADIOBOX, 1);
    createButton("dz = 1", cb_dz_1, this, CV_RADIOBOX, 0);
    createButton("dz = 10", cb_dz_10, this, CV_RADIOBOX, 0);
    createButton("dz = 100", cb_dz_100, this, CV_RADIOBOX, 0);
    
    // cvCreateTrackbar("drx", NULL, &initrv, 10, cb_drx);
    // createTrackbar("dry", NULL, &initrv, 10, cb_dry, this);

    updateLiveFrame();

    while( 1 ){
        int key = cvWaitKey(10);
        VLOG(4)<<"Key press: "<<(key & 255)<<endl;

        if ( (key & 255)!=255 ) {

            if ( (key & 255)==83 ) {
                z_ += dz_;
            } else if( (key & 255)==81 ) {
                z_ -= dz_;
            } else if( (key & 255)==61 ) {
                if (nlca_ || nlca_fast_) {
                    delta_ += ddelta;
                } else if (mult_) {
                    if (mult_exp_<mult_exp_limit)
                        mult_exp_ += mult_thresh;
                } else {
                    if (thresh_<tulimit)
                        thresh_ += dthresh;
                }
            } else if( (key & 255)==45 ) {
                if (nlca_ || nlca_fast_) {
                    if (delta_>0.01)
                        delta_ -= ddelta;
                } else if (mult_) {
                    if (mult_exp_>0)
                        mult_exp_ -= mult_thresh;
                } else {
                    if (thresh_>tllimit)
                        thresh_ -= dthresh;
                }
            } else if( (key & 255)==46 ) {
                z_ += dz_;
            } else if( (key & 255)==44 ) {
                z_ -= dz_;
            } else if( (key & 255)==119 ) { // w
                rx_ += drx_;
            } else if( (key & 255)==113 ) { // q
                rx_ -= drx_;
            } else if( (key & 255)==115 ) { // s
                ry_ += dry_;
            } else if( (key & 255)==97 ) {  // a
                ry_ -= dry_;
            } else if( (key & 255)==120 ) { // x
                rz_ += drz_;
            } else if( (key & 255)==122 ) { // z
                rz_ -= drz_;
            } else if( (key & 255)==114 ) { // r
                xs_ += dx_;
            } else if( (key & 255)==101 ) { // e
                xs_ -= dx_;
            } else if( (key & 255)==102 ) { // f
                ys_ += dy_;
            } else if( (key & 255)==100 ) { // d
                ys_ -= dy_;
            } else if( (key & 255)==118 ) { // v
                zs_ += 1;
            } else if( (key & 255)==99 ) {  // c
                zs_ -= 1;
            } else if( (key & 255)==117 ) { // u
                crx_ += 1;
            } else if( (key & 255)==121 ) { // y
                crx_ -= 1;
            } else if( (key & 255)==106 ) { // j
                cry_ += 1;
            } else if( (key & 255)==104 ) { // h
                cry_ -= 1;
            } else if( (key & 255)==109 ) { // m
                crz_ += 1;
            } else if( (key & 255)==110 ) { // n
                crz_ -= 1;
            } else if( (key & 255)==32 ) {
                mult_ = (mult_+1)%2;
            } else if( (key & 255)==27 ) {  // ESC
                cvDestroyAllWindows();
                break;
            }

            updateLiveFrame();

        }

    }

}

void saRefocus::updateLiveFrame() {

    // Call refocus function
    if(REF_FLAG) {
        if (CORNER_FLAG) {
            GPUrefocus_ref_corner(1, active_frame_);
        } else {
            GPUrefocus_ref(1, active_frame_);
        }
    } else {
        GPUrefocus(1, active_frame_);
    }

}

#endif

// ---GPU Refocusing Functions End--- //

// ---CPU Refocusing Functions Begin--- //

void saRefocus::CPUrefocus(int live, int frame) {

    Scalar fact = Scalar(1/double(num_cams_));

    Mat H, trans;
    calc_refocus_H(0, H);
    warpPerspective(imgs[0][frame], cputemp, H, img_size_);
    // qimshow(cputemp);

    if (mult_) {
        pow(cputemp, mult_exp_, cputemp2);
    } else if (minlos_) {
        cputemp2 = cputemp.clone();
    } else {
        multiply(cputemp, fact, cputemp2);
    }

    cpurefocused = cputemp2.clone();

    for (int i=1; i<num_cams_; i++) {

        calc_refocus_H(i, H);
        warpPerspective(imgs[i][frame], cputemp, H, img_size_);
        // qimshow(cputemp);

        if (mult_) {
            pow(cputemp, mult_exp_, cputemp2);
            multiply(cpurefocused, cputemp2, cpurefocused);
	} else if (minlos_) {
	    min(cputemp,cpurefocused,cpurefocused);
        } else {
            multiply(cputemp, fact, cputemp2);
            add(cpurefocused, cputemp2, cpurefocused);
        }
    }

    threshold(cpurefocused, cpurefocused, thresh_, 0, THRESH_TOZERO);

    Mat refocused_host_(cpurefocused);

    if (live)
        liveViewWindow(refocused_host_);

    result_ = refocused_host_.clone();

}

void saRefocus::CPUrefocus_ref(int live, int frame) {

    Mat_<double> x = Mat_<double>::zeros(img_size_.height, img_size_.width);
    Mat_<double> y = Mat_<double>::zeros(img_size_.height, img_size_.width);
    calc_ref_refocus_map(cam_locations_[0], z_, x, y, 0);

    Mat res, xmap, ymap;
    x.convertTo(xmap, CV_32FC1);
    y.convertTo(ymap, CV_32FC1);
    remap(imgs[0][frame], res, xmap, ymap, INTER_LINEAR);

    refocused_host_ = res.clone()/double(num_cams_);

    for (int i=1; i<num_cams_; i++) {

        calc_ref_refocus_map(cam_locations_[i], z_, x, y, i);
        x.convertTo(xmap, CV_32FC1);
        y.convertTo(ymap, CV_32FC1);

        remap(imgs[i][frame], res, xmap, ymap, INTER_LINEAR);

        refocused_host_ += res.clone()/double(num_cams_);

    }

    // TODO: thresholding missing?

    if (live)
        liveViewWindow(refocused_host_);

    result_ = refocused_host_.clone();

}

void saRefocus::CPUrefocus_ref_corner(int live, int frame) {

    Mat H;
    calc_ref_refocus_H(0, H);

    Mat res;
    warpPerspective(imgs[0][frame], res, H, img_size_);

    if (mult_) {
        pow(res, mult_exp_, cputemp2);
    } else if (minlos_) {
        cputemp2 = res.clone();
    } else {
        cputemp2 = res.clone()/double(num_cams_);
    }

    refocused_host_ = cputemp2.clone();
    
    for (int i=1; i<num_cams_; i++) {

        calc_ref_refocus_H(i, H);
        warpPerspective(imgs[i][frame], res, H, img_size_);
        
	if (mult_) {
	    pow(res, mult_exp_, cputemp2);
	    multiply(refocused_host_, cputemp2, refocused_host_);
	} else if (minlos_) {
	    min(refocused_host_,res,refocused_host_);
	} else {
            refocused_host_ += res.clone()/double(num_cams_);
	}
    }

    // TODO: thresholding missing?

    if (live)
        liveViewWindow(refocused_host_);

    result_ = refocused_host_.clone();

}

// ---CPU Refocusing Functions End--- //

void saRefocus::calc_ref_refocus_map(Mat_<double> Xcam, double z, Mat_<double> &x, Mat_<double> &y, int cam) {

    int width = img_size_.width;
    int height = img_size_.height;

    Mat_<double> D = Mat_<double>::zeros(3,3);
    D(0,0) = scale_; D(1,1) = scale_;
    D(0,2) = width*0.5;
    D(1,2) = height*0.5;
    D(2,2) = 1;
    Mat hinv = D.inv();

    Mat_<double> X = Mat_<double>::zeros(3, height*width);
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            X(0,i*height+j) = i;
            X(1,i*height+j) = j;
            X(2,i*height+j) = 1;
        }
    }
    X = hinv*X;

    for (int i=0; i<X.cols; i++)
        X(2,i) = z;

    //cout<<"Refracting points"<<endl;
    Mat_<double> X_out = Mat_<double>::zeros(4, height*width);
    img_refrac(Xcam, X, X_out);

    //cout<<"Projecting to find final map"<<endl;
    Mat_<double> proj = P_mats_[cam]*X_out;
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            int ind = i*height+j; // TODO: check this indexing
            proj(0,ind) /= proj(2,ind);
            proj(1,ind) /= proj(2,ind);
            x(j,i) = proj(0,ind);
            y(j,i) = proj(1,ind);
        }
    }

}

void saRefocus::calc_refocus_map(Mat_<double> &x, Mat_<double> &y, int cam) {

    int width = img_size_.width;
    int height = img_size_.height;

    Mat_<double> D = Mat_<double>::zeros(3,3);
    D(0,0) = scale_; D(1,1) = scale_;
    D(0,2) = width*0.5;
    D(1,2) = height*0.5;
    D(2,2) = 1;
    Mat hinv = D.inv();

    Mat_<double> X = Mat_<double>::zeros(3, height*width);
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            X(0,i*height+j) = i;
            X(1,i*height+j) = j;
            X(2,i*height+j) = 1;
        }
    }
    X = hinv*X;

    double r = 50;
    //r = r*warp_factor_;
    Mat_<double> X2 = Mat_<double>::zeros(4, height*width);
    for (int j=0; j<X.cols; j++) {
        X2(0,j) = X(0,j);
        X2(1,j) = X(1,j);
        X2(2,j) = r - r*cos(asin(X(0,j)/r)) + z_;
        X2(3,j) = 1;
    }

    //cout<<"Projecting to find final map"<<endl;
    Mat_<double> proj = P_mats_[cam]*X2;
    for (int i=0; i<width; i++) {
        for (int j=0; j<height; j++) {
            int ind = i*height+j; // TODO: check this indexing
            proj(0,ind) /= proj(2,ind);
            proj(1,ind) /= proj(2,ind);
            x(j,i) = proj(0,ind);
            y(j,i) = proj(1,ind);
        }
    }

}

void saRefocus::calc_ref_refocus_H(int cam, Mat &H) {

    Mat_<double> X = Mat_<double>::zeros(3, 4);
    X(0,0) = 0;                 X(1,0) = 0;
    X(0,3) = img_size_.width-1; X(1,3) = 0;
    X(0,2) = img_size_.width-1; X(1,2) = img_size_.height-1;
    X(0,1) = 0;                 X(1,1) = img_size_.height-1;
    X = hinv_*X;

    // Mat R = getRotMat(rx_, ry_, rz_);
    // X = R*X;

    Mat_<double> X2 = Mat_<double>::zeros(3, 4);
    for (int j=0; j<X.cols; j++) {
        X2(0,j) = X(0,j) + xs_;
        X2(1,j) = X(1,j) + ys_;
        X2(2,j) = X(2,j) + z_;
    }

    Mat_<double> X_out = Mat_<double>::zeros(4, 4);
    img_refrac(cam_locations_[cam], X2, X_out);

    Mat_<double> proj = P_mats_[cam]*X_out;

    Point2f src, dst;
    vector<Point2f> sp, dp;
    int i, j;

    for (int i=0; i<X.cols; i++) {
        src.x = X(0,i); src.y = X(1,i);
        dst.x = proj(0,i)/proj(2,i); dst.y = proj(1,i)/proj(2,i);
        sp.push_back(src); dp.push_back(dst);
    }

    H = findHomography(dp, sp, 0);
    H = D_*H;

}

void saRefocus::calc_refocus_H(int cam, Mat &H) {

    Mat_<double> X = Mat_<double>::zeros(3, 4);
    X(0,0) = 0;                 X(1,0) = 0;
    X(0,1) = img_size_.width-1; X(1,1) = 0;
    X(0,2) = img_size_.width-1; X(1,2) = img_size_.height-1;
    X(0,3) = 0;                 X(1,3) = img_size_.height-1;
    X = hinv_*X;

    Mat_<double> X2 = Mat_<double>::zeros(4, 4);

    for (int i=0; i<X.cols; i++)
        X(2,i) = 0; //z_;

    Mat R = getRotMat(rx_, ry_, rz_);
    X = R*X;

    for (int j=0; j<X.cols; j++) {
        X2(0,j) = X(0,j) + xs_;
        X2(1,j) = X(1,j) + ys_;
        X2(2,j) = X(2,j)+z_;
        X2(3,j) = 1;
    }

    Mat_<double> proj = P_mats_[cam]*X2;

    Point2f src, dst;
    vector<Point2f> sp, dp;
    int i, j;

    for (int i=0; i<X.cols; i++) {
        src.x = X(0,i); src.y = X(1,i);
        dst.x = proj(0,i)/proj(2,i); dst.y = proj(1,i)/proj(2,i);
        sp.push_back(src); dp.push_back(dst);
    }

    H = findHomography(dp, sp, 0);
    H = D_*H;

}

// Function to project a 3D point into cam^th camera
// TODO: for now this assumes that scene is already refractive
Mat saRefocus::project_point(int cam, Mat_<double> X) {

    Mat_<double> X_out = Mat_<double>::zeros(4,1);
    img_refrac(cam_locations_[cam], X, X_out);

    Mat_<double> proj = P_mats_[cam]*X_out;

    Mat_<double> X_img = Mat_<double>::zeros(2,1);
    X_img(0,0) = proj(0,0)/proj(2,0);
    X_img(1,0) = proj(1,0)/proj(2,0);

    return X_img;

}

void saRefocus::img_refrac(Mat_<double> Xcam, Mat_<double> X, Mat_<double> &X_out) {

    float zW_ = geom[0]; float n1_ = geom[1]; float n2_ = geom[2]; float n3_ = geom[3]; float t_ = geom[4];

    double c[3];
    for (int i=0; i<3; i++)
        c[i] = Xcam.at<double>(0,i);

    double a[3];
    double b[3];
    double point[3];
    double rp, dp, phi, ra, rb, da, db;
    double f, g, dfdra, dfdrb, dgdra, dgdrb;

    for (int n=0; n<X.cols; n++) {

        for (int i=0; i<3; i++)
            point[i] = X(i,n);

        a[0] = c[0] + (point[0]-c[0])*(zW_-c[2])/(point[2]-c[2]);
        a[1] = c[1] + (point[1]-c[1])*(zW_-c[2])/(point[2]-c[2]);
        a[2] = zW_;
        b[0] = c[0] + (point[0]-c[0])*(t_+zW_-c[2])/(point[2]-c[2]);
        b[1] = c[1] + (point[1]-c[1])*(t_+zW_-c[2])/(point[2]-c[2]);
        b[2] = t_+zW_;

        rp = sqrt( pow(point[0]-c[0],2) + pow(point[1]-c[1],2) );
        dp = point[2]-b[2];
        phi = atan2(point[1]-c[1],point[0]-c[0]);

        ra = sqrt( pow(a[0]-c[0],2) + pow(a[1]-c[1],2) );
        rb = sqrt( pow(b[0]-c[0],2) + pow(b[1]-c[1],2) );
        da = a[2]-c[2];
        db = b[2]-a[2];

        // Newton Raphson loop to solve for Snell's law
        double tol = IMG_REFRAC_TOL;
        double ra1, rb1, res;
        ra1 = ra; rb1 = rb;
        VLOG(4)<<"img_refrac() Newton Raphson solver progress:";
        int i;
        for (i=0; i<MAX_NR_ITERS; i++) {

            f = ( ra/sqrt(pow(ra,2)+pow(da,2)) ) - ( (n2_/n1_)*(rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) );
            g = ( (rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) ) - ( (n3_/n2_)*(rp-rb)/sqrt(pow(rp-rb,2)+pow(dp,2)) );

            dfdra = ( (1.0)/sqrt(pow(ra,2)+pow(da,2)) )
                - ( pow(ra,2)/pow(pow(ra,2)+pow(da,2),1.5) )
                + ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) )
                - ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) );

            dfdrb = ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) );

            dgdra = ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) );

            dgdrb = ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) )
                + ( (n3_/n2_)/sqrt(pow(rb-rp,2)+pow(dp,2)) )
                - ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
                - ( (n3_/n2_)*(rb-rp)*(2*rb-2*rp)/(2*pow(pow(rb-rp,2)+pow(dp,2),1.5)) );

            ra = ra - ( (f*dgdrb - g*dfdrb)/(dfdra*dgdrb - dfdrb*dgdra) );
            rb = rb - ( (g*dfdra - f*dgdra)/(dfdra*dgdrb - dfdrb*dgdra) );

            res = abs(ra1-ra)+abs(rb1-rb);
            VLOG(4)<<(i+1)<<": "<<res;
            ra1 = ra; rb1 = rb;
            if (res < tol) {
                VLOG(4)<<"Tolerance reached. Terminating solver...";
                break;
            }

        }

        VLOG(3)<<"# NR iterations to convergence: "<<(i+1);
        if (i+1==MAX_NR_ITERS)
            LOG(WARNING)<<"Maximum iterations were reached for the NR solver in img_refrac()";

        a[0] = ra*cos(phi) + c[0];
        a[1] = ra*sin(phi) + c[1];

        X_out(0,n) = a[0];
        X_out(1,n) = a[1];
        X_out(2,n) = a[2];
        X_out(3,n) = 1.0;

    }

    // for (int n=0; n<X.cols; n++) {

    //     double a[3];
    //     double b[3];
    //     double point[3];
    //     for (int i=0; i<3; i++)
    //         point[i] = X(i,n);

    //     a[0] = c[0] + (point[0]-c[0])*(zW_-c[2])/(point[2]-c[2]);
    //     a[1] = c[1] + (point[1]-c[1])*(zW_-c[2])/(point[2]-c[2]);
    //     a[2] = zW_;
    //     b[0] = c[0] + (point[0]-c[0])*(t_+zW_-c[2])/(point[2]-c[2]);
    //     b[1] = c[1] + (point[1]-c[1])*(t_+zW_-c[2])/(point[2]-c[2]);
    //     b[2] = t_+zW_;

    //     double rp = sqrt( pow(point[0]-c[0],2) + pow(point[1]-c[1],2) );
    //     double dp = point[2]-b[2];
    //     double phi = atan2(point[1]-c[1],point[0]-c[0]);

    //     double ra = sqrt( pow(a[0]-c[0],2) + pow(a[1]-c[1],2) );
    //     double rb = sqrt( pow(b[0]-c[0],2) + pow(b[1]-c[1],2) );
    //     double da = a[2]-c[2];
    //     double db = b[2]-a[2];

    //     double f, g, dfdra, dfdrb, dgdra, dgdrb;

    //     // Newton Raphson loop to solve for Snell's law
    //     double tol=1E-8;

    //     for (int i=0; i<20; i++) {

    //         f = ( ra/sqrt(pow(ra,2)+pow(da,2)) ) - ( (n2_/n1_)*(rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) );
    //         g = ( (rb-ra)/sqrt(pow(rb-ra,2)+pow(db,2)) ) - ( (n3_/n2_)*(rp-rb)/sqrt(pow(rp-rb,2)+pow(dp,2)) );

    //         dfdra = ( (1.0)/sqrt(pow(ra,2)+pow(da,2)) )
    //             - ( pow(ra,2)/pow(pow(ra,2)+pow(da,2),1.5) )
    //             + ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) )
    //             - ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) );

    //         dfdrb = ( (n2_/n1_)*(ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
    //             - ( (n2_/n1_)/sqrt(pow(ra-rb,2)+pow(db,2)) );

    //         dgdra = ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
    //             - ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) );

    //         dgdrb = ( (1.0)/sqrt(pow(ra-rb,2)+pow(db,2)) )
    //             + ( (n3_/n2_)/sqrt(pow(rb-rp,2)+pow(dp,2)) )
    //             - ( (ra-rb)*(2*ra-2*rb)/(2*pow(pow(ra-rb,2)+pow(db,2),1.5)) )
    //             - ( (n3_/n2_)*(rb-rp)*(2*rb-2*rp)/(2*pow(pow(rb-rp,2)+pow(dp,2),1.5)) );

    //         ra = ra - ( (f*dgdrb - g*dfdrb)/(dfdra*dgdrb - dfdrb*dgdra) );
    //         rb = rb - ( (g*dfdra - f*dgdra)/(dfdra*dgdrb - dfdrb*dgdra) );

    //     }

    //     a[0] = ra*cos(phi) + c[0];
    //     a[1] = ra*sin(phi) + c[1];

    //     X_out(0,n) = a[0];
    //     X_out(1,n) = a[1];
    //     X_out(2,n) = a[2];
    //     X_out(3,n) = 1.0;

    // }

}

void saRefocus::dump_stack(string path, double zmin, double zmax, double dz, double thresh, string type) {

    LOG(INFO)<<"SAVING STACK TO "<<path;

    if (*path.rbegin() != '/')
        path += '/';

    if (boost::filesystem::is_directory(path)) {
        LOG(WARNING) << "Directory " << path << " already exists!";
        if (boost::filesystem::is_empty(path))
            LOG(INFO) << "However, it is empty. Will write data in same directory.";
        else {
            path = generate_unique_path(path);
            LOG(INFO) << "Routing output to " << path << " instead.";
            mkdir(path.c_str(), S_IRWXU);
        }
    } else {
        LOG(INFO) << "Creating directory " << path;
        mkdir(path.c_str(), S_IRWXU);
    }

    for (int f=0; f<frames_.size(); f++) {

        stringstream fn;
        fn<<path<<stack_names_[frames_[f]];
        mkdir(fn.str().c_str(), S_IRWXU);

        LOG(INFO) << "Saving frame " << frames_.at(f) << " (" << fn.str() << ")...";

        vector<Mat> stack;
#ifndef WITHOUT_CUDA
        if (GPU_FLAG) {
            uploadSingleToGPU(f);            
            for (double z=zmin; z<=zmax; z+=dz) {
                Mat img = refocus(z, 0, 0, 0, thresh, 0);
                stack.push_back(img);
            }
        }
#endif
        if (!GPU_FLAG) {
            for (double z=zmin; z<=zmax; z+=dz) {
                Mat img = refocus(z, 0, 0, 0, thresh, frames_[f]);
                stack.push_back(img);
            }
        }


        imageIO io(fn.str());
        io<<stack; stack.clear();

    }

    LOG(INFO)<<"SAVING COMPLETE!"<<endl;

}

void saRefocus::write_piv_settings(string path, double zmin, double zmax, double dz, double thresh) {

    LOG(INFO)<<"SAVING PIV SETTINGS FILE IN "<<path;

    string out_file = path + "piv_config.yaml";
    ofstream file(out_file.c_str());

    YAML::Emitter rec_out;

    rec_out << YAML::BeginMap;
    rec_out << YAML::Comment("settings from SA reconstruction");

    rec_out << YAML::Key << "data_path";
    rec_out << YAML::Value << path;

    rec_out << YAML::Key << "piv_save_path";
    rec_out << YAML::Value << path + "piv_results/";

    rec_out << YAML::Key << "pix_per_mm";
    rec_out << YAML::Value << scale_;

    rec_out << YAML::EndMap;

    file << rec_out.c_str() << "\n\n";

    YAML::Emitter piv_out;

    piv_out << YAML::BeginMap;
    piv_out << YAML::Comment("default PIV settings (change as needed)");

    piv_out << YAML::Key << "dt";
    piv_out << YAML::Value << 1.0;

    piv_out << YAML::Key << "passes";
    piv_out << YAML::Value << 3;

    piv_out << YAML::Key << "windows";
    piv_out << YAML::Value << YAML::BeginSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 64 << 64 << 64 << YAML::EndSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 48 << 48 << 48 << YAML::EndSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 32 << 32 << 32 << YAML::EndSeq;
    piv_out << YAML::EndSeq;

    piv_out << YAML::Key << "overlap";
    piv_out << YAML::Value << YAML::BeginSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 50 << 50 << 50 << YAML::EndSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 50 << 50 << 50 << YAML::EndSeq;
    piv_out << YAML::Flow << YAML::BeginSeq << 50 << 50 << 50 << YAML::EndSeq;
    piv_out << YAML::EndSeq;

    piv_out << YAML::EndMap;

    file << piv_out.c_str() << "\n";
    file.close();

    LOG(INFO)<<"SAVING COMPLETE!"<<endl;

}

void saRefocus::dump_stack_piv(string path, double zmin, double zmax, double dz, double thresh, string type, int f, vector<Mat> &returnStack, double &time) {

    LOG(INFO)<<"SAVING STACK TO "<<path;

    stringstream fn;
    fn<<path<<f;
    mkdir(fn.str().c_str(), S_IRWXU);

    fn<<"/refocused";
    mkdir(fn.str().c_str(), S_IRWXU);

    LOG(INFO)<<"Saving frame "<<f<<"...";

    boost::chrono::system_clock::time_point t1 = boost::chrono::system_clock::now();

    vector<Mat> stack;
    for (double z=zmin; z<=zmax; z+=dz) {
        Mat img = refocus(z, 0, 0, 0, thresh, 0);
        stack.push_back(img);
    }

    boost::chrono::duration<double> t2 = boost::chrono::system_clock::now() - t1;
    time = t2.count();
    VLOG(1)<<"Time taken for reconstruction: "<<t2;

    imageIO io(fn.str());
    io<<stack;

    returnStack = stack;

    stack.clear();

    LOG(INFO)<<"done!"<<endl;

    LOG(INFO)<<"SAVING COMPLETE!"<<endl;

}

void saRefocus::liveViewWindow(Mat img) {

    char title[250];
    if (STDEV_THRESH) {
        sprintf(title, "delta = %f, exp = %f, T = %f (x StDev), frame = %d, xs = %f, ys = %f, zs = %f \nrx = %f, ry = %f, rz = %f, crx = %f, cry = %f, crz = %f", delta_, mult_exp_, thresh_, active_frame_, xs_, ys_, z_, rx_, ry_, rz_, crx_, cry_, crz_);
    } else {
        sprintf(title, "delta = %f, exp = %f, T = %f, frame = %d, xs = %f, ys = %f, zs = %f \nrx = %f, ry = %f, rz = %f, crx = %f, cry = %f, crz = %f", delta_, mult_exp_, thresh_*255.0, active_frame_, xs_, ys_, z_, rx_, ry_, rz_, crx_, cry_, crz_);
    }

    imshow("Live View", img);
    displayOverlay("Live View", title);

}

// Function to reconstruct a volume and then compare to reference stack and calculate Q without
// dumping stack
void saRefocus::calculateQ(double zmin, double zmax, double dz, double thresh, int frame, string refPath) {

    // get refStack
    string stackPath = refPath + "stack/";
    vector<string> img_names;
    listDir(stackPath, img_names);
    sort(img_names.begin(), img_names.end());

    vector<Mat> refStack;
    LOG(INFO)<<"Reading reference stack from "<<stackPath;
    readImgStack(img_names, refStack);
    LOG(INFO)<<"done.";

    vector<Mat> stack;
    LOG(INFO)<<"Reconstructing volume...";
    return_stack(zmin, zmax, dz, thresh, frame, stack);

    LOG(INFO)<<"Calculating Q...";
    double q = getQ(stack, refStack);

    LOG(INFO)<<q;

}

void saRefocus::return_stack(double zmin, double zmax, double dz, double thresh, int frame, vector<Mat> &stack) {

    boost::chrono::system_clock::time_point t1 = boost::chrono::system_clock::now();

    for (double z=zmin; z<=zmax+(dz*0.5); z+=dz) {
        Mat img = refocus(z, 0, 0, 0, thresh, frame);
        stack.push_back(img);
    }

    boost::chrono::duration<double> t2 = boost::chrono::system_clock::now() - t1;
    VLOG(1)<<"Time taken for reconstruction: "<<t2;

}

void saRefocus::return_stack(double zmin, double zmax, double dz, double thresh, int frame, vector<Mat> &stack, double &time) {

    boost::chrono::system_clock::time_point t1 = boost::chrono::system_clock::now();

    for (double z=zmin; z<=zmax; z+=dz) {
        Mat img = refocus(z, 0, 0, 0, thresh, frame);
        stack.push_back(img);
    }

    boost::chrono::duration<double> t2 = boost::chrono::system_clock::now() - t1;
    time = t2.count();
    VLOG(1)<<"Time taken for reconstruction: "<<time;

}

double saRefocus::getQ(vector<Mat> &stack, vector<Mat> &refStack) {

    double xct=0;
    double xc1=0;
    double xc2=0;

    for (int i=0; i<stack.size(); i++) {

        Mat a; multiply(stack[i], refStack[i], a);
        xct += double(sum(a)[0]);

        Mat b; pow(stack[i], 2, b);
        xc1 += double(sum(b)[0]);

        Mat c; pow(refStack[i], 2, c);
        xc2 += double(sum(c)[0]);

    }

    double q = xct/sqrt(xc1*xc2);

    return(q);

}

// ---Preprocessing / Image Processing related functions--- //

void saRefocus::threshold_image(Mat &img) {

    // add code here and later replace for CPU functions

}

void saRefocus::saturate_images() {

    LOG(INFO) << "Saturating images...";

    for (int i=0; i<imgs.size(); i++) {
        for (int j=0; j<imgs[i].size(); j++) {
            saturate_image(imgs[i][j]);
        }
    }

}

void saRefocus::saturate_image(Mat &img) {

    Scalar max_val(1);
    Mat le_mask, gt_mask;
    compare(img, max_val, le_mask, CMP_LE);
    compare(img, max_val, gt_mask, CMP_GT);
    le_mask.convertTo(le_mask, CV_32F);
    gt_mask.convertTo(gt_mask, CV_32F);
    le_mask /= 255.0;
    gt_mask /= 255.0;
    
    multiply(img, le_mask, img);
    add(img, gt_mask, img);

}

void saRefocus::weight_images() {

    LOG(INFO) << "Weighting images i.e. setting pixels < mean value to -1...";

    for (int i=0; i<imgs.size(); i++) {
        for (int j=0; j<imgs[i].size(); j++) {
            weight_image(imgs[i][j]);
        }
    }

}

void saRefocus::weight_image(Mat &img) {

    Scalar mean_val = mean(img);
    double min_val, max_val;
    minMaxIdx(img, &min_val, &max_val);
    if (max_val > 1.0)
        LOG(WARNING) << "Maximum intensity (" << max_val << ") in image is larger than 1! This means images have not been saturated and final reconstruction will be affected.";

    Mat ge_mask, lt_mask;
    compare(img, mean_val, ge_mask, CMP_GE);
    compare(img, mean_val, lt_mask, CMP_LT);
    ge_mask.convertTo(ge_mask, CV_32F);
    lt_mask.convertTo(lt_mask, CV_32F);
    ge_mask /= 255.0;
    if (weighting_mode_ == 1)
        lt_mask *= -1.0*max_val/255.0;
    else if (weighting_mode_ == 2)
        lt_mask *= -1.0*num_cams_/255.0;
    else
        LOG(FATAL) << "Invalid weighting mode! Only options are 0, 1 and 2.";

    multiply(img, ge_mask, img);
    add(img, lt_mask, img);

}

#ifndef WITHOUT_CUDA
void saRefocus::threshold_image(GpuMat &refocused) {

    if (STDEV_THRESH) {
        Scalar mean, stdev;
        gpu::multiply(refocused, Scalar(255, 255, 255), temp); temp.convertTo(temp2, CV_8UC1);
        gpu::meanStdDev(temp2, mean, stdev);
        VLOG(3)<<"Thresholding at: "<<mean[0]+thresh_*stdev[0];
        gpu::threshold(refocused, refocused, (mean[0]+thresh_*stdev[0])/255, 0, THRESH_TOZERO);
    } else {
        gpu::threshold(refocused, refocused, thresh_, 0, THRESH_TOZERO);
    }

}
#endif

void saRefocus::apply_preprocess(void (*preprocess_func)(Mat, Mat), string path) {

    if(imgs_read_) {

        vector<vector<Mat> > imgs_sub;

        for(int i=0; i<imgs.size(); i++) {
            vector<Mat> preprocessed_imgs_sub;
            for(int j=0; j<imgs[i].size(); j++) {
                Mat im;
                preprocess_func(imgs[i][j], im);
                preprocessed_imgs_sub.push_back(im);
            }
            imgs_sub.push_back(preprocessed_imgs_sub);
        }
        imgs.clear();
        imgs.swap(imgs_sub);

        VLOG(1)<<"done!\n";

    }
    else{
        LOG(INFO)<<"Images must be read before preprocessing!"<<endl;
    }
}

void saRefocus::adaptiveNorm(Mat in, Mat &out, int xf, int yf) {

    // TODO: this will have to change assuming image coming in is a float

    int xs = in.cols/xf;
    int ys = in.rows/yf;

    if (xs*xf != in.cols || ys*yf != in.rows)
        LOG(WARNING)<<"Adaptive normalization divide factor leads to non integer window sizes!"<<endl;

    out.create(in.rows, in.cols, CV_8U);

    for (int i=0; i<xf; i++) {
        for (int j=0; j<yf; j++) {

            Mat submat = in(Rect(i*xs,j*ys,xs,ys)).clone();
            Mat subf; submat.convertTo(subf, CV_32F);
            SparseMat spsubf(subf);

            double min, max;
            minMaxLoc(spsubf, &min, &max, NULL, NULL);
            min--;
            if (min>255.0) min = 0;
            subf -+ min; subf /= max; subf *= 255;
            subf.convertTo(submat, CV_8U);

            submat.copyTo(out(Rect(i*xs,j*ys,xs,ys)));

        }
    }

}

void saRefocus::slidingMinToZero(Mat in, Mat &out, int xf, int yf) {

    int xs = in.cols/xf;
    int ys = in.rows/yf;

    if (xs*xf != in.cols || ys*yf != in.rows)
        LOG(WARNING)<<"Sliding minimum divide factor leads to non integer window sizes!"<<endl;

    out.create(in.rows, in.cols, CV_8U);

    for (int i=0; i<xf; i++) {
        for (int j=0; j<yf; j++) {

            Mat submat = in(Rect(i*xs,j*ys,xs,ys)).clone();
            Mat subf; submat.convertTo(subf, CV_32F);
            SparseMat spsubf(subf);

            double min, max;
            minMaxLoc(spsubf, &min, &max, NULL, NULL);
            min--;
            if (min>255.0) min = 0;
            subf -+ min;
            subf.convertTo(submat, CV_8U);

            submat.copyTo(out(Rect(i*xs,j*ys,xs,ys)));

        }
    }

}

// ---Expert mode functions--- //

void saRefocus::setBenchmarkMode(int flag) {

    LOG(WARNING)<<"Benchmarking mode is ON now! Thresholding might not work...";
    BENCHMARK_MODE = flag;

}

void saRefocus::setIntImgMode(int flag) {

    LOG(WARNING)<<"Integer image mode is ON now! Might break things in random places...";
    INT_IMG_MODE = flag;

}

void saRefocus::setGpuDevice(int id) {

    gpu::setDevice(id);

}

void saRefocus::setGpuMode(int flag) {

    GPU_FLAG = flag;

}

void saRefocus::setSingleCamDebug(int flag) {

    if (!num_cams_)
        LOG(FATAL)<<"No camera views have been added yet! Single camera debugging has no way of knowing how to initalize containers.";

    SINGLE_CAM_DEBUG = 1;

    vector<Mat> empty_stack;
    for (int i=0; i<num_cams_; i++)
        cam_stacks_.push_back(empty_stack);

}

void saRefocus::setStdevThresh(int flag) {

    STDEV_THRESH = 1;

}

void saRefocus::setArrayData(vector<Mat> imgs_sub, vector<Mat> Pmats, vector<Mat> cam_locations) {

    img_size_ = Size(imgs_sub[0].cols, imgs_sub[0].rows);
    updateHinv();

    P_mats_ = Pmats;

    for (int i=0; i<imgs_sub.size(); i++) {

        vector<Mat> sub;

        // Applying a 5x5 1.5x1.5 sigma GaussianBlur to preprocess
        // Mat img;
        // GaussianBlur(imgs_sub[i], img, Size(15,15), 1.5, 1.5);

        sub.push_back(imgs_sub[i]);
        imgs.push_back(sub);

    }

    cam_locations_ = cam_locations;

    /*
    geom[0] = -100.0;
    geom[1] = 1.0; geom[2] = 1.5; geom[3] = 1.33;
    geom[4] = 5.0;
    */

}

void saRefocus::updateHinv() {

    Mat D;
    if (INVERT_Y_FLAG) {
        D = (Mat_<double>(3,3) << scale_, 0, img_size_.width*0.5, 0, -1.0*scale_, img_size_.height*0.5, 0, 0, 1);
    } else {
        D = (Mat_<double>(3,3) << scale_, 0, img_size_.width*0.5, 0, scale_, img_size_.height*0.5, 0, 0, 1);
    }
    Mat hinv = D.inv();
    D_ = D;
    hinv_ = hinv;

}

void saRefocus::addView(Mat img, Mat P, Mat location) {

    img_size_ = Size(img.cols, img.rows);
    updateHinv();

    P_mats_.push_back(P);

    vector<Mat> sub; sub.push_back(img);
    imgs.push_back(sub);

    cam_locations_.push_back(location);

    num_cams_++;
    fact_ = Scalar(1/double(num_cams_));

}

void saRefocus::addViews(vector< vector<Mat> > frames, vector<Mat> Ps, vector<Mat> locations) {

    Mat img = frames[0][0];
    img_size_ = Size(img.cols, img.rows);
    updateHinv();

    P_mats_ = Ps;
    cam_locations_ = locations;

    for (int i=0; i<frames[0].size(); i++) {
        vector<Mat> view;
        for (int j=0; j<frames.size(); j++) {
            view.push_back(frames[j][i]);
        }
        imgs.push_back(view);
    }

    // imgs = frames;

    for (int i=1; i<frames.size(); i++)
        frames_.push_back(i);

    num_cams_ = frames[0].size();
    fact_ = Scalar(1/double(num_cams_));

}

void saRefocus::clearViews() {

    P_mats_.clear();
    imgs.clear();
    cam_locations_.clear();
    num_cams_ = 0;

}

void saRefocus::setF(double f) {

    scale_ = f;

}

void saRefocus::setMult(int flag, double exp) {

    mult_ = flag;
    mult_exp_ = exp;

    nlca_ = 0;
    nlca_fast_ = 0;
    minlos_ = 0;

}

void saRefocus::setNlca(int flag, double delta) {

    if (num_cams_ != 4)
        LOG(FATAL) << "NLCA only supported for 4 cameras!";

    nlca_ = flag;
    delta_ = delta;

    nlca_fast_ = 0;
    mult_ = 0;
    minlos_ = 0;

}

void saRefocus::setNlcaFast(int flag, double delta) {

    if (num_cams_ != 4)
        LOG(FATAL) << "NLCA (fast) only supported for 4 cameras!";

    nlca_fast_ = flag;
    delta_ = delta;

    nlca_ = 0;
    mult_ = 0;
    minlos_ = 0;

}

void saRefocus::setNlcaWindow(int size) {

    if ((img_size_.width % size != 0) && (img_size_.height % size != 0))
        LOG(FATAL) << "Image size in both directions must be divible by NLCA window size!";

    if (size > 32)
        LOG(FATAL) << "Window size greater than 32 not supported yet!";

    nlca_win_ = size;

}

void saRefocus::setHF(int hf) {

    CORNER_FLAG = hf;

}

void saRefocus::setRefractive(int ref, double zW, double n1, double n2, double n3, double t) {

    REF_FLAG = ref;
    geom[0] = zW;
    geom[1] = n1; geom[2] = n2; geom[3] = n3;
    geom[4] = t;

}

string saRefocus::showSettings() {

    stringstream s;
    s<<"--- FLAGS ---"<<endl;
    s<<"GPU:\t\t"<<GPU_FLAG<<endl;
    s<<"Refractive:\t"<<REF_FLAG<<endl;
    if (REF_FLAG) {
        s<<"Wall z: "<<geom[0]<<endl;
        s<<"n1: "<<geom[1]<<endl;
        s<<"n2: "<<geom[2]<<endl;
        s<<"n3: "<<geom[3]<<endl;
        s<<"Wall t: "<<geom[4]<<endl;
    }
    s<<"HF Method:\t"<<CORNER_FLAG<<endl;
    s<<"Multiplicative:\t"<<mult_<<endl;
    if (mult_)
        s<<"Mult. exp.:\t"<<mult_exp_<<endl;
    s<<endl<<"--- Other Parameters ---"<<endl;
    s<<"Num Cams:\t"<<num_cams_<<endl;
    s<<"f:\t\t"<<scale_;

    return(s.str());

}

Mat saRefocus::getP(int cam) {

    return P_mats_[cam];

}

Mat saRefocus::getC(int cam) {

    return cam_locations_[cam];

}

vector< vector<Mat> > saRefocus::getCamStacks() {

    return cam_stacks_;

}

// Python wrapper
BOOST_PYTHON_MODULE(refocusing) {

    using namespace boost::python;

    docstring_options local_docstring_options(true, true, false);

    class_<saRefocus>("saRefocus")
        .def("read_calib_data", &saRefocus::read_calib_data, "@DocString(read_calib_data)")
        .def("read_imgs", &saRefocus::read_imgs, "@DocString(read_imgs)")
        .def("addView", &saRefocus::addView)
        .def("clearViews", &saRefocus::clearViews)
        .def("setF", &saRefocus::setF)
        .def("setMult", &saRefocus::setMult)
        .def("setHF", &saRefocus::setHF)
        .def("setRefractive", &saRefocus::setRefractive)
        .def("showSettings", &saRefocus::showSettings)
#ifndef WITHOUT_CUDA
        .def("initializeGPU", &saRefocus::initializeGPU, "@DocString(initializeGPU)")
#endif
	.def("refocus", &saRefocus::refocus, "@DocString(refocus)")
        .def("project_point", &saRefocus::project_point)
        .def("getP", &saRefocus::getP)
        .def("getC", &saRefocus::getC)
    ;

}

/* LEGACY CODE
void saRefocus::read_calib_data_pin(string path) {

    ifstream file;
    file.open(path.c_str());
    if(file.fail())
        LOG(FATAL)<<"Could not open calibration file! Termintation...";

    LOG(INFO)<<"LOADING PINHOLE CALIBRATION DATA...";

    string time_stamp;
    getline(file, time_stamp);

    double reproj_error1, reproj_error2;
    file>>reproj_error1>>reproj_error2;
    file>>num_cams_;

    Mat_<double> P_u = Mat_<double>::zeros(3,4);
    Mat_<double> P = Mat_<double>::zeros(3,4);
    string cam_name;
    double tmp;

    for (int i=0; i<num_cams_; i++) {

        for (int j=0; j<2; j++) getline(file, cam_name);
        cam_names_.push_back(cam_name);

        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                file>>P_u(j,k);
            }
            file>>P_u(j,3);
        }
        for (int j=0; j<3; j++) {
            for (int k=0; k<3; k++) {
                file>>P(j,k);
            }
            file>>P(j,3);
        }
        //refocusing_params_.P_mats_u.push_back(P_u.clone());
        P_mats_.push_back(P.clone());

    }

    file>>img_size_.width;
    file>>img_size_.height;
    file>>scale_;
    //file>>warp_factor_;

    file.close();

}

void saRefocus::read_kalibr_data(string path) {

    LOG(INFO)<<"Reading calibration (kalibr) data...";

    FileStorage fs(path, FileStorage::READ);
    FileNode fn = fs.root();

    FileNodeIterator fi = fn.begin(), fi_end = fn.end();

    int i=0;
    for (; fi != fi_end; ++fi, i++) {

        FileNode f = *fi;
        string cam_name; f["rostopic"]>>cam_name;
        if (MP4_FLAG)
            cam_names_.push_back(cam_name.substr(1,8) + ".MP4");
        else
            cam_names_.push_back(cam_name.substr(1,4));

        VLOG(1)<<"Camera: "<<cam_names_[i];

        string cam_model; f["camera_model"]>>cam_model;
        string dist_model; f["distortion_model"]>>dist_model;

        if (cam_model.compare("pinhole"))
            LOG(FATAL)<<"Only pinhole camera model is supported as of now!";
        if (dist_model.compare("equidistant"))
            LOG(FATAL)<<"Only equidistant distortion model is supported as of now!";

        // Reading distortion coefficients
        vector<double> dc;
        Mat_<double> dist_coeff = Mat_<double>::zeros(1,4);
        f["distortion_coeffs"] >> dc;
        for (int j=0; j < dc.size(); j++)
            dist_coeff(0,j) = (double)dc[j];

        vector<int> ims;
        f["resolution"] >> ims;
        if (i>0) {
            if (((int)ims[0] != calib_img_size_.width) || ((int)ims[1] != calib_img_size_.height))
                LOG(FATAL)<<"Resolution of all images is not the same!";
        } else {
            calib_img_size_ = Size((int)ims[0], (int)ims[1]);
        }

        // Reading K (camera matrix)
        vector<double> intr;
        f["intrinsics"] >> intr;
        Mat_<double> K_mat = Mat_<double>::zeros(3,3);
        K_mat(0,0) = (double)intr[0]; K_mat(1,1) = (double)intr[1];
        K_mat(0,2) = (double)intr[2]; K_mat(1,2) = (double)intr[3];
        K_mat(2,2) = 1.0;

        // Reading R and t matrices
        Mat_<double> R = Mat_<double>::zeros(3,3);
        Mat_<double> t = Mat_<double>::zeros(3,1);
        FileNode tn = f["T_cn_cnm1"];
        if (tn.empty()) {
            R(0,0) = -1.0; R(1,1) = -1.0; R(2,2) = -1.0;
            t(0,0) = 0.0; t(1,0) = 0.0; t(2,0) = 0.0;
        } else {
            FileNodeIterator fi2 = tn.begin(), fi2_end = tn.end();
            int r = 0;
            for (; fi2 != fi2_end; ++fi2, r++) {
                if (r==3)
                    continue;
                FileNode f2 = *fi2;
                R(r,0) = (double)f2[0]; R(r,1) = (double)f2[1]; R(r,2) = (double)f2[2];
                t(r,0) = (double)f2[3]*-1000.0; // converting from [m] to [mm]
            }
        }

        // Converting R and t matrices to be relative to world coordinates
        if (i>0) {
            Mat R3 = R.clone()*R_mats_[i-1].clone();
            Mat t3 = R.clone()*t_vecs_[i-1].clone() + t.clone();
            R = R3.clone(); t = t3.clone();
        }

        Mat Rt = build_Rt(R, t);
        Mat P = K_mat*Rt;

        VLOG(2)<<K_mat;
        VLOG(2)<<Rt;
        VLOG(3)<<P;

        R_mats_.push_back(R);
        t_vecs_.push_back(t);
        dist_coeffs_.push_back(dist_coeff);
        K_mats_.push_back(K_mat);
        P_mats_.push_back(P);

    }

    scale_ = 30; // TODO: fix this!!
    num_cams_ = i;
    REF_FLAG = 0;

    // Averaging P matrices for perspective shift fix
    P_mat_avg_ = P_mats_[0].clone()/num_cams_;
    for (int i=1; i<P_mats_.size(); i++)
        P_mat_avg_ += P_mats_[i].clone()/num_cams_;

}

void saRefocus::read_imgs_mp4(string path) {

    for (int i=0; i<cam_names_.size(); i++) {

        string file_path = path+cam_names_[i];
        mp4Reader mf(file_path);

        int total_frames = mf.num_frames();
        VLOG(1)<<"Total frames: "<<total_frames;

        Mat frame, frame2, frame3;

        vector<Mat> refocusing_imgs_sub;
        for (int j=0; j<frames_.size(); j++) {

            frame = mf.get_frame(frames_[j] + shifts_[i]);

            if (RESIZE_IMAGES) {
                resize(frame, frame2, Size(int(frame.cols*rf_), int(frame.rows*rf_)));
            } else {
                frame2 = frame.clone();
            }

            if (UNDISTORT_IMAGES) {
                fisheye::undistortImage(frame2, frame3, K_mats_[i], dist_coeffs_[i], K_mats_[i]);
            } else {
                frame3 = frame2.clone();
            }

            refocusing_imgs_sub.push_back(frame3.clone()); // store frame

            if (i==0 && j==0) {
                img_size_ = Size(refocusing_imgs_sub[0].cols, refocusing_imgs_sub[0].rows);
            } else {
                if (refocusing_imgs_sub[j].cols != img_size_.width || refocusing_imgs_sub[j].rows != img_size_.height)
                    LOG(FATAL)<<"Size of images to refocus is not the same!";
            }

        }

        imgs.push_back(refocusing_imgs_sub);

    }

    // img_size_ = Size(imgs[0][0].cols, imgs[0][0].rows);
    if (img_size_.width != calib_img_size_.width || img_size_.height != calib_img_size_.height)
        LOG(FATAL)<<"Resolution of images used for calibration and size of images to refocus is not the same!";

    initializeRefocus();

    LOG(INFO)<<"DONE READING IMAGES!";

}
*/
