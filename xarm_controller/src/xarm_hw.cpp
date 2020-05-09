/* Copyright 2018 UFACTORY Inc. All Rights Reserved.
 *
 * Software License Agreement (BSD License)
 *
 * Author: Jason Peng <jason@ufactory.cc>
 ============================================================================*/

#include "xarm_hw.h"

namespace xarm_control
{
	void XArmHW::clientInit(const std::string& robot_ip, ros::NodeHandle &root_nh)
	{
		position_cmd_.resize(dof_);
		position_cmd_float_.resize(dof_); // command vector must have 7 dimention!
		position_fdb_.resize(dof_);
		velocity_cmd_.resize(dof_);
		velocity_fdb_.resize(dof_);
		effort_cmd_.resize(dof_);
		effort_fdb_.resize(dof_);

		curr_err = 0;
		curr_state = 0;

		pos_sub_ = root_nh.subscribe(jnt_state_topic, 100, &XArmHW::pos_fb_cb, this);
		state_sub_ = root_nh.subscribe(xarm_state_topic, 100, &XArmHW::state_fb_cb, this);

		for(unsigned int j=0; j < dof_; j++)
	  	{
	  		// Create joint state interface for all joints
	    	js_interface_.registerHandle(hardware_interface::JointStateHandle(jnt_names_[j], &position_fdb_[j], &velocity_fdb_[j], &effort_fdb_[j]));

	    	hardware_interface::JointHandle joint_handle;
	    	joint_handle = hardware_interface::JointHandle(js_interface_.getHandle(jnt_names_[j]),&position_cmd_[j]);
	      	pj_interface_.registerHandle(joint_handle);
	  	}

	  	registerInterface(&js_interface_);
	  	registerInterface(&pj_interface_);
	  	
	  	int ret1 = xarm.motionEnable(1);
	  	int ret2 = xarm.setMode(XARM_MODE::SERVO);
	  	int ret3 = xarm.setState(XARM_STATE::START);

	  	if(ret3)
	  	{
	  		ROS_ERROR("The Xarm may not be properly connected or hardware error exists, PLEASE CHECK or RESTART HARDWARE!!!");
	  		ROS_ERROR(" ");
	  		ROS_ERROR("Did you specify the correct ros param xarm_robot_ip ? Exitting...");
	  		ros::shutdown();
	  		exit(1);
	  	}

	}

	bool XArmHW::init(ros::NodeHandle& root_nh, ros::NodeHandle& robot_hw_nh)
	{
		std::string hw_ns = robot_hw_nh.getNamespace() + "/";
		ros::service::waitForService(hw_ns+"motion_ctrl");
	  	ros::service::waitForService(hw_ns+"set_state");
	  	ros::service::waitForService(hw_ns+"set_mode");
	  	ros::service::waitForService(hw_ns+"move_servoj");
		xarm.init(robot_hw_nh);
		std::string robot_ip;
		std::vector<std::string> jnt_names;
		int xarm_dof = 0;

		if(!robot_hw_nh.hasParam("DOF"))
		{
			ROS_ERROR("ROS Parameter xarm_dof not specified!");
			return false;
		}
		if(!robot_hw_nh.hasParam("xarm_robot_ip"))
		{
			ROS_ERROR("ROS Parameter xarm_robot_ip not specified!");
			return false;
		}

		/* getParam forbids to change member */
		robot_hw_nh.getParam("DOF", xarm_dof);
		robot_hw_nh.getParam("xarm_robot_ip", robot_ip);
		robot_hw_nh.getParam("joint_names", jnt_names);
		dof_ = xarm_dof;
		jnt_names_ = jnt_names;

		clientInit(robot_ip, robot_hw_nh);
		return true;
	}

	XArmHW::~XArmHW()
	{
		xarm.setMode(XARM_MODE::POSE);
	}

	void XArmHW::pos_fb_cb(const sensor_msgs::msg::JointState::ConstPtr& data)
	{
		for(int j=0; j<dof_; j++)
		{
			position_fdb_[j] = data->position[j];
			velocity_fdb_[j] = data->velocity[j];
			effort_fdb_[j] = data->effort[j];
		}
	}

	void XArmHW::state_fb_cb(const xarm_msgs::srv::RobotMsg::ConstPtr& data)
	{
		curr_mode = data->mode;
		curr_state = data->state;
		curr_err = data->err;
	}

	void XArmHW::read(const ros::Time& time, const ros::Duration& period)
	{
		// basically the above feedback callback functions have done the job
	}

	void XArmHW::write(const ros::Time& time, const ros::Duration& period)
	{

		for(int k=0; k<dof_; k++)
		{
			position_cmd_float_[k] = (float)position_cmd_[k];
		}

		xarm.setServoJ(position_cmd_float_);

	}

	void XArmHW::get_status(int state_mode_err[3])
	{
		state_mode_err[0] = curr_state;
		state_mode_err[1] = curr_mode;
		state_mode_err[2] = curr_err;
	}

	bool XArmHW::need_reset()
	{
		if(curr_state==4 || curr_state==5 || curr_err)
			return true;
		else
			return false;
	}
}

PLUGINLIB_EXPORT_CLASS(xarm_control::XArmHW, hardware_interface::RobotHW)