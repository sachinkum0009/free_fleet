/*
 * Copyright (C) 2020 Open Source Robotics Foundation
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

#include <string>

#include <QSizePolicy>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <free_fleet/ServerConfig.hpp>
#include <free_fleet/messages/Location.hpp>
#include <free_fleet/messages/RobotMode.hpp>
#include <free_fleet/messages/RobotState.hpp>
#include <free_fleet/messages/DestinationRequest.hpp>

#include <visualization_msgs/Marker.h>

#include "FFPanel.hpp"
#include "FFPanelConfig.hpp"
#include "utilities.hpp"

namespace free_fleet {

//==============================================================================

FFPanel::FFPanel(QWidget* parent)
: rviz::Panel(parent)
{
  create_layout();
  create_connections();

  auto panel_config = PanelConfig::make();

  _fleet_name->setText(QString::fromStdString(panel_config.fleet_name));
  
  _free_fleet_server = Server::make(panel_config.get_server_config());
  if (!_free_fleet_server)
  {
    _debug_label->setText("Free Fleet server unable to start...");
    return;
  }

  _rviz_nav_goal_sub = _nh.subscribe(
      panel_config.rviz_nav_goal_topic, 2, 
      &FFPanel::rviz_nav_goal_callback, this);
  _state_array_relay_sub = _nh.subscribe(
      panel_config.panel_state_array_topic, 2, 
      &FFPanel::update_states, this);
  _nav_goal_markers_pub = _nh.advertise<visualization_msgs::MarkerArray>(
      panel_config.navigation_markers_topic, 10);
}

//==============================================================================

void FFPanel::update_robot_name_selector()
{
  _robot_name_selector->blockSignals(true);
  _robot_name_selector->clear();

  std::unique_lock<std::mutex> robot_states_lock(_robot_states_mutex);
  for (auto it : _robot_states)
  {
    _robot_name_selector->addItem(QString(it.first.c_str()));
  }
  _robot_name_selector->model()->sort(0);
  robot_states_lock.unlock();

  _robot_name_selector->blockSignals(false);
}

//==============================================================================

void FFPanel::update_nav_goal()
{
  // clear up the markers
  std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
  auto it = _nav_markers_map.find(_current_nav_markers_owner);
  clear_nav_markers(it->second);

  // set up the new markers
  const std::string selected_robot =
      _robot_name_selector->currentText().toStdString();
  it = _nav_markers_map.find(selected_robot);
  if (it == _nav_markers_map.end())
    return;
  set_nav_markers(it->second);

  // clear up and set up the new list
  set_goal_list(it->second);
}

//==============================================================================

void FFPanel::clear_nav_goal()
{
  const std::string selected_robot =
      _robot_name_selector->currentText().toStdString();

  std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
  auto it = _nav_markers_map.find(selected_robot);
  if (it == _nav_markers_map.end())
    return;

  // clear up previous markers
  clear_nav_markers(it->second);

  // clean up and set up new list
  it->second.arrow_markers.markers.clear();
  it->second.text_markers.markers.clear();
  set_goal_list(it->second);
}

//==============================================================================

void FFPanel::delete_waypoint()
{
  const std::string selected_robot =
      _robot_name_selector->currentText().toStdString();

  std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
  auto it = _nav_markers_map.find(selected_robot);
  if (it == _nav_markers_map.end() || 
      it->second.arrow_markers.markers.empty() ||
      it->second.text_markers.markers.empty())
    return;

  // collect most recent markers and delete them
  NavMarkers delete_markers;
  delete_markers.arrow_markers.markers = 
      {it->second.arrow_markers.markers.back()};
  delete_markers.text_markers.markers = 
      {it->second.text_markers.markers.back()};
  clear_nav_markers(delete_markers);

  // remove most recent markers from memory and update list
  it->second.arrow_markers.markers.pop_back();
  it->second.text_markers.markers.pop_back();
  set_goal_list(it->second);
}

//==============================================================================

void FFPanel::send_nav_goal()
{
  const std::string selected_robot =
      _robot_name_selector->currentText().toStdString();

  messages::PathRequest path_request;
  path_request.fleet_name = _fleet_name->text().toStdString();
  path_request.robot_name = selected_robot;
  path_request.path.clear();
  path_request.task_id = generate_random_task_id(20);

  std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
  const auto it = _nav_markers_map.find(selected_robot);
  for (const auto& m : it->second.arrow_markers.markers)
  {
    auto time_now = ros::Time::now();
    path_request.path.push_back({
      static_cast<int>(time_now.sec),
      time_now.nsec,
      static_cast<float>(m.pose.position.x),
      static_cast<float>(m.pose.position.y),
      static_cast<float>(get_yaw_from_quat(m.pose.orientation)),
      ""
    });
  }
  nav_markers_lock.unlock();

  if (!_free_fleet_server->send_path_request(path_request))
  {
    std::string debug_str = "Failed to send navigation request...";
    _debug_label->setText(QString::fromStdString(debug_str));
    return;
  }
}

//==============================================================================

QGroupBox* FFPanel::create_robot_group_box()
{
  QLabel* fleet_name_label = new QLabel("Fleet:");
  fleet_name_label->setStyleSheet("font: italic;");

  QLabel* robot_name_label = new QLabel("Robot:");
  robot_name_label->setStyleSheet("font: italic;");

  _fleet_name = new QLabel("");

  _robot_name_selector = new QComboBox;

  QGridLayout* layout = new QGridLayout;
  layout->addWidget(fleet_name_label, 0, 0, 1, 1);
  layout->addWidget(_fleet_name, 0, 1, 1, 3);
  layout->addWidget(robot_name_label, 1, 0, 1, 1);
  layout->addWidget(_robot_name_selector, 1, 1, 1, 3);

  QGroupBox* group_box = new QGroupBox("Selection");
  group_box->setLayout(layout);
  return group_box;
}

//==============================================================================

QGroupBox* FFPanel::create_nav_group_box()
{
  _nav_goal_str_list = QStringList();
  _nav_goal_list_model = new QStringListModel;
  _nav_goal_list_view = new QListView;
  _nav_goal_list_view->setModel(_nav_goal_list_model);
  _nav_goal_list_view->setToolTip(QString("x(m), y(m), yaw(rad)"));

  _clear_goal_button = new QPushButton("&Clear Goal");
  _delete_waypoint_button = new QPushButton("&Delete Waypoint");
  _send_goal_button = new QPushButton("&Send Goal");

  QSizePolicy size_policy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  size_policy.setHorizontalStretch(0);
  size_policy.setVerticalStretch(0);
  size_policy.setHeightForWidth(
      _clear_goal_button->sizePolicy().hasHeightForWidth());
  _clear_goal_button->setSizePolicy(size_policy);
  _delete_waypoint_button->setSizePolicy(size_policy);
  _send_goal_button->setSizePolicy(size_policy);

  QGridLayout* layout = new QGridLayout;
  layout->addWidget(_nav_goal_list_view, 0, 0, 6, 3);
  layout->addWidget(_clear_goal_button, 0, 3, 2, 1);
  layout->addWidget(_delete_waypoint_button, 2, 3, 2, 1);
  layout->addWidget(_send_goal_button, 4, 3, 2, 1);

  QGroupBox* group_box = new QGroupBox("Navigation");
  group_box->setLayout(layout);
  return group_box;
}

//==============================================================================

QGroupBox* FFPanel::create_debug_group_box()
{
  _debug_label = new QLabel("Panel started...");

  QHBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(_debug_label);

  QGroupBox* group_box = new QGroupBox("Debug");
  group_box->setLayout(layout);
  return group_box;
}

//==============================================================================

void FFPanel::create_layout()
{
  QGroupBox* robot_gb = create_robot_group_box();
  QGroupBox* navigation_gb = create_nav_group_box();
  QGroupBox* debug_gb = create_debug_group_box();

  QGridLayout* layout = new QGridLayout;
  layout->addWidget(robot_gb, 0, 0, 1, 1);
  layout->addWidget(navigation_gb, 1, 0, 6, 1);
  layout->addWidget(debug_gb, 7, 0, 1, 1);
  setLayout(layout);
  setStyleSheet(
      "QGroupBox {"
      "  font: bold;"
      "  border: 1px solid silver;"
      "  border-radius: 6px;"
      "  margin-top: 6px;"
      "  padding-top: 10px;"
      "}"
      "QGroupBox::title {"
      "  subcontrol-origin: margin;"
      "  left: 7px;"
      "  padding: 0px 5px 0px 5px;"
      "}"
  );
}

//==============================================================================

void FFPanel::create_connections()
{
  connect(this, SIGNAL(configChanged()), this,
      SLOT(update_robot_name_selector()));
  connect(_robot_name_selector, SIGNAL(currentTextChanged(const QString &)),
      this, SLOT(update_nav_goal()));

  connect(_clear_goal_button, &QPushButton::clicked, this,
      &FFPanel::clear_nav_goal);
  connect(_delete_waypoint_button, &QPushButton::clicked, this,
      &FFPanel::delete_waypoint);
  connect(_send_goal_button, &QPushButton::clicked, this,
      &FFPanel::send_nav_goal);
}

//==============================================================================

void FFPanel::rviz_nav_goal_callback(
    const geometry_msgs::PoseStamped::ConstPtr& msg)
{
  const std::string selected_robot =
      _robot_name_selector->currentText().toStdString();

  std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
  auto it = _nav_markers_map.find(selected_robot);
  int marker_id = 0;
  if (it != _nav_markers_map.end())
  {
    marker_id = static_cast<int>(it->second.arrow_markers.markers.size());
  }
  else
  {
    NavMarkers new_nav_markers;
    _nav_markers_map.insert(std::make_pair(selected_robot, new_nav_markers));
    it = _nav_markers_map.find(selected_robot);
    it->second.arrow_markers.markers.clear();
    it->second.text_markers.markers.clear();
  }

  visualization_msgs::Marker arrow_marker;
  arrow_marker.header.stamp = ros::Time::now();
  arrow_marker.header.frame_id = "map";
  arrow_marker.ns = selected_robot + "/arrow";
  arrow_marker.id = marker_id;
  arrow_marker.type = visualization_msgs::Marker::ARROW;
  arrow_marker.action = visualization_msgs::Marker::MODIFY;
  arrow_marker.pose = msg->pose;
  arrow_marker.scale.x = 0.7;
  arrow_marker.scale.y = 0.1;
  arrow_marker.scale.z = 0.1;
  arrow_marker.color.r = 1.0;
  arrow_marker.color.a = 0.6;

  visualization_msgs::Marker text_marker;
  text_marker.header.stamp = ros::Time::now();
  text_marker.header.frame_id = "map";
  text_marker.ns = selected_robot + "/text";
  text_marker.id = marker_id;
  text_marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
  text_marker.action = visualization_msgs::Marker::MODIFY;
  text_marker.pose = msg->pose;
  text_marker.pose.position.x -= 0.2;
  text_marker.scale.z = 0.3;
  text_marker.color.r = 1.0;
  text_marker.color.a = 0.6;
  text_marker.text = std::to_string(marker_id);

  // keep new markers into memory and update list
  it->second.arrow_markers.markers.push_back(arrow_marker);
  it->second.text_markers.markers.push_back(text_marker);
  set_goal_list(it->second);

  // visualize new markers
  MarkerArray new_markers;
  new_markers.markers.push_back(arrow_marker);
  new_markers.markers.push_back(text_marker);
  _nav_goal_markers_pub.publish(new_markers);
}

//==============================================================================

void FFPanel::clear_nav_markers(const NavMarkers& nav_markers) const
{
  MarkerArray delete_markers;
  delete_markers.markers.clear();

  for (const auto m : nav_markers.arrow_markers.markers)
    delete_markers.markers.push_back(m);
  for (const auto m : nav_markers.text_markers.markers)
    delete_markers.markers.push_back(m);
  for (size_t i = 0; i < delete_markers.markers.size(); ++i)
  {
    delete_markers.markers[i].header.stamp = ros::Time::now();
    delete_markers.markers[i].action = visualization_msgs::Marker::DELETE;
  }

  _nav_goal_markers_pub.publish(delete_markers);
}

//==============================================================================

void FFPanel::set_nav_markers(const NavMarkers& nav_markers) const
{
  MarkerArray new_markers;
  new_markers.markers.clear();

  for (const auto m : nav_markers.arrow_markers.markers)
    new_markers.markers.push_back(m);
  for (const auto m : nav_markers.text_markers.markers)
    new_markers.markers.push_back(m);
  for (size_t i = 0; i < new_markers.markers.size(); ++i)
    new_markers.markers[i].header.stamp = ros::Time::now();

  _nav_goal_markers_pub.publish(new_markers);
}

//==============================================================================

void FFPanel::set_goal_list(const NavMarkers& nav_markers)
{
  _nav_goal_str_list.clear();
  for (const auto& m : nav_markers.arrow_markers.markers)
    _nav_goal_str_list.append(marker_to_qstring(m));
  
  _nav_goal_list_model->setStringList(_nav_goal_str_list);
  _nav_goal_list_view->scrollToBottom();
}

//==============================================================================

void FFPanel::update_states(
    const ff_rviz_plugins_msgs::RobotStateArray::ConstPtr& msg)
{
  std::unique_lock<std::mutex> robot_states_lock(_robot_states_mutex);
  bool new_robot_found = false;
  for (const auto& rs : msg->states)
  {
    std::string robot_name = rs.name;
    if (_robot_states.find(robot_name) == _robot_states.end())
    {
      new_robot_found = true;
      std::unique_lock<std::mutex> nav_markers_lock(_nav_markers_mutex);
      NavMarkers new_nav_markers;
      _nav_markers_map.insert(std::make_pair(rs.name, new_nav_markers));
    }
    _robot_states.insert(std::make_pair(rs.name, rs));
  }
  robot_states_lock.unlock();

  if (new_robot_found)
  {
    std::string debug_label_str = "New robot found, refreshing...";
    _debug_label->setText(QString::fromStdString(debug_label_str));
    Q_EMIT configChanged();
  }
  else
  {
    std::string debug_label_str = 
        "Currently handling " 
        + std::to_string(_robot_states.size()) 
        + " robots...";
    _debug_label->setText(QString::fromStdString(debug_label_str));
  }
}

//==============================================================================

QString FFPanel::marker_to_qstring(const Marker& marker)
{
  auto to_string_precision = [](double val, int precision) 
  {
    std::ostringstream out;
    out.precision(precision);
    out << std::fixed << val;
    return out.str();
  };

  std::ostringstream ss;
  ss << "x: " << to_string_precision(marker.pose.position.x, 2)
      << ", y: " << to_string_precision(marker.pose.position.y, 2) 
      << ", yaw: " 
      << to_string_precision(get_yaw_from_quat(marker.pose.orientation), 2);
  return QString::fromStdString(ss.str());
}

//==============================================================================

} // namespace free_fleet

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(free_fleet::FFPanel, rviz::Panel)
