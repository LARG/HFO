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

#include "bhv_their_goal_kick_move.h"

#include "strategy.h"

#include "bhv_set_play.h"

#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/geom/ray_2d.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TheirGoalKickMove::execute( PlayerAgent * agent )
{
    static const Rect2D
        expand_their_penalty
        ( Vector2D( ServerParam::i().theirPenaltyAreaLineX() - 0.75,
                    -ServerParam::i().penaltyAreaHalfWidth() - 0.75 ),
          Size2D( ServerParam::i().penaltyAreaLength() + 0.75,
                  ServerParam::i().penaltyAreaWidth() + 1.5 ) );

    const WorldModel & wm = agent->world();

    if ( doChaseBall( agent ) )
    {
        return true;
    }

    Vector2D intersection;

    if ( wm.ball().vel().r() > 0.2 )
    {
        if ( ! expand_their_penalty.contains( wm.ball().pos() )
             || expand_their_penalty.intersection( Ray2D
                                                   ( wm.ball().pos(),
                                                     wm.ball().vel().th() ),
                                                   &intersection, NULL ) != 1
             )
        {
            doNormal( agent );
            return true;
        }
    }
    else
    {
        if ( wm.ball().pos().x > ServerParam::i().theirPenaltyAreaLineX() + 7.0
             && wm.ball().pos().absY() < ServerParam::i().goalAreaHalfWidth() + 2.0 )
        {
            doNormal( agent );
            return true;
        }

        intersection.x = ServerParam::i().theirPenaltyAreaLineX() - 0.76;
        intersection.y = wm.ball().pos().y;
    }


    dlog.addText( Logger::TEAM,
                  __FILE__": penalty area intersection (%.1f %,1f)",
                  intersection.x, intersection.y );

    double min_dist = 100.0;
    wm.getTeammateNearestTo( intersection, 10, &min_dist );
    if ( min_dist < wm.self().pos().dist( intersection ) )
    {
        doNormal( agent );
        return true;
    }

    double dash_power
        = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() * 0.8 );

    if ( intersection.x < ServerParam::i().theirPenaltyAreaLineX()
         && wm.self().pos().x > ServerParam::i().theirPenaltyAreaLineX() - 0.5 )
    {
        intersection.y = ServerParam::i().penaltyAreaHalfWidth() - 0.5;
        if ( wm.self().pos().y < 0.0 ) intersection.y *= -1.0;
    }
    else if ( intersection.y > ServerParam::i().penaltyAreaHalfWidth()
              && wm.self().pos().absY() < ServerParam::i().penaltyAreaHalfWidth() + 0.5 )
    {
        intersection.y = ServerParam::i().penaltyAreaHalfWidth() + 0.5;
        if ( wm.self().pos().y < 0.0 ) intersection.y *= -1.0;
    }

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    if ( ! Body_GoToPoint( intersection,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }
    agent->setNeckAction( new Neck_ScanField() );
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_TheirGoalKickMove::doNormal( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    // attract to ball
    if ( target_point.x > 25.0
         && ( target_point.y * wm.ball().pos().y < 0.0
              || target_point.absY() < 10.0 )
         )
    {
        double ydiff = wm.ball().pos().y - target_point.y;
        target_point.y += ydiff * 0.4;
    }

    // check penalty area
    if ( wm.self().pos().x > ServerParam::i().theirPenaltyAreaLineX()
         && target_point.absY() < ServerParam::i().penaltyAreaHalfWidth() )
    {
        target_point.y = ServerParam::i().penaltyAreaHalfWidth() + 0.5;
        if ( wm.self().pos().y < 0.0 ) target_point.y *= -1.0;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": doNormal target_point(%.1f %,1f)",
                  target_point.x, target_point.y );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;
    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }
    agent->setNeckAction( new Neck_TurnToBallOrScan() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_TheirGoalKickMove::doChaseBall( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.ball().vel().r() < 0.2 )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();

    if ( self_min > 10 )
    {
        return false;
    }

    Vector2D get_pos = wm.ball().inertiaPoint( self_min );

    const double pen_x = ServerParam::i().theirPenaltyAreaLineX() - 1.0;
    const double pen_y = ServerParam::i().penaltyAreaHalfWidth() + 1.0;
    const Rect2D their_penalty
        ( Vector2D( pen_x, -pen_y ),
          Size2D( ServerParam::i().penaltyAreaLength() + 1.0,
                  ServerParam::i().penaltyAreaWidth() - 2.0 ) );
    if ( their_penalty.contains( get_pos ) )
    {
        return false;
    }

    if ( get_pos.x > pen_x
         && wm.self().pos().x < pen_x
         && wm.self().pos().absY() < pen_y - 0.5 )
    {
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": doChaseBall. cycle = %d  get_pos(%.2f %.2f)",
                  self_min, get_pos.x, get_pos.y );

    // can chase !!
    agent->debugClient().addMessage( "GKickGetBall" );
    Body_Intercept().execute( agent );
    agent->setNeckAction( new Neck_TurnToBallOrScan() );
    return true;
}
