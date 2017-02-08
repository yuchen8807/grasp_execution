#include <grasp_execution/grasp_execution.h>
#include<grasp_execution/create_gripper_marker.h>



namespace  grasp_execution{

float joints_pre_nominal[] = {-0.12055318838547446, -1.9131810679104309, 1.9643177327972587, -1.6509883639374454, -1.5562078432847617, 3.0825801942323423};

float joints_nominal[] = { -0.10114108404035793, -1.499387382710979, 1.59793845485712, -1.6985646409007025, -1.556784117994499, 3.101942511788552};

float joints_place[] = {-2.422389332448141, -1.5381997267352503, 1.956423282623291, -1.9831450621234339, -1.5961278120623987, -0.08470756212343389};

GraspExecution::GraspExecution(ros::NodeHandle& node, grasp_execution::grasp grasp): spinner(1), group("manipulator"), group2("gripper")
{
  grasp_ = grasp;
  nh_ = node;
  approach_pub_ = nh_.advertise<visualization_msgs::Marker> ("final_grasp", 10);
  grasp_waypoints_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("grasp_waypoints",10);
  waypoints_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("pick_waypoints",10);
  grasp_wayPoints_.clear();
  pick_wayPoints_.clear();


  spinner.start();
  group.setPlannerId ("RRTkConfigDefault");
}

void GraspExecution::publishGripperMarker()
{
  ROS_INFO("Publishing gripper marker");
  imServer.reset(new interactive_markers::InteractiveMarkerServer("gripper", "gripper", false));
  ros::Duration(0.1).sleep();
  imServer->applyChanges();
  visualization_msgs::InteractiveMarker gripperMarker;
  gripperMarker = CreateGripperMarker::makeGripperMaker (grasp_.pose);
  imServer->insert(gripperMarker);
  imServer->applyChanges();


}

visualization_msgs::MarkerArray GraspExecution::createWayPointMarkers(std::vector<geometry_msgs::Pose> waypoints, int type)
{
  visualization_msgs::MarkerArray markerArr;

  if(type == 1)
  {
    for(int i=0;i<waypoints.size ();i++)
    {
      visualization_msgs::Marker marker;
      marker.type = visualization_msgs::Marker::CUBE;
      marker.header.frame_id = "/world";
      marker.header.stamp = ros::Time::now ();
      marker.lifetime = ros::Duration(20.0);
      marker.action = visualization_msgs::Marker::ADD;
      marker.ns = "basic_shape";
      marker.id = i;
      marker.scale.x = marker.scale.y = marker.scale.z = 0.015;
      marker.color.r = 0.0;
      marker.color.g = 0.0;
      marker.color.b = 1.0;
      marker.color.a = 1.0;
      marker.pose.position = waypoints[i].position;
      marker.pose.orientation = waypoints[i].orientation;
      markerArr.markers.push_back (marker);
    }
  }

  else
  {
    for(int i=0;i<waypoints.size ();i++)
    {
      visualization_msgs::Marker marker;
      marker.type = visualization_msgs::Marker::SPHERE;
      marker.header.frame_id = "/world";
      marker.header.stamp = ros::Time::now ();
      marker.lifetime = ros::Duration(25.0);
      marker.action = visualization_msgs::Marker::ADD;
      marker.ns = "basic_shape";
      marker.id = i;
      marker.scale.x = marker.scale.y = marker.scale.z = 0.015;
      marker.color.r = 1.0;
      marker.color.g = 1.0;
      marker.color.b = 0.0;
      marker.color.a = 1.0;
      marker.pose.position = waypoints[i].position;
      marker.pose.orientation = waypoints[i].orientation;
      markerArr.markers.push_back (marker);
    }
  }
  return markerArr;
}

 visualization_msgs::Marker GraspExecution::createApproachMarker(grasp_execution::grasp grasp)
 {
   visualization_msgs::Marker marker;
   marker.type = visualization_msgs::Marker::ARROW;
   marker.header.frame_id = "/world";
   marker.header.stamp = ros::Time::now ();
   marker.lifetime = ros::Duration(35.0);
   marker.action = visualization_msgs::Marker::ADD;
   marker.ns = "basic_shape";
   marker.id = 0;
   marker.scale.x = 0.01;
   marker.scale.y = 0.03;
   marker.scale.z = 0.02;
   marker.color.r =  0.0f;
   marker.color.g =  1.0f;
   marker.color.b =  0.0f;
   marker.color.a =  1.0f;
   geometry_msgs::Point p,q;
   p.x = grasp.pose.position.x;
   p.y = grasp.pose.position.y;
   p.z = grasp.pose.position.z;
   q.x = p.x - 0.12* grasp.approach.x;
   q.y = p.y - 0.12* grasp.approach.y;
   q.z = p.z - 0.12* grasp.approach.z;
   marker.points.push_back (q);
   marker.points.push_back (p);
   return marker;
 }

void GraspExecution::publishApproachMarker()
{
  ROS_INFO("Publishing arrow marker");
  visualization_msgs::Marker marker;
  marker = createApproachMarker(grasp_);
  sleep(1); // TODO: Give time to Rviz to be fully started
  approach_pub_.publish(marker);
}

void GraspExecution::publishGraspWayPointsMarker()
{
  ROS_INFO("Publishing grasp waypoints marker");
  visualization_msgs::MarkerArray markerArray;
  markerArray = createWayPointMarkers(grasp_wayPoints_, 2);
  sleep(1); // TODO: Give time to Rviz to be fully started
  grasp_waypoints_pub_.publish(markerArray);
}

void GraspExecution::publishPickWayPointsMarker()
{
  ROS_INFO("Publishing waypoints marker");
  visualization_msgs::MarkerArray markerArray;
  markerArray = createWayPointMarkers(pick_wayPoints_, 1);
  sleep(1); // TODO: Give time to Rviz to be fully started
  waypoints_pub_.publish(markerArray);
}

bool GraspExecution::executePickPose()
{
  //Moveit stuff
  geometry_msgs::Pose pose = createPickPose();
  group.setPoseTarget (pose);
  ROS_INFO("Reference frame: %s", group.getPlanningFrame().c_str());
  ROS_INFO("Reference frame: %s", group.getEndEffectorLink().c_str());
  moveit::planning_interface::MoveGroup::Plan my_plan;
  bool move_success = group.plan (my_plan);
  group.move ();
  sleep(1.0);

}


geometry_msgs::Pose GraspExecution::createPickPose()
{
  geometry_msgs::PoseStamped poseStamed_curr = group.getCurrentPose();
  geometry_msgs::Pose pose_curr = poseStamed_curr.pose;
  geometry_msgs::Pose pickPose;
  pickPose.orientation = pose_curr.orientation;
  pickPose.position.x = pose_curr.position.x;
  pickPose.position.y = pose_curr.position.y;
  pickPose.position.z = pose_curr.position.z + 0.1;
  return pickPose;
}

bool GraspExecution::executePoseGrasp(grasp_execution::grasp grasp)
{
  ros::AsyncSpinner spinner3(1);
  spinner3.start();
  moveit::planning_interface::MoveGroup group("manipulator");
  ROS_INFO("Executing Grasp pose");
  group.setPoseTarget (grasp.pose);
  ROS_INFO("Reference frame: %s", group.getPlanningFrame().c_str());
  ROS_INFO("Reference frame: %s", group.getEndEffectorLink().c_str());
  moveit::planning_interface::MoveGroup::Plan my_plan;
  bool move_success = group.plan (my_plan);
  group.move ();
  sleep(1.0);
}


bool GraspExecution::generateWayPointsGrasp(grasp_execution::grasp grasp)
{
  int num_of_waypoints = 3;
  float dist[] = {0.12, 0.07, 0.0};
  for(int i=0;i<num_of_waypoints;i++)
  {
    geometry_msgs::Pose way;
    way.position.x = grasp.pose.position.x - dist[i] * grasp.approach.x;
    way.position.y = grasp.pose.position.y - dist[i] * grasp.approach.y;
    way.position.z = grasp.pose.position.z - dist[i] * grasp.approach.z;
    way.orientation = grasp.pose.orientation;
    grasp_wayPoints_.push_back(way);
  }
}


double GraspExecution::executeWayPoints(std::vector<geometry_msgs::Pose> waypoints)
{
  ROS_INFO("Executing cartesian path for approach");
  group.setStartStateToCurrentState ();
  robot_state::RobotState start_state(*group.getCurrentState());
  moveit_msgs::RobotTrajectory trajectory_msg;
  group.setPlanningTime(10.0);
  double fraction = group.computeCartesianPath(waypoints,  0.01,  // eef_step
                                                 0.0,   // jump_threshold
                                                 trajectory_msg, false);
  // The trajectory needs to be modified so it will include velocities as well.
  // First to create a RobotTrajectory object
  robot_trajectory::RobotTrajectory rt(group.getCurrentState()->getRobotModel(), "manipulator");
  // Second get a RobotTrajectory from trajectory
  rt.setRobotTrajectoryMsg(*group.getCurrentState(), trajectory_msg);
  // Thrid create a IterativeParabolicTimeParameterization object
  trajectory_processing::IterativeParabolicTimeParameterization iptp;
  // Fourth compute computeTimeStamps
  bool success = iptp.computeTimeStamps(rt);
  ROS_INFO("Computed time stamp %s",success?"SUCCEDED":"FAILED");
  // Get RobotTrajectory_msg from RobotTrajectory
  rt.getRobotTrajectoryMsg(trajectory_msg);

  // Finally plan and execute the trajectory
  moveit::planning_interface::MoveGroup::Plan plan;
  bool success1 = group.plan(plan);
  plan.trajectory_ = trajectory_msg;
  ROS_INFO("Visualizing plan 4 (cartesian path) (%.2f%% acheived)",fraction * 100.0);
  sleep(2.0);
  group.execute(plan);
  return fraction*100.0;
}

bool GraspExecution::executeJointTarget(float joint_values[])
{
  ros::AsyncSpinner spinner2(1);
  spinner2.start();
  moveit::planning_interface::MoveGroup group("manipulator");
  std::map<std::string, double> joints;
  joints["shoulder_pan_joint"] = joint_values[0];
  joints["shoulder_lift_joint"] =  joint_values[1];
  joints["elbow_joint"] =  joint_values[2];
  joints["wrist_1_joint"] =  joint_values[3];
  joints["wrist_2_joint"] =  joint_values[4];
  joints["wrist_3_joint"] = -joint_values[5];

  group.setJointValueTarget(joints);
  group.move();

}


bool GraspExecution::executeJointTargetNominal()
{
  ROS_INFO("Going to joint nominal position");
  executeJointTarget(joints_nominal);

}

bool GraspExecution::executeJointTargetPreNominal()
{
  ROS_INFO("Going to joint pre nominal position");
  executeJointTarget(joints_pre_nominal);

}

bool GraspExecution::executeJointTargetPlace()
{
  ROS_INFO("Going to place");
  executeJointTarget(joints_place);

}



bool GraspExecution::openGripper()
{

  ROS_INFO("opening gripper");
  group2.setJointValueTarget("robotiq_85_left_knuckle_joint", 0.01);
  group2.move();

}

bool GraspExecution::closeGripper()
{
  ROS_INFO(" Closing gripper");
  group2.setJointValueTarget("robotiq_85_left_knuckle_joint", 0.79);
  group2.move();

}

bool GraspExecution::createExtraWayPoints()
{
  moveit::planning_interface::MoveGroup group("manipulator");
  geometry_msgs::PoseStamped pose_curr = group.getCurrentPose();
  geometry_msgs::Pose bw_pose;
  bw_pose.position.x = pose_curr.pose.position.x + 0.5 * (grasp_wayPoints_[0].position.x - pose_curr.pose.position.x);
  bw_pose.position.y = pose_curr.pose.position.y + 0.5 * (grasp_wayPoints_[0].position.y - pose_curr.pose.position.y);
  bw_pose.position.z = pose_curr.pose.position.z + 0.5 * (grasp_wayPoints_[0].position.z - pose_curr.pose.position.z);
  bw_pose.orientation = grasp_wayPoints_[0].orientation;
  std::vector<geometry_msgs::Pose>::iterator it;
  it = grasp_wayPoints_.begin();
  it = grasp_wayPoints_.insert(it, bw_pose);

}

bool GraspExecution::generatePickWayPoints()
{
  pick_wayPoints_.push_back(grasp_wayPoints_[grasp_wayPoints_.size()-1]);
  //pick 10 cm up and create 5 waypoints
  //float dist_z = 1.0;
  for (float i=pick_wayPoints_[0].position.z+0.03;i<pick_wayPoints_[0].position.z+0.1;i+=0.03)
  {
    geometry_msgs::Pose pickPose;
    pickPose.position.x = pick_wayPoints_[0].position.x;
    pickPose.position.y = pick_wayPoints_[0].position.y;
    pickPose.position.z = i;
    pickPose.orientation = pick_wayPoints_[0].orientation;
    pick_wayPoints_.push_back(pickPose);
  }
}

geometry_msgs::Pose GraspExecution::currentPose()
{
  geometry_msgs::PoseStamped poseStamped_curr = group.getCurrentPose();
  geometry_msgs::Pose pose_curr;;
  pose_curr = poseStamped_curr.pose;
  return pose_curr;
}


bool GraspExecution::goToGrasp()
{
  executeJointTargetPreNominal();
  //executeJointTargetNominal();
  openGripper();
  publishApproachMarker();
  publishGripperMarker ();
  generateWayPointsGrasp(grasp_);
  publishGraspWayPointsMarker();
  double score = executeWayPoints(grasp_wayPoints_);
  if(score<50.0)
  {
    ROS_INFO("Cartesian pose failed. Trying directly the grasp pose");
    executePoseGrasp(grasp_);
  }

  closeGripper();
}

bool GraspExecution::pickUp()
{
  //generatePickWayPoints();
  //publishPickWayPointsMarker();
  ROS_INFO("Picking up");
  //executeWayPoints(pick_wayPoints_);
  executePickPose();


}

bool GraspExecution::place()
{
  executeJointTargetPlace();
  openGripper();
  executeJointTargetPreNominal();
  spinner.stop();
}


}//end namespace
