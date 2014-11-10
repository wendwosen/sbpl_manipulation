/*
 * Copyright (c) 2010, Maxim Likhachev
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
 *     * Neither the name of the University of Pennsylvania nor the names of its
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

 /** \author Benjamin Cohen */

#include <sbpl_manipulation_components/occupancy_grid.h>
#include <ros/console.h>
#include <leatherman/viz.h>
#include <leatherman/objects.h>

using namespace std;

namespace sbpl_arm_planner
{

OccupancyGrid::OccupancyGrid(double dim_x, double dim_y, double dim_z, double resolution, double origin_x, double origin_y, double origin_z)
{
  grid_ = new distance_field::PropagationDistanceField(dim_x, dim_y, dim_z, resolution, origin_x, origin_y,  origin_z, 0.40);
  grid_->reset();
  delete_grid_ = true;
}

OccupancyGrid::OccupancyGrid(distance_field::PropagationDistanceField* df)
{
  grid_ = df;
  delete_grid_ = false;
}

OccupancyGrid::~OccupancyGrid()
{
  if(grid_ && delete_grid_)
    delete grid_;
}

void OccupancyGrid::getGridSize(int &dim_x, int &dim_y, int &dim_z)
{
  dim_x = grid_->getSizeX() / grid_->getResolution();
  dim_y = grid_->getSizeY() / grid_->getResolution();
  dim_z = grid_->getSizeZ() / grid_->getResolution();
}

void OccupancyGrid::getWorldSize(double &dim_x, double &dim_y, double &dim_z)
{
  dim_x = grid_->getSizeX();
  dim_y = grid_->getSizeY();
  dim_z = grid_->getSizeZ();
}

void OccupancyGrid::reset()
{
  grid_->reset();
}

void OccupancyGrid::getOrigin(double &wx, double &wy, double &wz)
{
  grid_->gridToWorld(0, 0, 0, wx, wy, wz);
}

double OccupancyGrid::getResolution()
{
  return grid_->getResolution();
}

void OccupancyGrid::updateFromCollisionMap(const arm_navigation_msgs::CollisionMap &collision_map)
{
  if(collision_map.boxes.empty())
  {
    ROS_DEBUG("[grid] collision map received is empty.");
    return;
  }
  reference_frame_ = collision_map.header.frame_id;
  addCollisionMapToField(collision_map);
}

void OccupancyGrid::updateFromOctree(const octomap::OcTree* oct)
{
  grid_->addOcTreeToField(oct);
}

void OccupancyGrid::addCube(double origin_x, double origin_y, double origin_z, double size_x, double size_y, double size_z)
{
  EigenSTL::vector_Vector3d pts;

  for (double x=origin_x-size_x/2.0; x<=origin_x+size_x/2.0; x+=grid_->getResolution())
  {
    for (double y=origin_y-size_y/2.0; y<=origin_y+size_y/2.0; y+=grid_->getResolution())
    {
      for (double z=origin_z-size_z/2.0; z<=origin_z+size_z/2.0; z+=grid_->getResolution())
      {
        pts.push_back(Eigen::Vector3d(x, y, z));
      }
    }
  }
  grid_->addPointsToField(pts);
}

void OccupancyGrid::getOccupiedVoxels(const geometry_msgs::Pose &pose, const std::vector<double> &dim, std::vector<Eigen::Vector3d> &voxels)
{
  Eigen::Vector3d vin, vout, v(pose.position.x, pose.position.y, pose.position.z);
  Eigen::Matrix3d m(Eigen::Quaterniond(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z));

  for (double x=0-dim[0]/2.0; x<=dim[0]/2.0; x+=grid_->getResolution())
  {
    for (double y=0-dim[1]/2.0; y<=dim[1]/2.0; y+=grid_->getResolution())
    {
      for (double z=0-dim[2]/2.0; z<=dim[2]/2.0; z+=grid_->getResolution())
      {
        vin(0) = (x);
        vin(1) = (y);
        vin(2) = (z); 
        vout = m*vin;
        vout += v;

        voxels.push_back(vout);
      }
    }
  }
}

void OccupancyGrid::getOccupiedVoxels(double x_center, double y_center, double z_center, double radius, std::string text, std::vector<geometry_msgs::Point> &voxels)
{
  int x_c, y_c, z_c, radius_c;
  geometry_msgs::Point v;
  worldToGrid(x_center, y_center, z_center, x_c, y_c, z_c);
  radius_c = radius/getResolution() + 0.5;

  for(int z = z_c-radius_c; z < z_c+radius_c; ++z)
  {
    for(int y = y_c-radius_c; y < y_c+radius_c; ++y)
    {
      for(int x = x_c-radius_c; x < x_c+radius_c; ++x)
      {
        if(getCell(x,y,z) == 0)
        {
          gridToWorld(x, y, z, v.x, v.y, v.z);
          voxels.push_back(v);
        }
      }
    }
  }
}

void OccupancyGrid::getOccupiedVoxels(std::vector<geometry_msgs::Point> &voxels)
{
  geometry_msgs::Point v;
  std::vector<double> dim(3,0), origin(3,0);
  getOrigin(origin[0], origin[1], origin[2]);
  getWorldSize(dim[0], dim[1], dim[2]);
  
  for(double x=origin[0]; x<origin[0]+dim[0]-grid_->getResolution(); x+=grid_->getResolution())
  {
    for(double y=origin[1]; y<origin[1]+dim[1]-grid_->getResolution(); y+=grid_->getResolution())
    {
      for(double z=origin[2]; z<origin[2]+dim[2]-grid_->getResolution(); z+=grid_->getResolution())
      {
        if(getDistanceFromPoint(x,y,z) == 0)
        {
          v.x = x;
          v.y = y;
          v.z = z;
          voxels.push_back(v);
        }
      }
    }
  }
}

visualization_msgs::MarkerArray OccupancyGrid::getVisualization(std::string type)
{
  visualization_msgs::MarkerArray ma;

  if(type.compare("bounds") == 0)
  {
    double dimx, dimy, dimz, originx, originy, originz;
    std::vector<geometry_msgs::Point> pts(10);
    getOrigin(originx, originy, originz);
    getWorldSize(dimx,dimy,dimz);
    pts[0].x = originx;      pts[0].y = originy;      pts[0].z = originz;
    pts[1].x = originx+dimx; pts[1].y = originy;      pts[1].z = originz;
    pts[2].x = originx+dimx; pts[2].y = originy+dimy; pts[2].z = originz;
    pts[3].x = originx;      pts[3].y = originy+dimy; pts[3].z = originz;
    pts[4].x = originx;      pts[4].y = originy;      pts[4].z = originz;
    pts[5].x = originx;      pts[5].y = originy;      pts[5].z = originz+dimz;
    pts[6].x = originx+dimx; pts[6].y = originy;      pts[6].z = originz+dimz;
    pts[7].x = originx+dimx; pts[7].y = originy+dimy; pts[7].z = originz+dimz;
    pts[8].x = originx;      pts[8].y = originy+dimy; pts[8].z = originz+dimz;
    pts[9].x = originx;      pts[9].y = originy;      pts[9].z = originz+dimz;

    ma.markers.resize(1);
    ma.markers[0] = viz::getLineMarker(pts, 0.05, 10, getReferenceFrame(), "occupancy_grid_bounds", 0);
  }
  else if(type.compare("distance_field") == 0)
  {
    visualization_msgs::Marker m;
    grid_->getIsoSurfaceMarkers(0.01, grid_->getResolution(), getReferenceFrame(), ros::Time::now(), m);
    m.color.a += 0.2;
    ma.markers.push_back(m);
  }
  else if(type.compare("occupied_voxels") == 0)
  {
    visualization_msgs::Marker marker;
    std::vector<geometry_msgs::Point> voxels;
    getOccupiedVoxels(voxels);
    marker.header.seq = 0;
    marker.header.stamp = ros::Time::now();
    marker.header.frame_id = getReferenceFrame();
    marker.ns = "occupied_voxels";
    marker.id = 1;
    marker.type = visualization_msgs::Marker::POINTS;
    marker.action = visualization_msgs::Marker::ADD;
    marker.lifetime = ros::Duration(0.0);
    marker.scale.x = grid_->getResolution() / 2.0;
    marker.scale.y = grid_->getResolution() / 2.0;
    marker.scale.z = grid_->getResolution() / 2.0;
    marker.color.r = 0.8;
    marker.color.g = 0.3;
    marker.color.b = 0.5;
    marker.color.a = 1;
    marker.points = voxels;
    ma.markers.push_back(marker);
  }
  else
    ROS_ERROR("No visualization found of type '%s'.", type.c_str());
  return ma;
}

bool OccupancyGrid::writeCollisionMapToBagFile(const arm_navigation_msgs::CollisionMap &map, std::string bag_filename, std::string topic_name)
{
  //TODO: Add in error checking (catch the BagExceptions)
  rosbag::Bag bag;
  bag.open(bag_filename, rosbag::bagmode::Write);
  bag.write(topic_name, ros::Time::now(), map);
  bag.close();
  return true;
}

bool OccupancyGrid::writeOccupancyGridToBagFile(std::string bag_filename, std::string topic_name)
{
  arm_navigation_msgs::CollisionMap map;
  arm_navigation_msgs::OrientedBoundingBox box;
  std::vector<double> dim(3,0), origin(3,0);
  getOrigin(origin[0], origin[1], origin[2]);
  getWorldSize(dim[0], dim[1], dim[2]);

  map.header.frame_id = reference_frame_;
  box.angle = 0.0;
  box.extents.x = grid_->getResolution();
  box.extents.y = grid_->getResolution();
  box.extents.z = grid_->getResolution();
 
  for(double x=origin[0]; x<=origin[0]+dim[0]; x+=grid_->getResolution())
  {
    for(double y=origin[1]; y<=origin[1]+dim[1]; y+=grid_->getResolution())
    {
      for(double z=origin[2]; z<=origin[2]+dim[2]; z+=grid_->getResolution())
      {
        if(getDistanceFromPoint(x,y,z) == 0)
        {
          box.center.x = x;
          box.center.y = y;
          box.center.z = z;
          map.boxes.push_back(box);
        }
      }
    }
  }
  return writeCollisionMapToBagFile(map, bag_filename, topic_name);
}

void OccupancyGrid::addCollisionMapToField(const arm_navigation_msgs::CollisionMap &collision_map)
{
  size_t num_boxes = collision_map.boxes.size();
  EigenSTL::vector_Vector3d points;
  points.reserve(num_boxes);
  for (size_t i=0; i<num_boxes; ++i)
  {
    points.push_back(Eigen::Vector3d(collision_map.boxes[i].center.x, collision_map.boxes[i].center.y, collision_map.boxes[i].center.z));
  }
  grid_->addPointsToField(points);
}

void OccupancyGrid::addShapeToField(const arm_navigation_msgs::Shape &shape_msg, const geometry_msgs::Pose &pose)
{
  shape_msgs::SolidPrimitive solid;
  leatherman::convertShapeToSolidPrimitive(shape_msg, solid);
  shapes::Shape* shape = shapes::constructShapeFromMsg(solid);
  grid_->addShapeToField(shape, pose);
}

void OccupancyGrid::addShapeToField(const shape_msgs::SolidPrimitive &shape_msg, const geometry_msgs::Pose &pose)
{
  shapes::Shape* shape = shapes::constructShapeFromMsg(shape_msg);
  grid_->addShapeToField(shape, pose);
}

void OccupancyGrid::removeShapeFromField(const shape_msgs::SolidPrimitive &shape_msg, const geometry_msgs::Pose &pose)
{
  shapes::Shape* shape = shapes::constructShapeFromMsg(shape_msg);
  grid_->removeShapeFromField(shape, pose);
}

void OccupancyGrid::removeShapeFromField(const arm_navigation_msgs::Shape &shape_msg, const geometry_msgs::Pose &pose)
{
  shape_msgs::SolidPrimitive solid;
  leatherman::convertShapeToSolidPrimitive(shape_msg, solid);
  shapes::Shape* shape = shapes::constructShapeFromMsg(solid);
  grid_->removeShapeFromField(shape, pose);
}

}
