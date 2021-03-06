/***********************************************************************/
/**                                                                    */
/** teresa_node.hpp                                                    */
/**                                                                    */
/** Copyright (c) 2016, Service Robotics Lab.                          */ 
/**                     http://robotics.upo.es                         */
/**                                                                    */
/** All rights reserved.                                               */
/**                                                                    */
/** Authors:                                                           */
/** Ignacio Perez-Hurtado (maintainer)                                 */
/** Noe Perez                                                          */
/** Rafael Ramon                                                       */
/** David Alejo Teissière                                              */
/** Fernando Caballero                                                 */
/** Jesus Capitan                                                      */
/** Luis Merino                                                        */
/**                                                                    */   
/** This software may be modified and distributed under the terms      */
/** of the BSD license. See the LICENSE file for details.              */
/**                                                                    */
/** http://www.opensource.org/licenses/BSD-3-Clause                    */
/**                                                                    */
/***********************************************************************/
#ifndef _TERESA_NODE_HPP_
#define _TERESA_NODE_HPP_

#include <ros/ros.h>
#include <string>
#include <cmath>	
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>			
#include <geometry_msgs/Twist.h>		
#include <sensor_msgs/Imu.h>
#include <teresa_driver/Stalk.h>
#include <teresa_driver/StalkRef.h>
#include <teresa_driver/Temperature.h>
#include <teresa_driver/Buttons.h>
#include <teresa_driver/Volume.h>
#include <teresa_driver/Batteries.h>
#include <teresa_driver/Set_DCDC.h>
#include <teresa_driver/Get_DCDC.h>
#include <teresa_driver/Teresa_leds.h>
#include <teresa_driver/Diagnostics.h>
#include <teresa_driver/CmdVelRaw.h>
#include <teresa_driver/simulated_teresa_robot.hpp>
#include <teresa_driver/idmind_teresa_robot.hpp>
#include <teresa_driver/teresa_leds.hpp>

namespace Teresa
{




/**
 * Enumeration to manage the status of the tilt and height motors
 */
enum MotorStatus {MOTOR_UP, MOTOR_DOWN, MOTOR_STOP};

/**
 * The ROS node class
 */
class Node
{
public:
	Node(ros::NodeHandle& n, ros::NodeHandle& pn);
	~Node();
private:
	void loop(); // The main loop
	void imuReceived(const sensor_msgs::Imu::ConstPtr& imu); // The IMU callback function
	void stalkReceived(const teresa_driver::Stalk::ConstPtr& stalk); // The joystick stalk callback funcrion
	void stalkRefReceived(const teresa_driver::StalkRef::ConstPtr& stalk_ref);
	void cmdVelReceived(const geometry_msgs::Twist::ConstPtr& cmd_vel); // The Command vel callback function
	void cmdVelRawReceived(const teresa_driver::CmdVelRaw::ConstPtr& vel_ref); // The raw vel callback function

	bool setDCDC(teresa_driver::Set_DCDC::Request  &req,
			teresa_driver::Set_DCDC::Response &res); // Set DCDC service
	bool getDCDC(teresa_driver::Get_DCDC::Request  &req,
			teresa_driver::Get_DCDC::Response &res); // Get DCDC service
	bool teresaLeds(teresa_driver::Teresa_leds::Request &req,
				teresa_driver::Teresa_leds::Response &res); // The Leds service

	static void printInfo(const std::string& message){ROS_INFO("%s",message.c_str());} // Print Info function
	static void printError(const std::string& message){ROS_ERROR("%s",message.c_str());} // Print Error function
	
	ros::NodeHandle& n;
	double lin_vel; // Linear velocity
	double ang_vel; // Angular velocity
	bool imu_error; // IMU error?
	double yaw; // Yaw angle
	double inc_yaw; // Yaw increment
	bool imu_first_time; // Is it the first time we get IMU data?
	int using_imu; // Are we using an IMU?
	int publish_temperature; // Are we going to publish the temperatures? (1 = yes, 0 = no)
	int publish_buttons; // Are we going to publish the arcade buttons?
	int publish_volume;  // Are we going to publish the rotary volumen control?
	int publish_diagnostics; // Are we going to publish diagnostics?
	int height_velocity; // The configured heght motor velocity in mm/s
	int tilt_velocity; // The configured tilt motor velocity in degrees/s
	double freq; // Main loop frequency;
        int number_of_leds; // Number of leds
	bool use_upo_calib;
	// Frame IDs
	std::string base_frame_id;
	std::string odom_frame_id;
	std::string head_frame_id;
	std::string stalk_frame_id;
	// Publishers and subscribers
	ros::Publisher odom_pub;
	ros::Subscriber cmd_vel_sub;
	ros::Subscriber cmd_vel_raw_sub;
	ros::Subscriber imu_sub;
	ros::Subscriber stalk_sub;
	ros::Subscriber stalk_ref_sub;
	ros::Publisher buttons_pub;
	ros::Publisher batteries_pub;
	ros::Publisher volume_pub;
	ros::Publisher diagnostics_pub;
	ros::Publisher temperature_pub;	
	// Services
	ros::ServiceServer set_dcdc_service;
	ros::ServiceServer get_dcdc_service;
	ros::ServiceServer leds_service;

	// Some time stamps... see the code below
	ros::Time imu_time; 
	ros::Time imu_past_time; 
	ros::Time cmd_vel_time; 

	Robot *teresa; // The robot interface
	MotorStatus tiltMotor; // Status of the tilt motor
	MotorStatus heightMotor; // Status of the height motor
	Leds *leds; // A little bit of fun

	Calibration calibration; // Calibration parameters

	bool deadZoneIsActive;
	double lin_vel_dead_zone;
	double ang_vel_dead_zone;
	double lin_vel_zero_threshold;
	double ang_vel_zero_threshold;
	

};

inline
Node::Node(ros::NodeHandle& n, ros::NodeHandle& pn)
: n(n), 
  lin_vel(0.0),
  ang_vel(0.0),
  imu_error(false),
  yaw(0.0),
  inc_yaw(0.0),
  imu_first_time(true),
  teresa(NULL),
  tiltMotor(MOTOR_STOP),
  heightMotor(MOTOR_STOP),
  leds(NULL)
{
	try
	{
		int simulation;
		std::string board1;
		std::string board2;
		std::string leds_pattern;
		int initial_dcdc_mask,final_dcdc_mask;
	        // Parameters
		pn.param<std::string>("board1",board1,"/dev/ttyUSB0");
		pn.param<std::string>("board2",board2,"/dev/ttyUSB1");
		pn.param<std::string>("base_frame_id", base_frame_id, "/base_link");
		pn.param<std::string>("odom_frame_id", odom_frame_id, "/odom");
		pn.param<std::string>("head_frame_id", head_frame_id, "/teresa_head");	
	    	pn.param<std::string>("stalk_frame_id", stalk_frame_id, "/teresa_stalk");
		pn.param<int>("simulation",simulation,0);	
		pn.param<int>("using_imu", using_imu, 1);
		pn.param<int>("publish_temperature", publish_temperature, 1);
		pn.param<int>("publish_buttons", publish_buttons, 1);
		pn.param<int>("publish_volume", publish_volume, 1);
		pn.param<int>("publish_diagnostics",publish_diagnostics,1);
		pn.param<int>("number_of_leds",number_of_leds,60);
		pn.param<int>("initial_dcdc_mask",initial_dcdc_mask,0xFF);
		pn.param<int>("final_dcdc_mask",final_dcdc_mask,0x00);
		pn.param<double>("freq",freq,20);
		pn.param<int>("height_velocity",height_velocity,20);
		pn.param<int>("tilt_velocity",tilt_velocity,2);
		pn.param<bool>("inverse_left_motor",calibration.inverse_left_motor,true);
		pn.param<bool>("inverse_right_motor",calibration.inverse_right_motor,false);
		pn.param<std::string>("leds_pattern",leds_pattern,"null");
		pn.param<double>("A_left",calibration.A_left,41.2); //210.0);
		pn.param<double>("B_left",calibration.B_left,0.0); //8.35);
		pn.param<double>("A_right",calibration.A_right,41.2); //210.0);
		pn.param<double>("B_right",calibration.B_right,0.0); //8.35);
		pn.param<bool>("use_upo_calib",use_upo_calib, true);
		pn.param<bool>("deadZoneIsActive", deadZoneIsActive, true);
		pn.param<double>("lin_vel_dead_zone",lin_vel_dead_zone,0.15);
		pn.param<double>("ang_vel_dead_zone",ang_vel_dead_zone,0.3);
		pn.param<double>("lin_vel_zero_threshold",lin_vel_zero_threshold,0.05);
		pn.param<double>("ang_vel_zero_threshold",ang_vel_zero_threshold,0.05);
		leds = getLedsPattern(leds_pattern,number_of_leds);
		
		if (simulation) {
			using_imu=0;
			// Using a simulated robot, for debugging and testing
			teresa = new SimulatedRobot(); 
		} else {
			// Using the IdMind robot
			teresa = new IdMindRobot(board1,board2,calibration,initial_dcdc_mask,final_dcdc_mask,number_of_leds,printInfo,printError);
		}
		teresa->setHeightVelocity(height_velocity);
		teresa->setTiltVelocity(tilt_velocity);
		// Publishers and subscribers
		odom_pub = pn.advertise<nav_msgs::Odometry>(odom_frame_id, 5);
		cmd_vel_sub = n.subscribe<geometry_msgs::Twist>("/cmd_vel",1,&Node::cmdVelReceived,this);
		cmd_vel_raw_sub = n.subscribe<teresa_driver::CmdVelRaw>("/cmd_vel_raw",1,&Node::cmdVelRawReceived,this);
		if (using_imu) {
			imu_sub = n.subscribe<sensor_msgs::Imu>("/imu/data",1,&Node::imuReceived,this);	
		}
		stalk_sub = n.subscribe<teresa_driver::Stalk>("/stalk",1,&Node::stalkReceived,this);
		stalk_ref_sub =n.subscribe<teresa_driver::StalkRef>("/stalk_ref",1,&Node::stalkRefReceived,this);
		if (publish_buttons) {
			buttons_pub = pn.advertise<teresa_driver::Buttons>("/arcade_buttons",5);
		}
		if (publish_volume) {
			volume_pub = pn.advertise<teresa_driver::Volume>("/volume_increment",5);
		}
		if (publish_temperature) {
			temperature_pub = pn.advertise<teresa_driver::Temperature>("/temperatures",5);
		}
		if (publish_diagnostics) {
			diagnostics_pub = pn.advertise<teresa_driver::Diagnostics>("/teresa_diagnostics",5);
		}
		batteries_pub = pn.advertise<teresa_driver::Batteries>("/batteries",5);	
		// Services
		set_dcdc_service = n.advertiseService("set_teresa_dcdc", &Node::setDCDC,this);
		get_dcdc_service = n.advertiseService("get_teresa_dcdc", &Node::getDCDC,this);				
		leds_service = n.advertiseService("teresa_leds", &Node::teresaLeds,this);
		// Run the main loop
		loop();
	} catch (const char* msg) {
		// I have a bad feeling about this...
		ROS_FATAL("%s",msg);
	}	
}

inline
Node::~Node()
{
	delete teresa;
	delete leds;
}

// IMU callback function
inline
void Node::imuReceived(const sensor_msgs::Imu::ConstPtr& imu)
{
	imu_time = ros::Time::now();
	// The first time we get data from the IMU
	if (imu_first_time) {
		imu_past_time = imu->header.stamp;
		imu_first_time=false;
		return;
	}
	// Calculate the duration since the last received message
	double duration = (imu->header.stamp - imu_past_time).toSec();
	// Update the time of the last received message
	imu_past_time=imu->header.stamp; 
	// Is the robot stopped?
    	if (teresa->isStopped() || fabs(imu->angular_velocity.z) < 0.04) {
		ang_vel = 0.0;
		return;
	}
	// Get angular velocity from IMU
	ang_vel = imu->angular_velocity.z;
	// Increment yaw angle
	inc_yaw += ang_vel * duration;
}

// Stalk callback function (command from joystick)
inline
void Node::stalkReceived(const teresa_driver::Stalk::ConstPtr& stalk)
{ 
	if (stalk->head_up && heightMotor!=MOTOR_UP) { // height motor UP
		teresa->setHeight(MAX_HEIGHT_MM);
		heightMotor = MOTOR_UP;
	}
	else // height motor DOWN
	if (stalk->head_down && heightMotor!=MOTOR_DOWN) {
		teresa->setHeight(MIN_HEIGHT_MM);
		heightMotor = MOTOR_DOWN;
	}
	else if (heightMotor!=MOTOR_STOP){ // height motor STOP
		int height_in_millimeters=0;
		if (teresa->getHeight(height_in_millimeters) &&
			teresa->setHeight(height_in_millimeters)) {
			heightMotor = MOTOR_STOP;
		}
	}
	
	if (stalk->tilt_up && tiltMotor!=MOTOR_UP) { //tilt motor UP
		teresa->setTilt(MAX_TILT_ANGLE_DEGREES);
		tiltMotor = MOTOR_UP;
	}
	else
	if (stalk->tilt_down && tiltMotor!=MOTOR_DOWN) { // tilt motor DOWN
		teresa->setTilt(MIN_TILT_ANGLE_DEGREES);
		tiltMotor = MOTOR_DOWN;
	}
	else if (tiltMotor!=MOTOR_STOP){ // tilt motor STOP
		int tilt_in_degrees=0;
		if (teresa->getTilt(tilt_in_degrees) &&
			teresa->setTilt(tilt_in_degrees)) {
			tiltMotor = MOTOR_STOP;
		}
	}
}

// StalkRef callback function
inline
void Node::stalkRefReceived(const teresa_driver::StalkRef::ConstPtr& stalk_ref)
{ 
	teresa->setHeight((int)std::round(stalk_ref->head_height*1000)); // From meters to millimeters
        teresa->setTilt((int)std::round(stalk_ref->head_tilt * 57.2958)); // From radians to degrees
}

// CmdVel callback function
inline
void Node::cmdVelReceived(const geometry_msgs::Twist::ConstPtr& cmd_vel)
{ 
	cmd_vel_time = ros::Time::now(); // Get the time
	if (!imu_error) { // If IMU error, do not move!
		double cmdLinVel = cmd_vel->linear.x;
		double cmdAngVel = cmd_vel->angular.z;

		if(deadZoneIsActive) {
			//if robot is (almost) stopped
			if(fabs(lin_vel) < lin_vel_zero_threshold && fabs(ang_vel) < ang_vel_zero_threshold)
			{
				if (fabs(cmdAngVel)>0 && fabs(cmdAngVel)<ang_vel_dead_zone && fabs(cmdLinVel)<lin_vel_dead_zone) {
					cmdAngVel = ang_vel_dead_zone;
				} else if (fabs(cmdLinVel)>0 && fabs(cmdLinVel)<lin_vel_dead_zone && fabs(cmdAngVel)<ang_vel_dead_zone) {
					cmdLinVel = lin_vel_dead_zone;
				} 
			}
		}

		if(use_upo_calib)
			teresa->setVelocity2( cmdLinVel, cmdAngVel);
		else
			teresa->setVelocity( cmdLinVel, cmdAngVel);
	}
}

// CmdVelRaw callback function
inline
void Node::cmdVelRawReceived(const teresa_driver::CmdVelRaw::ConstPtr& vel_ref)
{
	teresa->setVelocityRaw(vel_ref->left_wheel, vel_ref->right_wheel);
}


// Set DCDC service
inline
bool Node::setDCDC(teresa_driver::Set_DCDC::Request  &req,
			teresa_driver::Set_DCDC::Response &res)
{
	if (req.mode>2) {
		res.success = false;
	} else if (req.mode == 0) { 
		res.success = teresa->enableDCDC(req.mask);
	} else {
		unsigned char mask;
		if (!teresa->getDCDC(mask)) {
			res.success = false;
		} else if (req.mode == 1) { 
			res.success = teresa->enableDCDC(mask | req.mask);
		} else { 
			res.success = teresa->enableDCDC(mask & ~req.mask); 
		} 
	}
	return true;
}

// Get DCDC service
inline
bool Node::getDCDC(teresa_driver::Get_DCDC::Request  &req,
			teresa_driver::Get_DCDC::Response &res)
{ 
	res.mask = 0;
	res.success = teresa->getDCDC(res.mask);
	return true;
}


// Leds service
inline
bool Node::teresaLeds(teresa_driver::Teresa_leds::Request &req,
			teresa_driver::Teresa_leds::Response &res)
{
	if ((int)req.rgb_values.size() != (number_of_leds*3)) {
		res.success = false;
		return true;
	}

	if (leds!=NULL) {
		delete leds;
		leds = NULL;
	} 
	res.success = teresa->setLeds(req.rgb_values);
	return true;
}

// Main Loop
inline
void Node::loop()
{
	
	double pos_x=0.0;
	double pos_y=0.0;
	ros::Time current_time,last_time;
	if (using_imu) {
		imu_time = ros::Time::now();
	}
	last_time = ros::Time::now();
	cmd_vel_time = ros::Time::now();
	ros::Rate r(freq);
	tf::TransformBroadcaster tf_broadcaster;
	double imdl,imdr;
	double dt;
	bool first_time=true;
	double height_in_meters=0;
	int height_in_millimeters=0;
	double tilt_in_radians=0;
	int tilt_in_degrees=0;
	bool button1=false,button2=false;
	bool button1_tmp,button2_tmp;
	int rotaryEncoder;
	unsigned char elec_level, PC1_level, motorH_level, motorL_level, charger_status;
	int temperature_left_motor,temperature_right_motor,temperature_left_driver,temperature_right_driver;
	bool tilt_overheat,height_overheat;
	PowerDiagnostics diagnostics;
	double loopDurationSum=0;
	unsigned long loopCounter=0;
	while (n.ok()) {
		current_time = ros::Time::now();
		if (using_imu) {		
			double imu_sec = (current_time - imu_time).toSec();
			if(imu_sec >= 0.25){
				teresa->setVelocity(0,0);
				ang_vel = 0;
				imu_error = true;
				ROS_WARN("-_-_-_-_-_- IMU STOP -_-_-_-_-_- imu_sec=%.3f sec",imu_sec);
			} else {
				imu_error = false;
			}
		}
		double cmd_vel_sec = (current_time - cmd_vel_time).toSec();
		if (cmd_vel_sec >= 0.5) {
			teresa->setVelocity(0,0);
		}
		teresa->getIMD(imdl,imdr);
		dt = (current_time - last_time).toSec();
		if (!using_imu) {
			double vr = imdr/dt;
			double vl = imdl/dt;
			ang_vel = (vr-vl)/ROBOT_DIAMETER_M;
			inc_yaw += ang_vel*dt;
		}
		last_time = current_time;
		if (!first_time) {
			double imd = (imdl+imdr)/2;
			lin_vel = imd / dt;
			pos_x += imd*std::cos(yaw + ang_vel*dt/2);
			pos_y += imd*std::sin(yaw + ang_vel*dt/2);
			yaw += inc_yaw;
			inc_yaw = 0;
		} 
		// ******************************************************************************************
		//first, we'll publish the transforms over tf
		geometry_msgs::TransformStamped odom_trans;
		odom_trans.header.stamp = current_time;
		odom_trans.header.frame_id = odom_frame_id;
		odom_trans.child_frame_id = base_frame_id;
		odom_trans.transform.translation.x = pos_x;
		odom_trans.transform.translation.y = pos_y;
		odom_trans.transform.translation.z = 0.0;
		odom_trans.transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, yaw);
		tf_broadcaster.sendTransform(odom_trans);
		if (teresa->getHeight(height_in_millimeters)) {
			//ROS_INFO("%d",height_in_millimeters);
			height_in_meters= (double)height_in_millimeters * 0.001;
		}
		
        	if (teresa->getTilt(tilt_in_degrees)) {
			//ROS_INFO("%d",tilt_in_degrees);
			tilt_in_radians = tilt_in_degrees * 0.0174533;
		}

		geometry_msgs::TransformStamped stalk_trans;
		stalk_trans.header.stamp = current_time;
		stalk_trans.header.frame_id = base_frame_id;
		stalk_trans.child_frame_id = stalk_frame_id;
		stalk_trans.transform.translation.x = 0.0;
		stalk_trans.transform.translation.y = 0.0;
		stalk_trans.transform.translation.z = height_in_meters;
		stalk_trans.transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, 0.0);
		tf_broadcaster.sendTransform(stalk_trans);

		geometry_msgs::TransformStamped head_trans;
		head_trans.header.stamp = current_time;
		head_trans.header.frame_id = stalk_frame_id;
		head_trans.child_frame_id = head_frame_id;
		head_trans.transform.translation.x = 0.0;
		head_trans.transform.translation.y = 0.0;
		head_trans.transform.translation.z = 0.0;
		head_trans.transform.rotation = tf::createQuaternionMsgFromRollPitchYaw(0.0, tilt_in_radians, 0.0);
		tf_broadcaster.sendTransform(head_trans);

		// ******************************************************************************************
		//next, we'll publish the odometry message over ROS
		nav_msgs::Odometry odom;
		odom.header.stamp = current_time;
		odom.header.frame_id = odom_frame_id;
		
		//set the position
		odom.pose.pose.position.x = pos_x;
		odom.pose.pose.position.y = pos_y;
		odom.pose.pose.position.z = 0.0;
		odom.pose.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0.0, 0.0, yaw);
		
		//set the velocity
		odom.child_frame_id = base_frame_id;
		odom.twist.twist.linear.x = lin_vel;
		odom.twist.twist.linear.y = 0.0; 
		odom.twist.twist.angular.z = ang_vel;
		
		//publish the odometry
		odom_pub.publish(odom);

		//publish the state of the batteries
		if (teresa->getBatteryStatus(elec_level,PC1_level,motorH_level,motorL_level,charger_status)) {
			teresa_driver::Batteries battmsg;
			battmsg.header.stamp = current_time;
			battmsg.elec_level = elec_level;
			battmsg.PC1_level = PC1_level;
			battmsg.motorH_level = motorH_level;
			battmsg.motorL_level = motorL_level;
			battmsg.charger_status = charger_status;
			batteries_pub.publish(battmsg);	
		}

		//publish the state of the buttons
		if (publish_buttons &&	teresa->getButtons(button1_tmp,button2_tmp) && 
			(first_time || button1!=button1_tmp || button2!=button2_tmp)) {
			button1 = button1_tmp;
			button2 = button2_tmp;
			teresa_driver::Buttons buttonsmsg;
			buttonsmsg.header.stamp = current_time;
			buttonsmsg.button1=button1;
			buttonsmsg.button2=button2;
			buttons_pub.publish(buttonsmsg);
		}
		//publish the state of the rotaryEncoder				
		if (publish_volume && teresa->getRotaryEncoder(rotaryEncoder) && rotaryEncoder!=0) {
			teresa_driver::Volume volumemsg;
			volumemsg.header.stamp = current_time;
			volumemsg.volume_inc=rotaryEncoder;
			volume_pub.publish(volumemsg);
		}
		//publish the temperatures
		if (publish_temperature &&
			teresa->getTemperature(temperature_left_motor,
						temperature_right_motor,
						temperature_left_driver,
						temperature_right_driver,
						tilt_overheat,
						height_overheat)) {
			teresa_driver::Temperature temperaturemsg;
			temperaturemsg.header.stamp = current_time;
			temperaturemsg.left_motor_temperature = temperature_left_motor;
			temperaturemsg.right_motor_temperature = temperature_right_motor;
			temperaturemsg.left_driver_temperature = temperature_left_driver;
			temperaturemsg.right_driver_temperature = temperature_right_driver;
			temperaturemsg.tilt_driver_overheat = tilt_overheat;
			temperaturemsg.height_driver_overheat = height_overheat;
			temperature_pub.publish(temperaturemsg);
		}

		//publish diagnostics
		if (publish_diagnostics && teresa->getPowerDiagnostics(diagnostics)) {
			teresa_driver::Diagnostics diagnosticsmsg;
			diagnosticsmsg.header.stamp = current_time;
			diagnosticsmsg.elec_bat_voltage = diagnostics.elec_bat_voltage;
			diagnosticsmsg.PC1_bat_voltage = diagnostics.PC1_bat_voltage;
			diagnosticsmsg.cable_bat_voltage = diagnostics.cable_bat_voltage;
			diagnosticsmsg.motor_voltage = diagnostics.motor_voltage;
			diagnosticsmsg.motor_h_voltage = diagnostics.motor_h_voltage;
			diagnosticsmsg.motor_l_voltage = diagnostics.motor_l_voltage;
			diagnosticsmsg.elec_instant_current = diagnostics.elec_instant_current;
			diagnosticsmsg.motor_instant_current = diagnostics.motor_instant_current;
			diagnosticsmsg.elec_integrated_current = diagnostics.elec_integrated_current;
			diagnosticsmsg.motor_integrated_current = diagnostics.motor_integrated_current;
			diagnosticsmsg.average_loop_freq = 1.0 / (loopDurationSum/(double)loopCounter);
			diagnostics_pub.publish(diagnosticsmsg);
		}

		// Leds Pattern
		if (leds!=NULL) {
			teresa->setLeds(leds->getLeds());
			leds->update();
		}
		first_time=false;
		r.sleep();	
		ros::spinOnce();
		loopDurationSum += (ros::Time::now() - current_time).toSec();
		loopCounter++;
		
	}	

}

}

#endif

