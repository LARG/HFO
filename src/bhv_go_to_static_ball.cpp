// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_go_to_static_ball.h"

#include "bhv_set_play.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_GoToStaticBall::execute( PlayerAgent * agent )
{
    const double dir_margin = 15.0;

    const WorldModel & wm = agent->world();

    AngleDeg angle_diff = wm.ball().angleFromSelf() - M_ball_place_angle;

    if ( angle_diff.abs() < dir_margin
         && wm.ball().distFromSelf() < ( wm.self().playerType().playerSize()
                                         + ServerParam::i().ballSize()
                                         + 0.08 )
         )
    {
        // already reach
        return false;
    }

    // decide sub-target point
    Vector2D sub_target = wm.ball().pos()
                          + Vector2D::polar2vector( 2.0, M_ball_place_angle + 180.0 );

    double dash_power = 20.0;
    double dash_speed = -1.0;
    if ( wm.ball().distFromSelf() > 2.0 )
    {
        dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    }
    else
    {
        dash_speed = wm.self().playerType().playerSize();
        dash_power = wm.self().playerType().getDashPowerToKeepSpeed( dash_speed, wm.self().effort() );
    }

    // it is necessary to go to sub target point
    if ( angle_diff.abs() > dir_margin )
    {
        dlog.addText( Logger::TEAM,
                            __FILE__": go to sub-target(%.1f, %.1f)",
                            sub_target.x, sub_target.y );
        Body_GoToPoint( sub_target,
                        0.1,
                        dash_power,
                        dash_speed ).execute( agent );
    }
    // dir diff is small. go to ball
    else
    {
        // body dir is not right
        if ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() > 1.5 )
        {
            dlog.addText( Logger::TEAM,
                                __FILE__": turn to ball" );
            Body_TurnToBall().execute( agent );
        }
        // dash to ball
        else
        {
            dlog.addText( Logger::TEAM,
                                __FILE__": dash to ball" );
            agent->doDash( dash_power );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );

    return true;
}
