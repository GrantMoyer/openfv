// -------------------------------------------------------
// -------------------------------------------------------
// Synthetic Aperture - Particle Tracking Velocimetry Code
// --- Calibration Library ---
// -------------------------------------------------------
// Author: Abhishek Bajpayee
//         Dept. of Mechanical Engineering
//         Massachusetts Institute of Technology
// -------------------------------------------------------
// -------------------------------------------------------

#include "std_include.h"
#include "optimization.h"

class multiCamCalibration {
 
 public:
    ~multiCamCalibration() {
        //
    }

 multiCamCalibration(string path, Size grid_size, double grid_size_phys):
    path_(path), grid_size_(grid_size), grid_size_phys_(grid_size_phys) {}
    
    // Functions to access calibration data
    int num_cams() { return num_cams_; }
    int num_imgs() { return num_imgs_; }
    Size grid_size() { return grid_size_; }
    vector<string> cam_names() { return cam_names_; }
    
    // Functions to run calibration
    void initialize();
    void run();
    
    void read_cam_names();
    void read_calib_imgs();

    void find_corners();
    void initialize_cams();

    void write_BA_data();
    void run_BA();

    void write_calib_results();
    void load_calib_results();

 private:

    void get_grid_size_pix();

    string path_;
    string ba_file_;
    string result_dir_;
    string result_file_;
    
    int num_cams_;
    int num_imgs_;
    int center_cam_id_;
    
    Size grid_size_;
    Size img_size_;
    
    double grid_size_phys_;
    double grid_size_pix_;
    double pix_per_phys_;
    
    vector<string> cam_names_;
    vector< vector<Mat> > calib_imgs_;
    vector< vector<Point3f> > all_pattern_points_;
    vector< vector< vector<Point2f> > > all_corner_points_;
    vector<Mat> cameraMats_;
    vector<Mat> dist_coeffs_;
    vector< vector<Mat> > rvecs_;
    vector< vector<Mat> > tvecs_;
    
    vector<Mat> rVecs_;
    vector<Mat> tVecs_;
    vector<Mat> K_mats_;
    vector<Mat> dist_mats_;

    struct refocusing_data {
        vector<Mat> P_mats_u; // Unaligned P matrices
        vector<Mat> P_mats;
        vector<string> cam_names;
        double scale;
        Size img_size;
    } refocusing_params;

    baProblem ba_problem_;
    double total_reproj_error_;
    double avg_reproj_error_;

    // Option flags
    int solveForDistortion; // TODO: NOT IMPLEMENTED
    int squareGrid; // TODO: NOT IMPLEMENTED
    int saveCornerImgs; // TODO: NOT IMPLEMENTED
    int show_corners_flag;
    int run_calib_flag;
    int results_just_saved_flag;
    int load_results_flag;
    
};
    
    