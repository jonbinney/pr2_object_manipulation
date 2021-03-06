/*
 * Copyright (c) 2009, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pr2_interactive_manipulation/interactive_manipulation_frontend.h"

#include "object_manipulator/tools/camera_configurations.h"

#include <rviz/display_context.h>
#include <rviz/view_controller.h>
#include <rviz/frame_manager.h>
#include <OGRE/OgreMatrix3.h>

#include "ui_interactive_manipulation.h" // generated by uic from interactive_manipulation.ui

namespace pr2_interactive_manipulation {

typedef actionlib::SimpleActionClient<pr2_object_manipulation_msgs::IMGUIAction> Client;

InteractiveManipulationFrontend::InteractiveManipulationFrontend(rviz::DisplayContext* context, QWidget* parent) : 
  QWidget( parent ),
  context_(context),
  root_nh_(""),
  status_label_text_("idle"),
  adv_options_(AdvancedOptionsDialog::getDefaultsMsg()),
  ui_( new Ui::InteractiveManipulation )
{
  ui_->setupUi( this );

  connect( ui_->grasp_button_, SIGNAL( clicked() ), this, SLOT( graspButtonClicked() ));
  connect( ui_->place_button_, SIGNAL( clicked() ), this, SLOT( placeButtonClicked() ));
  connect( ui_->advanced_options_button_, SIGNAL( clicked() ), this, SLOT( advancedOptionsClicked() ));
  connect( ui_->planned_move_button_, SIGNAL( clicked() ), this, SLOT( plannedMoveButtonClicked() ));
  connect( ui_->reset_button_, SIGNAL( clicked() ), this, SLOT( resetButtonClicked() ));
  connect( ui_->model_object_button_, SIGNAL( clicked() ), this, SLOT( modelObjectClicked() ));
  connect( ui_->arm_go_button_, SIGNAL( clicked() ), this, SLOT( armGoButtonClicked() ));
  connect( ui_->rcommand_run_button_, SIGNAL( clicked() ), this, SLOT( rcommandRunButtonClicked() ));
  connect( ui_->rcommander_refresh_button_, SIGNAL( clicked() ), this, SLOT( rcommandRefreshButtonClicked() ));
  connect( ui_->cancel_button_, SIGNAL( clicked() ), this, SLOT( cancelButtonClicked() ));
  connect( ui_->stop_nav_button_, SIGNAL( clicked() ), this, SLOT( stopNavButtonClicked() ));
  connect( ui_->gripper_slider_, SIGNAL( valueChanged( int )), this, SLOT( gripperSliderScrollChanged() ));
  connect( ui_->center_head_button_, SIGNAL( clicked() ), this, SLOT( centerHeadButtonClicked() ));
  connect( ui_->draw_reachable_zones_button_, SIGNAL( clicked() ), this, SLOT( drawReachableZonesButtonClicked() ));

  root_nh_.param<int>("interactive_grasping/interface_number", interface_number_, 0);
  if (!interface_number_) ROS_WARN("No interface number specified for grasping study; using default configuration");
  else ROS_INFO("Using interface number %d for grasping study", interface_number_);
  root_nh_.param<int>("interactive_grasping/task_number", task_number_, 0);
  if (!task_number_) ROS_WARN("No task number specified for grasping study; using default configuration");
  else ROS_INFO("Using task number %d for grasping study", task_number_);

  action_name_ = "imgui_action";  
  action_client_ = new actionlib::SimpleActionClient<pr2_object_manipulation_msgs::IMGUIAction>(action_name_, true);

  status_name_ = "interactive_manipulation_status";
  status_sub_ = root_nh_.subscribe(status_name_, 1, &InteractiveManipulationFrontend::statusCallback, this);

  draw_reachable_zones_pub_ = root_nh_.advertise<std_msgs::Empty>("/draw_reachable_zones/ping", 10);

  rcommander_action_info_name_ = "list_rcommander_actions";
  rcommander_group_name_ = "house_hold";
  rcommander_action_info_client_ = root_nh_.serviceClient<pr2_object_manipulation_msgs::ActionInfo>(rcommander_action_info_name_);

  //scripted_action_name_ = "run_rcommander_action";
  //scripted_action_client_ = new actionlib::SimpleActionClient<RunScriptAction>(scripted_action_name, true);

  ui_->collision_panel_->setEnabled(false);
  ui_->helper_panel_->setEnabled(false);
  ui_->actions_panel_->setEnabled(false);
  ui_->place_button_->setEnabled(false);
  ui_->stop_nav_button_->setEnabled(true);

  ui_->advanced_options_button_->setEnabled(false);
  adv_options_ = AdvancedOptionsDialog::getDefaultsMsg(interface_number_, task_number_);

  switch (interface_number_)
  {
  case 1:
    ui_->collision_box_->setChecked(false);
    ui_->grasp_button_->setEnabled(false);
    ui_->planned_move_button_->setEnabled(false);
    break;
  case 2:
    ui_->collision_box_->setChecked(false);
    ui_->grasp_button_->setEnabled(false);
    ui_->planned_move_button_->setEnabled(true);
    break;
  case 3:
    ui_->grasp_button_->setEnabled(true);
    ui_->planned_move_button_->setEnabled(false);
    break;
  case 4:
    ui_->grasp_button_->setEnabled(true);
    ui_->planned_move_button_->setEnabled(false);
    break;
  case 0:
    ui_->collision_panel_->setEnabled(true);
    ui_->helper_panel_->setEnabled(true);
    ui_->actions_panel_->setEnabled(true);
    ui_->place_button_->setEnabled(true);
    ui_->advanced_options_button_->setEnabled(true);
    break;
  } 
  ui_->grasp_place_panel_->setFocus();
}

InteractiveManipulationFrontend::~InteractiveManipulationFrontend()
{
  delete action_client_;
}

void InteractiveManipulationFrontend::update()
{
  status_label_mutex_.lock();
  QString mystring = QString::fromUtf8( status_label_text_.c_str() );
  status_label_mutex_.unlock();
  ui_->status_label_->setText(mystring);

  ui_->reset_button_->setEnabled(ui_->collision_box_->isChecked());
  ui_->reset_choice_->setEnabled(ui_->collision_box_->isChecked());
}

void InteractiveManipulationFrontend::statusCallback( const std_msgs::StringConstPtr &status)
{
  ROS_DEBUG_STREAM("IM Frontend received stauts: " << status->data);
  boost::mutex::scoped_lock lock(status_label_mutex_);
  status_label_text_ = status->data;
}

void InteractiveManipulationFrontend::feedbackCallback(const pr2_object_manipulation_msgs::IMGUIFeedbackConstPtr &feedback)
{
  //we are now doing this with topic subscription instead
  /*
  ROS_DEBUG_STREAM("IM Frontend received feedback" << feedback->status);
  boost::mutex::scoped_lock lock(status_label_mutex_);
  status_label_text_ = feedback->status;
  */  
}

void InteractiveManipulationFrontend::cancelButtonClicked()
{
  action_client_->cancelAllGoals();
}

void InteractiveManipulationFrontend::stopNavButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.STOP_NAV;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::centerHeadButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.LOOK_AT_TABLE;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::drawReachableZonesButtonClicked()
{
  draw_reachable_zones_pub_.publish(std_msgs::Empty());
}

void InteractiveManipulationFrontend::graspButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.PICKUP;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::placeButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.PLACE;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::plannedMoveButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.PLANNED_MOVE;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::resetButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.RESET;  
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::armGoButtonClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.MOVE_ARM;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::modelObjectClicked()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.MODEL_OBJECT;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

//void InteractiveManipulationFrontend::openDrawerButtonClicked()
//{
//  IMGUIGoal goal;
//  goal.options = getDialogOptions();
//  goal.command.command = goal.command.OPEN_DRAWER;
//  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
//                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
//}

void InteractiveManipulationFrontend::rcommandRunButtonClicked()
{
    //Call RunScriptAction
    pr2_object_manipulation_msgs::IMGUIGoal goal;

    goal.options = getDialogOptions();
    goal.command.command = goal.command.SCRIPTED_ACTION;
    goal.command.script_name = ui_->rcommander_choice_->currentText().toStdString();
    if( goal.command.script_name == "" )
      return;
    goal.command.script_group_name = rcommander_group_name_;
    action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                             boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::rcommandRefreshButtonClicked()
{
    //Call service for list of actions
    pr2_object_manipulation_msgs::ActionInfo srv;
    srv.request.group_name = rcommander_group_name_;
    if (rcommander_action_info_client_.call(srv))
    {
        ui_->rcommander_choice_->clear();
        for (unsigned int i = 0; i < srv.response.services.size(); i++)
        {
          ui_->rcommander_choice_->addItem( QString::fromUtf8( srv.response.services[i].c_str() ));
        }
    }
}

void InteractiveManipulationFrontend::gripperSliderScrollChanged()
{
  pr2_object_manipulation_msgs::IMGUIGoal goal;
  goal.options = getDialogOptions();
  goal.command.command = goal.command.MOVE_GRIPPER;
  action_client_->sendGoal(goal,Client::SimpleDoneCallback(), Client::SimpleActiveCallback(),
                           boost::bind(&InteractiveManipulationFrontend::feedbackCallback, this, _1));
}

void InteractiveManipulationFrontend::advancedOptionsClicked()
{
  AdvancedOptionsDialog *dlg = new AdvancedOptionsDialog(this);
  dlg->setOptionsMsg(adv_options_);
  dlg->show();
}

pr2_object_manipulation_msgs::IMGUIOptions InteractiveManipulationFrontend::getDialogOptions()
{
  pr2_object_manipulation_msgs::IMGUIOptions options;
  options.collision_checked = ui_->collision_box_->isChecked();
  options.grasp_selection = 0;
  options.arm_selection = ui_->arm_choice_->currentIndex();
  options.reset_choice = ui_->reset_choice_->currentIndex();
  options.arm_action_choice = ui_->arm_action_choice_->currentIndex();
  options.arm_planner_choice = ui_->arm_planner_choice_->currentIndex();
  options.adv_options = adv_options_;
  options.gripper_slider_position = ui_->gripper_slider_->value();
  return options;
}

void InteractiveManipulationFrontend::setCamera(std::vector<double> params)
{
  ROS_ERROR( "setCamera is not supported anymore." );
#if 0
  float yaw_correction = 0;
  Ogre::Vector3 pos;
  Ogre::Quaternion orient;

  //correct for yaw rotation
  if (context_->getFrameManager()->getTransform("base_link",
						    ros::Time(),  pos, orient) )
  {
    yaw_correction = orient.getRoll().valueRadians();
  }
  params[1] -= yaw_correction;

  //also need to rotate the focal point
  Ogre::Vector3 focal_point(params[3], params[4], params[5]);
  Ogre::Matrix3 rot;
  rot.FromAxisAngle(Ogre::Vector3(0,1,0), Ogre::Radian(yaw_correction));
  focal_point = rot * focal_point;
  for(int i=0; i<3; i++) params[3+i] = focal_point[i];

  //set the camera params
  std::ostringstream os;
  for(int i=0; i<6; i++) os << params[i] << ' ';
  std::string s(os.str());
  //context_->setTargetFrame("base_link");
  //context_->setCurrentViewControllerType( "Orbit" );
  rviz::ViewController* vc = context_->getCurrentViewController();
  vc->fromString(s);
  context_->queueRender();
#endif
}

void InteractiveManipulationFrontend::cameraLeftButtonClicked()
{
  setCamera( object_manipulator::cameraConfigurations().get_camera_pose("left") );
}

void InteractiveManipulationFrontend::cameraRightButtonClicked()
{
  setCamera( object_manipulator::cameraConfigurations().get_camera_pose("right") );
}

void InteractiveManipulationFrontend::cameraFrontButtonClicked()
{
  setCamera( object_manipulator::cameraConfigurations().get_camera_pose("front") );
}

void InteractiveManipulationFrontend::cameraTopButtonClicked()
{
  setCamera( object_manipulator::cameraConfigurations().get_camera_pose("top") );
}

}
