#include <grasp_execution/grasp_execution.h>
#include<grasp_execution/create_gripper_marker.h>

#include <Eigen/Eigen>
#include <eigen_conversions/eigen_msg.h>
#include<tf/transform_listener.h>



#include<moveit/robot_model_loader/robot_model_loader.h>
#include<moveit/robot_model/robot_model.h>
#include<moveit/robot_model/joint_model_group.h>

#include<moveit/robot_state/robot_state.h>

namespace  grasp_execution{

float joints_pre_nominal[] = {-0.12055318838547446, -1.9131810679104309, 1.9643177327972587, -1.6509883639374454, -1.5562078432847617, 3.0825801942323423};

float joints_nominal[] = { -0.10114108404035793, -1.499387382710979, 1.59793845485712, -1.6985646409007025, -1.556784117994499, 3.101942511788552};

float joints_place[] = {-2.422389332448141, -1.5381997267352503, 1.956423282623291, -1.9831450621234339, -1.5961278120623987, -0.08470756212343389};

GraspExecution::GraspExecution(ros::NodeHandle& node, grasp_execution::graspArr grasps):nh_(node), spinner(1),
  group("left_arm"), group2("left_gripper")
{
  grasp_ = grasps.grasps[0];
  nh_ = node;
  gripper_pub_ = nh_.advertise<pr2_controllers_msgs::Pr2GripperCommand>("/l_gripper_controller/command",10);
  frame_id_ = grasps.header.frame_id;
  table_center_ = grasps.table_center;
  planning_scene_interface_.reset(new moveit::planning_interface::PlanningSceneInterface());

  //approach_pub_ = nh_.advertise<visualization_msgs::Marker> ("approach", 10);
  //grasp_waypoints_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("grasp_waypoints",10);
  //waypoints_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("pick_waypoints",10);

 // gripper_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("final_grasp",10);

  grasp_wayPoints_.clear();
  pick_wayPoints_.clear();


  spinner.start();
  group.setPlannerId ("RRTkConfigDefault");
}

//Codes influenced by moveit_visual_tools
bool GraspExecution::publishGripperMarkerMoveit()
{
  std::string ee_name = group.getEndEffector();
  moveit::core::RobotModelConstPtr robot_model = group.getRobotModel();
  moveit::core::RobotStatePtr robot_state_(new moveit::core::RobotState(robot_model));

  //Setting the gripper joint to the openning value of the gripper
  const std::string ee_joint = "robotiq_85_left_knuckle_joint";
  double value = double(grasp_.angle);
  double *valuePtr = &value;
  robot_state_->setJointPositions(ee_joint, valuePtr);
  robot_state_->update();

  //New jointmodel group of the end effector with the openning value
  const moveit::core::JointModelGroup* ee_jmp = robot_model->getJointModelGroup(ee_name);
  std::string ee_group = ee_jmp->getName();
  std::cout<<"EE group: "<<ee_group<<std::endl;

  if(ee_jmp == NULL)
  {
    ROS_ERROR_STREAM("Unable to find joint model group with address"<<ee_jmp);
    return false;
  }

  //maps
  std::map<const robot_model::JointModelGroup *, visualization_msgs::MarkerArray> ee_markers_map_;

  ee_markers_map_[ee_jmp].markers.clear();

  const std::vector<std::string>& ee_link_names = ee_jmp->getLinkModelNames();
  robot_state_->getRobotMarkers(ee_markers_map_[ee_jmp], ee_link_names);
  const std::string& ee_parent_link_name = ee_jmp->getEndEffectorParentGroup().second;

  Eigen::Affine3d tf_root_to_ee = robot_state_->getGlobalLinkTransform(ee_parent_link_name);
  Eigen::Affine3d tf_ee_to_root = tf_root_to_ee.inverse();
  Eigen::Affine3d trans_bw_poses;
  trans_bw_poses = tf_root_to_ee * tf_ee_to_root;


  Eigen::Affine3d grasp_tf;
  tf::poseMsgToEigen(grasp_.pose, grasp_tf);

  for(std::size_t i=0; i<ee_markers_map_[ee_jmp].markers.size();++i)
  {
    ee_markers_map_[ee_jmp].markers[i].header.frame_id = "/world";
    ee_markers_map_[ee_jmp].markers[i].type = visualization_msgs::Marker::MESH_RESOURCE;
    ee_markers_map_[ee_jmp].markers[i].mesh_use_embedded_materials = true;
    ee_markers_map_[ee_jmp].markers[i].id = i+1;

    ee_markers_map_[ee_jmp].markers[i].header.stamp = ros::Time::now();
    ee_markers_map_[ee_jmp].markers[i].ns = "gripper_links";
    ee_markers_map_[ee_jmp].markers[i].lifetime = ros::Duration(40.0);

    if(i==1 || i==2 || i==5 || i==6 || i==9)
    {
      ee_markers_map_[ee_jmp].markers[i].color.r = 0.1;
      ee_markers_map_[ee_jmp].markers[i].color.g = 0.1;
      ee_markers_map_[ee_jmp].markers[i].color.b = 0.1;
    }
    else{
      ee_markers_map_[ee_jmp].markers[i].color.r = 0.5;
      ee_markers_map_[ee_jmp].markers[i].color.g = 0.5;
      ee_markers_map_[ee_jmp].markers[i].color.b = 0.5;
    }


    ee_markers_map_[ee_jmp].markers[i].color.a = 1.0;

    Eigen::Affine3d link_marker;
    tf::poseMsgToEigen(ee_markers_map_[ee_jmp].markers[i].pose, link_marker);

    Eigen::Affine3d tf_link_in_root =  tf_ee_to_root * link_marker;

    geometry_msgs::Pose new_marker_pose;
    tf::poseEigenToMsg( grasp_tf * tf_link_in_root  , new_marker_pose );
    ee_markers_map_[ee_jmp].markers[i].pose = new_marker_pose;
  }

  sleep(1);
  gripper_pub_.publish(ee_markers_map_[ee_jmp]);

}


void GraspExecution::publishGripperMarker()
{
  ROS_INFO("Publishing gripper marker");

  imServer.reset(new interactive_markers::InteractiveMarkerServer("gripper", "gripper", false));
  ros::Duration(0.1).sleep();
  imServer->applyChanges();
  visualization_msgs::InteractiveMarker gripperMarker;

  //There is a distance between the ee_link(our planning frame) and the mesh model of the robotiq_85_base_link center which is no defined
  //in the URDF. So we are translating out gripper marker by 0.41
  Eigen::Affine3d tf_grasp;
  tf::poseMsgToEigen(grasp_.pose, tf_grasp);
  Eigen::Affine3d translation(Eigen::Translation3d(Eigen::Vector3d(0.041,0,0)));
  geometry_msgs::Pose new_pose;
  tf::poseEigenToMsg( tf_grasp  * translation , new_pose);

  gripperMarker = CreateGripperMarker::makeGripperMaker (new_pose);
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
   marker.lifetime = ros::Duration(30.0);
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
  robot_trajectory::RobotTrajectory rt(group.getCurrentState()->getRobotModel(), "left_arm");
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

geometry_msgs::Pose GraspExecution::getCurrentPose()
{
  geometry_msgs::PoseStamped poseStamped_curr = group.getCurrentPose();
  geometry_msgs::Pose pose_curr;
  pose_curr = poseStamped_curr.pose;
  return pose_curr;
}


void sq_create_transform(const geometry_msgs::Pose& pose, Eigen::Affine3f& transform)
{
  transform = Eigen::Affine3f::Identity();
  Eigen::Quaternionf q(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
  q.normalize();
  transform.translation()<<pose.position.x, pose.position.y, pose.position.z;
  transform.rotate(q);
}


void transformFrame(const std::string& input_frame, const std::string& output_frame, const geometry_msgs::Pose &pose_in, geometry_msgs::Pose &pose_out)
{
  tf::TransformListener listener;
  tf::StampedTransform transform;
    try
    {
      listener.waitForTransform(input_frame, output_frame, ros::Time(0), ros::Duration(3.0));
      listener.lookupTransform(input_frame, output_frame, ros::Time(0), transform);
      geometry_msgs::Pose inter_pose;
      inter_pose.position.x = transform.getOrigin().x();
      inter_pose.position.y = transform.getOrigin().y();
      inter_pose.position.z = transform.getOrigin().z();
      inter_pose.orientation.x = transform.getRotation().getX();
      inter_pose.orientation.y = transform.getRotation().getY();
      inter_pose.orientation.z = transform.getRotation().getZ();
      inter_pose.orientation.w = transform.getRotation().getW();
      Eigen::Affine3d transform_in_eigen;
      tf::poseMsgToEigen(inter_pose, transform_in_eigen);
      Eigen::Affine3f pose_in_eigen;
      sq_create_transform(pose_in, pose_in_eigen);
      tf::poseEigenToMsg( transform_in_eigen * pose_in_eigen.cast<double>(), pose_out );
    }
    catch(tf::TransformException ex)
    {
      ROS_ERROR("%s",ex.what());
      ros::Duration(1.0).sleep();
    }
  }

void findApproachPoseFromDir(const geometry_msgs::Pose &pose_in, const geometry_msgs::Vector3 &dir, geometry_msgs::Pose &pose_out)
{
  pose_out.orientation = pose_in.orientation;
  pose_out.position.x = pose_in.position.x + dir.x;
  pose_out.position.y = pose_in.position.y + dir.y;
  pose_out.position.z = pose_in.position.z + dir.z;
}

void GraspExecution::createTableCollisionObject()
{
  //moveit_msgs::CollisionObject collision_object;
  collision_object_.header.frame_id = frame_id_;
  collision_object_.id = "table";

  /* Define a table to add to the world. */
  shape_msgs::SolidPrimitive primitive;
  primitive.type = primitive.BOX;
  primitive.dimensions.resize(3);
  primitive.dimensions[0] = 1.0;
  primitive.dimensions[1] = 1.5;
  primitive.dimensions[2] = 0.005;

  /* A pose for the box (specified relative to frame_id) */
  geometry_msgs::Pose box_pose;
  box_pose.orientation.w = 1.0;
  box_pose.position.x =  table_center_.x;
  box_pose.position.y = table_center_.y;
  box_pose.position.z =  table_center_.z;

  collision_object_.primitives.push_back(primitive);
  collision_object_.primitive_poses.push_back(box_pose);
  collision_object_.operation = collision_object_.ADD;

  std::vector<moveit_msgs::CollisionObject> collision_objects;
  collision_objects.push_back(collision_object_);
  ROS_INFO("Add table into the world");
  planning_scene_interface_->addCollisionObjects(collision_objects);
  sleep(1.0);
}

void GraspExecution::removeTableCollisionObject()
{
  ROS_INFO("Removing the object from the world");
  std::vector<std::string> object_ids;
  object_ids.push_back(collision_object_.id);
  planning_scene_interface_->removeCollisionObjects(object_ids);
  sleep(1.0);
}

void GraspExecution::openPR2Gripper()
{
  pr2_controllers_msgs::Pr2GripperCommand command;
  command.position = 0.08;
  command.max_effort = 50.0;
  gripper_pub_.publish(command);
  ROS_INFO("Openning PR2 gripper");
}

void GraspExecution::closePR2Gripper()
{
  pr2_controllers_msgs::Pr2GripperCommand command;
  command.position = 0.0;
  command.max_effort = 50.0;
  gripper_pub_.publish(command);
  ROS_INFO("Closing PR2 gripper");
}

void GraspExecution::closePR2GripperByValue(double angle)
{
  pr2_controllers_msgs::Pr2GripperCommand command;
  command.position = angle/10.0;
  command.max_effort = 50.0;
  gripper_pub_.publish(command);
  std::cout<<"Angle is: "<<angle<<std::endl;
  ROS_INFO("Closing PR2 gripper by value");
}

void GraspExecution::moveToDrop()
{
  ROS_INFO("Going to DROP position");
  ros::AsyncSpinner spinner2(1);
  spinner2.start();
  moveit::planning_interface::MoveGroup group("left_arm");
  std::map<std::string, double> joints2;
  joints2["l_shoulder_pan_joint"] = 1.362511126818978;
  joints2["l_shoulder_lift_joint"] =  -0.34946162734450237;
  joints2["l_upper_arm_roll_joint"] = 1.4417106915622417;
  joints2["l_elbow_flex_joint"] =  -0.802871572171303;
  joints2["l_forearm_roll_joint"] =  1.4538708363284372;
  joints2["l_wrist_flex_joint"] =  -1.9956640712296867;
  joints2["l_wrist_roll_joint"] =  -34.02836370324722;
  group.setJointValueTarget(joints2);
  group.move();
}

void GraspExecution::moveToHome()
{
  ROS_INFO("Going to HOME position");
  ros::AsyncSpinner spinner2(1);
  spinner2.start();
  moveit::planning_interface::MoveGroup group("left_arm");
  std::map<std::string, double> joints;
  joints["l_shoulder_pan_joint"] = 0.965007062862802;
  joints["l_shoulder_lift_joint"] =  0.37371566744795714;
  joints["l_upper_arm_roll_joint"] = -0.01618789723951597;
  joints["l_elbow_flex_joint"] = -1.7464098397213954;
  joints["l_forearm_roll_joint"] =   2.1857378980573445;
  joints["l_wrist_flex_joint"] =  -1.6045068808841734;
  joints["l_wrist_roll_joint"] = -2.9733118297097265;
  group.setJointValueTarget(joints);
  group.move();
}

void GraspExecution::moveToApproach()
{
  geometry_msgs::Pose approach_pose;
  geometry_msgs::Pose trans_pose;

  ros::AsyncSpinner spinner2(1);
  spinner2.start();
  moveit::planning_interface::MoveGroup group("left_arm");
  std::cout<<"frame_id is: "<<frame_id_<<std::endl;
  std::cout<<"plannin_frame is: "<<group.getPlanningFrame()<<std::endl;
  std::cout<<"grasp pose: "<<grasp_.pose.position.x<<" "<<grasp_.pose.position.y<<" "<<grasp_.pose.position.z<<std::endl;
  findApproachPoseFromDir(grasp_.pose, grasp_.approach, approach_pose);
  transformFrame(group.getPlanningFrame(), frame_id_ ,approach_pose, trans_pose);
  std::cout<<"trans pose: "<<trans_pose.position.x<<" "<<trans_pose.position.y<<" "<<trans_pose.position.z<<std::endl;


  group.setPlannerId("RRTkConfigDefault");
  group.setStartStateToCurrentState ();
  group.setPoseTarget(trans_pose);
  moveit::planning_interface::MoveGroup::Plan my_plan;
  bool success = group.plan(my_plan);
  group.move();
  ROS_INFO("Reference frame: %s", group.getPlanningFrame().c_str());
  ROS_INFO("Reference frame: %s", group.getEndEffectorLink().c_str());
}


void GraspExecution::generateGraspWayPoints(std::vector<geometry_msgs::Pose> &way)
{
  geometry_msgs::Pose grasp_trans_pose;
  transformFrame(group.getPlanningFrame(), frame_id_ ,grasp_.pose, grasp_trans_pose);
  geometry_msgs::Pose approach_trans_pose;
  geometry_msgs::Pose approach_pose;
  findApproachPoseFromDir(grasp_.pose, grasp_.approach, approach_pose);
  transformFrame(group.getPlanningFrame(), frame_id_ ,grasp_.pose, approach_trans_pose);
  way.push_back(approach_trans_pose);
  geometry_msgs::Pose mid_pose;
  mid_pose.orientation = approach_trans_pose.orientation;
  mid_pose.position.x = (approach_trans_pose.position.x + grasp_trans_pose.position.x)/2.0;
  mid_pose.position.y = (approach_trans_pose.position.y + grasp_trans_pose.position.y)/2.0;
  mid_pose.position.z = (approach_trans_pose.position.z + grasp_trans_pose.position.z)/2.0;
  way.push_back(mid_pose);
  way.push_back(grasp_trans_pose);
}

void GraspExecution::moveToRetreat()
{
  /*ros::AsyncSpinner spinner2(1);
  spinner2.start();
  moveit::planning_interface::MoveGroup group("left_arm");
  group.setPlannerId("RRTkConfigDefault");
  group.setStartStateToCurrentState ();
  geometry_msgs::Pose pose_now = getCurrentPose();
  Eigen::Affine3d pose_now_eigen;
  tf::poseMsgToEigen(pose_now, pose_now_eigen);
  //Eigen::Affine3d retreating = pose_now_eigen.translate(-0.13*Eigen::Vector3d::UnitX());
  ROS_INFO("Retreating");
  group.setPoseTarget(retreating);
  moveit::planning_interface::MoveGroup::Plan my_plan;
  bool success = group.plan(my_plan);
  group.move();*/
  std::vector<geometry_msgs::Pose> waypoints;
  geometry_msgs::Pose curr_pose = getCurrentPose();
  waypoints.push_back(curr_pose);
  geometry_msgs::Pose new_pose = curr_pose;
  new_pose.position.z = curr_pose.position.z + 0.1;
  waypoints.push_back(new_pose); 
  double val = executeWayPoints(waypoints);
}

bool GraspExecution::goToGrasp()
{
  /*executeJointTargetPreNominal();
  //executeJointTargetNominal();
  openGripper();
  publishApproachMarker();
  //publishGripperMarker (); //can be used when the grasp msg does not have an openning parameter
  publishGripperMarkerMoveit();
  generateWayPointsGrasp(grasp_);
  publishGraspWayPointsMarker();
  double score = executeWayPoints(grasp_wayPoints_);
  if(score<50.0)
  {
    ROS_INFO("Cartesian pose failed. Trying directly the grasp pose");
    executePoseGrasp(grasp_);
  }

  closeGripper();*/
  createTableCollisionObject();
  openPR2Gripper();
  ros::Duration(2).sleep();
  moveToHome();
  moveToApproach();

  std::vector<geometry_msgs::Pose> waypoints;
  generateGraspWayPoints(waypoints);
  double val = executeWayPoints(waypoints);

  removeTableCollisionObject();
  closePR2GripperByValue(grasp_.angle);
  ros::Duration(5).sleep();
}

bool GraspExecution::pickUp()
{
  //generatePickWayPoints();
  //publishPickWayPointsMarker();
  ROS_INFO("Picking up");
  moveToRetreat();
  //executeWayPoints(pick_wayPoints_);
  //executePickPose();


}

bool GraspExecution::place()
{
  ROS_INFO("Placing");
  moveToDrop();
  openPR2Gripper();
  moveToHome();
  //closePR2Gripper();
  ros::Duration(2).sleep();
}


}//end namespace
