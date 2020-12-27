#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>

#include <Eigen/Core>
#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>

#include <apriltags/TagDetector.h>
#include <apriltags/Tag36h11.h>

#include "templateUtillity.hpp"
#include "EuRoC_dictionary.h"

using namespace std;
using namespace cv;

void test_elimination() {}

int main() {
  vector<string> file_names;
  glob("../data/*.png", file_names);

  // construct tag detector
  auto tagDetector_ptr =
      std::make_unique<AprilTags::TagDetector>(AprilTags::tagCodes36h11, 2);

  // colors for debug
  vector<Scalar> colors = {Scalar(0, 0, 255), Scalar(200, 90, 55),
                           Scalar(110, 255, 25), Scalar(20, 190, 155)};

  vector<unordered_map<uint16_t, Point2f>> pts_in_frame;
  pts_in_frame.reserve(file_names.size());

  // load frames and detect tags
  for (const auto &name : file_names) {
    cout << name << endl;
    Mat image = imread(name, IMREAD_GRAYSCALE);
    const uint16_t img_w = image.cols;
    const uint16_t img_h = image.rows;
    Mat imageCopy = image.clone();
    cvtColor(imageCopy, imageCopy, cv::COLOR_GRAY2BGR);
    std::vector<AprilTags::TagDetection> detections =
        tagDetector_ptr->extractTags(image);

    unordered_map<uint16_t, Point2f> p2d;
    for (auto it = detections.begin(); it != detections.end(); ++it) {
      for (int i = 0; i < 4; i++) {
        Point2f pt(it->p[i].first, it->p[i].second);
        const uint16_t id = it->id * 4 + i;
        if (it->good) {
          if (inBoard(pt, img_w, img_h, 10)) {
            cv::putText(imageCopy, to_string(it->id * 4 + i), pt, 1, 1.0,
                        colors[i], 1);
            cv::circle(imageCopy, pt, 2, colors[i]);
            // const auto &pt3d = Dict<>::EuRoC[id];
            p2d[id] = Point2f(pt.x, pt.y);
            // cout << id << p2d.at(id) << endl;
          } else {
            cout << "bad" << pt.x << "\t" << pt.y << endl;
          }

        } else {
          cv::putText(imageCopy, to_string(it->id * 4 + i), pt, 1, 1.0,
                      Scalar(0, 255, 0), 1);
          cv::circle(imageCopy, pt, 10, Scalar(0, 255, 0), -1);
        }
      }
    }
    pts_in_frame.push_back(p2d);
    // frame_pt2d.push_back(p2d);
    // frame_pt3d.push_back(p3d);
    // cout << p2d.size() << endl;
    Mat imageCopy2;
    cv::resize(imageCopy, imageCopy2, Size(), 2.0, 2.0);
    imshow("april grid", imageCopy2);
    waitKey(0);
  }

  vector<Point2f> xy0, xy1;
  vector<Point3f> xyz;

  unordered_set<uint16_t> ids;
  constexpr uint16_t p3d_number = 20;
  while (ids.size() < p3d_number) {
    ids.insert(rand() % 144);
  }

  for (const auto &id : ids) {
    xy0.push_back(pts_in_frame[0][id]);
    xy1.push_back(pts_in_frame[1][id]);
    const auto &pt3d = Dict<>::EuRoC[id];
    xyz.emplace_back(pt3d[0], pt3d[1], pt3d[2]);
    cout << id << xyz.back() << endl;
  }
  waitKey(0);

  Mat mask;
  Mat H = findHomography(xy0, xy1, mask);

  Mat im_in = imread(file_names[0], IMREAD_GRAYSCALE);
  Mat im_in1 = imread(file_names[1], IMREAD_GRAYSCALE);
  // Mat im_out, im_out1;

  // warpPerspective(im_in, im_out, H, im_in.size());
  // imshow("out0", im_out);
  // imshow("out1", im_in1);
  // addWeighted(im_out, 0.5, im_in1, 0.5, 0, im_out1);
  // imshow("out2", im_out1);
  // imshow("mask", mask);
  // waitKey(0);

  vector<vector<Point3f>> p3ds = {xyz, xyz};
  vector<vector<Point2f>> p2ds = {xy0, xy1};
  auto flag = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
  Mat K, D;
  vector<Vec3d> rvecs, tvecs;
  auto err = fisheye::calibrate(p3ds, p2ds, im_in.size(), K, D, rvecs, tvecs, flag);
  cout << "err: " << err << endl;
  cout << "K:\n" << K << endl << endl;
  cout << "D:\n" << D << endl << endl;
  cout << rvecs[0] << tvecs[0] << endl;
  const double alpha = *K.ptr<double>(0, 1);

  // validate after calibration
  vector<Point2f> out_p2f;
  fisheye::projectPoints(xyz, out_p2f, rvecs[0], tvecs[0], K, D);
  Mat img0show;

  cvtColor(im_in, img0show, COLOR_GRAY2BGR);
  for (const auto &pt : xy0) {
    cv::circle(img0show, pt, 10, Scalar(0, 255, 0), 2);
  }
  for (const auto &pt : out_p2f) {
    cv::circle(img0show, pt, 10, Scalar(255, 0, 0), 2);
  }
  imshow("project points", img0show);
  waitKey(0);

  return 0;
}