// -*-c++-*-

/*!
  \file cooperative_action.cpp
  \brief cooperative action type Source File.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cooperative_action.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
CooperativeAction::CooperativeAction( const ActionCategory & category,
                                      const int player_unum,
                                      const Vector2D & target_point,
                                      const int duration_step,
                                      const char * description )
    : M_category( category ),
      M_index( -1 ),
      M_player_unum( player_unum ),
      M_target_player_unum( Unum_Unknown ),
      M_target_point( target_point ),
      M_first_ball_speed( 0.0 ),
      M_first_turn_moment( 0.0 ),
      M_first_dash_power( 0.0 ),
      M_first_dash_angle( 0.0 ),
      M_duration_step( duration_step ),
      M_kick_count( 0 ),
      M_turn_count( 0 ),
      M_dash_count( 0 ),
      M_final_action( false ),
      M_description( description )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setCategory( const ActionCategory & category )
{
    M_category = category;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setPlayerUnum( const int unum )
{
    M_player_unum = unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setTargetPlayerUnum( const int unum )
{
    M_target_player_unum = unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setTargetPoint( const Vector2D & point )
{
    M_target_point = point;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setFirstBallSpeed( const double & speed )
{
    M_first_ball_speed = speed;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setFirstTurnMoment( const double & moment )
{
    M_first_turn_moment = moment;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setFirstDashPower( const double & power )
{
    M_first_dash_power = power;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setFirstDashAngle( const AngleDeg & angle )
{
    M_first_dash_angle = angle;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setDurationStep( const int duration_step )
{
    M_duration_step = duration_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setKickCount( const int count )
{
    M_kick_count = count;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setDashCount( const int count )
{
    M_dash_count = count;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setTurnCount( const int count )
{
    M_turn_count = count;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setFinalAction( const bool on )
{
    M_final_action = on;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CooperativeAction::setDescription( const char * description )
{
    M_description = description;
}
