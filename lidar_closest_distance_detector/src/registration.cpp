#include "registration.h"

namespace dyrosvehicle {

void RegistrationNodelet::addObstaclePointCloud(VPointCloud& lpc, const VPointCloud& rpc) {
  // Make the resultant point cloud take the newest stamp
  if (rpc.header.stamp > lpc.header.stamp)
    lpc.header.stamp = rpc.header.stamp;

  size_t nr_points = lpc.points.size ();
  lpc.points.resize (nr_points + rpc.points.size ());

  for (size_t i = nr_points; i < lpc.points.size (); ++i) {
      lpc.points[i] = rpc.points[i - nr_points];
  }

  lpc.width    = static_cast<uint32_t>(lpc.points.size ());
  lpc.height   = 1;

  // comment(edward): fix obstacle_points_registered issue which pointcloud goes to sparser
  // if (rpc.is_dense && lpc.is_dense)
  //   lpc.is_dense = true;
  // else
  //   lpc.is_dense = false;
  lpc.is_dense = false;

  lpc_ptr2 = lpc.makeShared();
  pcl::VoxelGrid<VPoint> vg;
  vg.setInputCloud(lpc_ptr2);
  vg.setLeafSize(0.2,0.2,0.2);
  vg.filter(lpc);
}

void RegistrationNodelet::points_obstacle_filtered_callback(const VPointCloud::ConstPtr &msg,
                                                            tf::TransformListener *listener,
                                                            tf::StampedTransform *transform_)
{
  try {
    listener->lookupTransform("camera_init", "camera", ros::Time(0), *transform_);
  }
  catch (tf::TransformException ex) {
    ROS_WARN("%s", ex.what());
  }

  pcl_ros::transformPointCloud(*msg, *msg_tf, *transform_);

  addObstaclePointCloud(*msg_sum, *msg_tf);

  msg_sum->header.frame_id = "camera_init";
  // publish to /points_obstacle_registered
  pub_obstacle_registered.publish(msg_sum);
}

void RegistrationNodelet::spacefilter_callback(const VPointCloud::ConstPtr &msg,
                                               ros::NodeHandle priv_nh) {
  if(loam_toggle == false) return;

  if(msg->points.empty()) return;

  double voxel_leaf = priv_nh.param<double>("spacefilter_voxel_leaf_size", 0.2);

  pcl::VoxelGrid<VPoint> vg;
  vg.setInputCloud(msg);
  vg.setLeafSize(voxel_leaf, voxel_leaf, voxel_leaf);
  vg.filter(*msg_vg);

  msg_vg->header.frame_id = "camera_init";
  // publish to /points_obstacle
  pub_obstacle.publish(msg_vg);
}

void RegistrationNodelet::loam_toggle_callback(const std_msgs::Bool::ConstPtr &msg) {
  loam_toggle = msg->data;
}

void RegistrationNodelet::onInit() {
  nh = getNodeHandle();
  priv_nh = getPrivateNodeHandle();

  pcl::console::setVerbosityLevel(pcl::console::L_ALWAYS);

  msg_tf = VPointCloud::Ptr(new VPointCloud);
  msg_ptfiltered = VPointCloud::Ptr(new VPointCloud);
  msg_vg = VPointCloud::Ptr(new VPointCloud);
  msg_sum = VPointCloud::Ptr(new VPointCloud);
  lpc_ptr2 = VPointCloud::Ptr(new VPointCloud);

  loam_toggle = true;

  // Subscribers
  sub_spacefilter = nh.subscribe<VPointCloud>("points_clipped", 1, boost::bind(&RegistrationNodelet::spacefilter_callback, this, _1, priv_nh));
  sub_obstacle_filtered = nh.subscribe<VPointCloud>("points_obstacle_filtered", 1, boost::bind(&RegistrationNodelet::points_obstacle_filtered_callback, this, _1 ,&listener, &transform_));

  sub_loam_toggle = nh.subscribe<std_msgs::Bool>("loam_toggle", 1, &RegistrationNodelet::loam_toggle_callback, this);

  // Publishers
  pub_obstacle = nh.advertise<VPointCloud>("points_obstacle", 1);
  pub_obstacle_registered = nh.advertise<VPointCloud>("points_obstacle_registered", 1);
}
} // namespace dyrosvehicle
