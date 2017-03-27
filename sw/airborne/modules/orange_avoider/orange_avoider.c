/*
 * Copyright (C) Roland Meertens
 *
 * This file is part of paparazzi
 *
 */
/**
 * @file "modules/orange_avoider/orange_avoider.c"
 * @author Roland Meertens
 * Example on how to use the colours detected to avoid orange pole in the cyberzoo
 */

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "firmwares/rotorcraft/navigation.h"

#include "generated/flight_plan.h"
#include "modules/computer_vision/colorfilter.h"
#include "modules/orange_avoider/orange_avoider.h"

#define ORANGE_AVOIDER_VERBOSE TRUE

#define PRINT(string,...) fprintf(stderr, "[orange_avoider->%s()] " string,__FUNCTION__ , ##__VA_ARGS__)
#if ORANGE_AVOIDER_VERBOSE
#define VERBOSE_PRINT PRINT
#else
#define VERBOSE_PRINT(...)
#endif

uint8_t safeToGoForwards        = false;
//uint8_t safeToGoForwardsO       = false, safeToGoForwardsB       = false;
int thresholdColorCountO        = 0.015 * 124800/3; // 520 x 240 = 124.800 total pixels
int thresholdColorCountB        = 0.06 * 124800/3; // 520 x 240 = 124.800 total pixels
float incrementForAvoidance;
uint16_t trajectoryConfidence   = 1;
float maxDistance               = 2.25;

/*
 * Initialisation function, setting the colour filter, random seed and incrementForAvoidance
 */
void orange_avoider_init()
{
  // Initialise the variables of the colorfilter to accept black
  color_lum_minB = 10;
  color_lum_maxB = 18;
  color_cb_minB  = 127;
  color_cb_maxB  = 150;
  color_cr_minB  = 127;
  color_cr_maxB  = 150;
  // Initialise the variables of the colorfilter to accept orange
  color_lum_minO = 20;
  color_lum_maxO = 255;
  color_cb_minO  = 75;
  color_cb_maxO  = 145;
  color_cr_minO  = 155;
  color_cr_maxO  = 255;
  // Initialise random values
  srand(time(NULL));
  //chooseRandomIncrementAvoidance();
}

/*
 * Function that checks it is safe to move forwards, and then moves a waypoint forward or changes the heading
 */
void orange_avoider_periodic()
{
  // Check the amount of orange. If this is above a threshold
  // you want to turn a certain amount of degrees

  /*  if (nC <= threshold_count) { 
	heading_decision = 0; 		// go straight
    } else {
	if (nR > nL) {
	    if (nL <= threshold_count) {
		heading_decision = -1; 	// turn left
	    } else {
		heading_decision = -2; 	// trun sharply left
	    }
	} else {
	    if (nR <= threshold_count) {
		heading_decision = 1; 	// turn right
	    } else {
		heading_decision = 2; 	// trun sharply right
	    }
	}
    }*/
 // safeToGoForwards = (color_countOc+color_countBc) < tresholdColorCount;
  safeToGoForwards = color_countOc < thresholdColorCountO && color_countBc < thresholdColorCountB;
 // VERBOSE_PRINT("Color_count orange: %d  threshold: %d safe: %d \n", color_countOc, tresholdColorCount, safeToGoForwards);
  //VERBOSE_PRINT("Hola");
  float moveDistance = fmin(maxDistance, 0.04 * trajectoryConfidence);
  if(safeToGoForwards){
      moveWaypointForward(WP_GOAL, moveDistance);
      moveWaypointForward(WP_TRAJECTORY, 1.25 * moveDistance);
      nav_set_heading_towards_waypoint(WP_GOAL);
      //chooseRandomIncrementAvoidance();
      VERBOSE_PRINT("Safe to go fwd \n");
      incrementForAvoidance = 0.0;
      trajectoryConfidence += 1;
  }
  else{
      waypoint_set_here_2d(WP_GOAL);
      waypoint_set_here_2d(WP_TRAJECTORY);
      chooseRandomIncrementAvoidance();
      increase_nav_heading(&nav_heading, incrementForAvoidance);
      VERBOSE_PRINT("Turn \n");
      if(trajectoryConfidence > 5){
          trajectoryConfidence -= 4;
      } else{
          trajectoryConfidence = 1;
      }
  }
  return;
}

/*
 * Increases the NAV heading. Assumes heading is an INT32_ANGLE. It is bound in this function.
 */
uint8_t increase_nav_heading(int32_t *heading, float incrementDegrees)
{
  struct Int32Eulers *eulerAngles   = stateGetNedToBodyEulers_i();
  int32_t newHeading = eulerAngles->psi + ANGLE_BFP_OF_REAL( incrementDegrees / 180.0 * M_PI);
  // Check if your turn made it go out of bounds...
  INT32_ANGLE_NORMALIZE(newHeading); // HEADING HAS INT32_ANGLE_FRAC....
  *heading = newHeading;
  //VERBOSE_PRINT("Increasing heading to %f\n", ANGLE_FLOAT_OF_BFP(*heading) * 180 / M_PI);
  return false;
}

/*
 * Calculates coordinates of a distance of 'distanceMeters' forward w.r.t. current position and heading
 */
uint8_t calculateForwards(struct EnuCoor_i *new_coor, float distanceMeters)
{
  struct EnuCoor_i *pos             = stateGetPositionEnu_i(); // Get your current position
  struct Int32Eulers *eulerAngles   = stateGetNedToBodyEulers_i();
  // Calculate the sine and cosine of the heading the drone is keeping
  float sin_heading                 = sinf(ANGLE_FLOAT_OF_BFP(eulerAngles->psi));
  float cos_heading                 = cosf(ANGLE_FLOAT_OF_BFP(eulerAngles->psi));
  // Now determine where to place the waypoint you want to go to
  new_coor->x                       = pos->x + POS_BFP_OF_REAL(sin_heading * (distanceMeters));
  new_coor->y                       = pos->y + POS_BFP_OF_REAL(cos_heading * (distanceMeters));
 // VERBOSE_PRINT("Calculated %f m forward position. x: %f  y: %f based on pos(%f, %f) and heading(%f)\n", distanceMeters, POS_FLOAT_OF_BFP(new_coor->x), POS_FLOAT_OF_BFP(new_coor->y), POS_FLOAT_OF_BFP(pos->x), POS_FLOAT_OF_BFP(pos->y), ANGLE_FLOAT_OF_BFP(eulerAngles->psi)*180/M_PI);
  return false;
}

/*
 * Sets waypoint 'waypoint' to the coordinates of 'new_coor'
 */
uint8_t moveWaypoint(uint8_t waypoint, struct EnuCoor_i *new_coor)
{
 // VERBOSE_PRINT("Moving waypoint %d to x:%f y:%f\n", waypoint, POS_FLOAT_OF_BFP(new_coor->x), POS_FLOAT_OF_BFP(new_coor->y));
  waypoint_set_xy_i(waypoint, new_coor->x, new_coor->y);
  return false;
}

/*
 * Calculates coordinates of distance forward and sets waypoint 'waypoint' to those coordinates
 */
uint8_t moveWaypointForward(uint8_t waypoint, float distanceMeters)
{
  struct EnuCoor_i new_coor;
  calculateForwards(&new_coor, distanceMeters);
  moveWaypoint(waypoint, &new_coor);
  return false;
}

/*
 * Sets the variable 'incrementForAvoidance' randomly positive/negative
 */
uint8_t chooseRandomIncrementAvoidance()
{

uint16_t nR = 0, nL = 0;

/*if (color_countOr<color_countBr){
   nR = color_countBr;
} else {
   nR = color_countOr;
}
if (color_countOl<color_countBl){
   nL = color_countBl;
} else {
   nL = color_countOl;
}*/
nR = color_countBr + color_countOr;
nL = color_countBl + color_countOl;

incrementForAvoidance = 2.0; // Initial value

 // See where there is lees point (left or right)
/*if (nL>nR) {
   if (nR < tresholdColorCount){
	incrementForAvoidance = 10.0; // turn right
VERBOSE_PRINT("Left larger that right1 \n");
   } else {
	incrementForAvoidance = 40.0; // sharp turn right
VERBOSE_PRINT("Left larger that right2 \n");
   }
} else {
   if (nL < tresholdColorCount){
	incrementForAvoidance = -10.0; // turn left
VERBOSE_PRINT("Left smaller that right1 \n");
   } else {
	incrementForAvoidance = -40.0; // sharp turn left
VERBOSE_PRINT("Left smaller that right2 \n");
   }
}*/

if(color_countOr<thresholdColorCountO && color_countOr<color_countOl ){ //&& 
   //color_countBr<thresholdColorCountB && color_countBr<color_countBl){
	incrementForAvoidance = 10.0;
} else {
   if(color_countOl<thresholdColorCountO && color_countBl<thresholdColorCountB){
	incrementForAvoidance = -10.0;
   } else {
      if(nR > nL){
	incrementForAvoidance = -40.0;
      } else {
	incrementForAvoidance = 40.0;
}}}
VERBOSE_PRINT("Save to go forwards = %d \n", safeToGoForwards);

VERBOSE_PRINT("Threshold count Orange = %d // Threshold count Black = %d \n", thresholdColorCountO, thresholdColorCountB);
VERBOSE_PRINT("Heading angle = %f \n", incrementForAvoidance);
VERBOSE_PRINT("Orange: left = %d,	center = %d,	right =  %d \n", color_countOl, color_countOc, color_countOr);
VERBOSE_PRINT("Black:  left = %d,	center = %d,	right =  %d \n", color_countBl, color_countBc, color_countBr);

/* // Randomly choose CW or CCW avoiding direction
  int r = rand() % 2;
  if (r == 0) {
    incrementForAvoidance = 10.0;
 //   VERBOSE_PRINT("Set avoidance increment to: %f\n", incrementForAvoidance);
  } else {
    incrementForAvoidance = -10.0;
  //  VERBOSE_PRINT("Set avoidance increment to: %f\n", incrementForAvoidance);
  }*/
  return false;
}

