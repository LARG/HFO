// -*-c++-*-

/*!
  \file bhv_normal_dribble.h
  \brief normal dribble action
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef BHV_NORMAL_DRIBBLE_H
#define BHV_NORMAL_DRIBBLE_H

#include "cooperative_action.h"

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

/*!
  \class Bhv_NormalDribble
  \brief normal dribble action
*/
class Bhv_NormalDribble
    : public rcsc::SoccerBehavior {
private:
    rcsc::Vector2D M_target_point;

    double M_first_ball_speed;
    double M_first_turn_moment;
    double M_first_dash_power;
    rcsc::AngleDeg M_first_dash_angle;

    int M_total_step;
    int M_kick_step;
    int M_turn_step;
    int M_dash_step;

    rcsc::NeckAction::Ptr M_neck_action;
    rcsc::ViewAction::Ptr M_view_action;

public:

    Bhv_NormalDribble( const CooperativeAction & action,
                       rcsc::NeckAction::Ptr neck = rcsc::NeckAction::Ptr(),
                       rcsc::ViewAction::Ptr view = rcsc::ViewAction::Ptr() );

    /*!
      \brief do dribble
      \param agent agent pointer to the agent itself
      \return true with action, false if can't do dribble
     */
    bool execute( rcsc::PlayerAgent * agent );
};

#endif
