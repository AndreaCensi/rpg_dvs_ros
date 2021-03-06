// This file is part of DVS-ROS - the RPG DVS ROS Package
//
// DVS-ROS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DVS-ROS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with DVS-ROS.  If not, see <http://www.gnu.org/licenses/>.

#include "dvs_calibration/dvs_calibration.h"

namespace dvs_calibration {

DvsCalibration::DvsCalibration()
{
  startCalibrationService = nh.advertiseService("dvs_calibration/start", &DvsCalibration::startCalibrationCallback, this);
  saveCalibrationService = nh.advertiseService("dvs_calibration/save", &DvsCalibration::saveCalibrationCallback, this);
  resetCalibrationService = nh.advertiseService("dvs_calibration/reset", &DvsCalibration::resetCalibrationCallback, this);

  numDetectionsPublisher = nh.advertise<std_msgs::Int32>("dvs_calibration/pattern_detections", 1);
  calibrationOutputPublisher = nh.advertise<std_msgs::String>("dvs_calibration/output", 1);

  calibration_running = false;
  num_detections = 0;

  // load parameters
  loadCalibrationParameters();

  for (int i = 0; i < params.dots_h; i++)
  {
    for (int j = 0; j < params.dots_w; j++)
    {
      world_pattern.push_back(cv::Point3f(i * params.dot_distance , j * params.dot_distance , 0.0));
    }
  }
}

void DvsCalibration::eventsCallback(const dvs_msgs::EventArray::ConstPtr& msg, int camera_id)
{
  if (calibration_running)
    return;

  transition_maps[camera_id].update(msg);
  if (transition_maps[camera_id].max() > params.enough_transitions_threshold) {
    transition_maps[camera_id].find_pattern();
    if (transition_maps[camera_id].has_pattern()) {
      ROS_INFO("Has pattern...");
      add_pattern(camera_id);

      std_msgs::Int32 msg;
      msg.data = num_detections;
      numDetectionsPublisher.publish(msg);

      update_visualization(camera_id);
    }

    transition_maps[camera_id].reset_maps();
  }
  else {
    update_visualization(camera_id);
  }

  // reset if nothing is found after certain amount of time
  if (ros::Time::now() - transition_maps[camera_id].get_last_reset_time() > ros::Duration(params.pattern_search_timeout)) {
    ROS_INFO("calling reset because of time...");
    transition_maps[camera_id].reset_maps();
  }
}

void DvsCalibration::loadCalibrationParameters()
{
  ros::NodeHandle nh_private("~");
  nh_private.param<int>("blinking_time_us", params.blinking_time_us, 1000);
  nh_private.param<int>("blinking_time_tolerance", params.blinking_time_tolerance, 500);
  nh_private.param<int>("enough_transitions_threshold", params.enough_transitions_threshold, 200);
  nh_private.param<int>("minimum_transitions_threshold", params.minimum_transitions_threshold, 10);
  nh_private.param<int>("minimum_led_mass", params.minimum_led_mass, 50);
  nh_private.param<int>("dots_w", params.dots_w, 5);
  nh_private.param<int>("dots_h", params.dots_h, 5);
  nh_private.param<double>("dot_distance", params.dot_distance, 0.05);
  nh_private.param<double>("pattern_search_timeout", params.pattern_search_timeout, 2.0);
}

bool DvsCalibration::resetCalibrationCallback(std_srvs::Empty::Request& request, std_srvs::Empty::Response& response)
{
  ROS_INFO("reset call");
  resetCalibration();
  return true;
}

bool DvsCalibration::startCalibrationCallback(std_srvs::Empty::Request& request, std_srvs::Empty::Response& response)
{
  ROS_INFO("start calib call");
  startCalibration();
  return true;
}

bool DvsCalibration::saveCalibrationCallback(std_srvs::Empty::Request& request, std_srvs::Empty::Response& response)
{
  ROS_INFO("save calib call");
  saveCalibration();
  return true;
}

} // namespace

