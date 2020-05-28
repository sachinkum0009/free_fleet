/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <cstdio>

#include "FFPanelConfig.hpp"

namespace free_fleet {

//==============================================================================

void PanelConfig::get_param_if_available(
    const ros::NodeHandle& _node, const std::string& _key,
    std::string& _param_out)
{
  std::string tmp_param;
  if (_node.getParam(_key, tmp_param))
  {
    ROS_INFO("Found %s on the parameter server. Setting %s to %s", 
        _key.c_str(), _key.c_str(), tmp_param.c_str());
    _param_out = tmp_param;
  }
}

//==============================================================================

void PanelConfig::get_param_if_available(
    const ros::NodeHandle& _node, const std::string& _key,
    int& _param_out)
{
  int tmp_param;
  if (_node.getParam(_key, tmp_param))
  {
    ROS_INFO("Found %s on the parameter server. Setting %s to %d.",
        _key.c_str(), _key.c_str(), tmp_param);
    _param_out = tmp_param;
  }
}

//==============================================================================

void PanelConfig::print_config() const
{
  printf("PANEL CONFIGURATION\n");
  printf("  state update rate: %d\n", state_update_rate);
  printf("  TOPICS\n");
  printf("    rviz navigation goal topic: %s\n", rviz_nav_goal_topic.c_str());
  printf("    panel robot state topic: %s\n", panel_robot_state_topic.c_str());
  printf("CLIENT-SERVER DDS CONFIGURATION\n");
  printf("  dds domain: %d\n", dds_domain);
  printf("  TOPICS\n");
  printf("    robot state: %s\n", dds_robot_state_topic.c_str());
  printf("    mode request: %s\n", dds_mode_request_topic.c_str());
  printf("    path request: %s\n", dds_path_request_topic.c_str());
  printf("    destination request: %s\n", 
      dds_destination_request_topic.c_str());
}

//==============================================================================

ServerConfig PanelConfig::get_server_config() const
{
  ServerConfig server_config = {
    dds_domain,
    dds_robot_state_topic,
    dds_mode_request_topic,
    dds_path_request_topic,
    dds_destination_request_topic
  };
  return server_config;
}

//==============================================================================


  int state_update_rate = 10;
  std::string rviz_nav_goal_topic = "/move_base_simple/goal";
  std::string panel_robot_state_topic = "/ff_panel/robot_state";

  int dds_domain = 42;
  std::string dds_robot_state_topic = "robot_state";
  std::string dds_mode_request_topic = "mode_request";
  std::string dds_path_request_topic = "path_request";
  std::string dds_destination_request_topic = "destination_request";

PanelConfig PanelConfig::make()
{
  PanelConfig config;
  ros::NodeHandle node_private_ns("~");
  config.get_param_if_available(
      node_private_ns, "state_update_rate", config.state_update_rate);
  config.get_param_if_available(
      node_private_ns, "state_update_rate", config.rviz_nav_goal_topic);
  config.get_param_if_available(
      node_private_ns, "state_update_rate", config.panel_robot_state_topic);
  config.get_param_if_available(
      node_private_ns, "dds_domain", config.dds_domain);
  config.get_param_if_available(
      node_private_ns, "dds_robot_state_topic", config.dds_robot_state_topic);
  config.get_param_if_available(
      node_private_ns, "dds_mode_request_topic", config.dds_mode_request_topic);
  config.get_param_if_available(
      node_private_ns, "dds_path_request_topic", config.dds_path_request_topic);
  config.get_param_if_available(
      node_private_ns, "dds_destination_request_topic", 
      config.dds_destination_request_topic);
  return config;
}

//==============================================================================

} // namespace free_fleet
