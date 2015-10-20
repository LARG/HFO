// -*-c++-*-

/*!
  \file neck_offensive_intercept_neck.cpp
  \brief neck action for an offensive intercept situation
*/

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "neck_offensive_intercept_neck.h"

#include "shoot_generator.h"

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/action/neck_turn_to_ball.h>
#include <rcsc/action/neck_turn_to_ball_and_player.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_goalie_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>
#include <rcsc/action/neck_turn_to_point.h>
#include <rcsc/action/view_wide.h>
#include <rcsc/action/view_normal.h>
#include <rcsc/action/view_synch.h>

#include "neck_default_intercept_neck.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_OffensiveInterceptNeck::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.existKickableOpponent() )
    {
        const PlayerObject * opp = wm.interceptTable()->fastestOpponent();
        if ( opp )
        {
            Neck_TurnToBallAndPlayer( opp ).execute( agent );
            dlog.addText( Logger::TEAM,
                          __FILE__": ball and player" );
            return true;
        }
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    //const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const ViewWidth next_view_width = agent->effector().queuedNextViewWidth();

    bool ball_safety = true;
    bool ball_in_visible = false;

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    const Vector2D next_ball_pos = agent->effector().queuedNextBallPos();

    if ( next_self_pos.dist( next_ball_pos ) < ServerParam::i().visibleDistance() - 0.5 )
    {
        ball_in_visible = true;

        if ( opp_min <= 1 )
        {
            ball_safety = false;
            ball_in_visible = false;
        }
        else
        {
            const PlayerObject * nearest_opp = wm.getOpponentNearestTo( next_ball_pos,
                                                                        10,
                                                                        NULL );
            if ( nearest_opp
                 && next_ball_pos.dist( nearest_opp->pos() + nearest_opp->vel() )
                 < nearest_opp->playerTypePtr()->kickableArea() + 0.3 )
            {
                ball_safety = false;
                ball_in_visible = false;
            }
        }
    }

    AngleDeg ball_angle = agent->effector().queuedNextAngleFromBody( next_ball_pos );
    double angle_diff = ball_angle.abs() - ServerParam::i().maxNeckAngle();

    ViewAction * view = static_cast< ViewAction * >( 0 );

    const double trap_ball_speed = wm.ball().vel().r() * std::pow( ServerParam::i().ballDecay(), 5 );

    if ( self_min == 4 && opp_min >= 3 )
    {
        if ( wm.ball().seenPosCount() == 0
             || trap_ball_speed < wm.self().playerType().kickableArea() )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": set wide. trap_ball_speed=%.3f",
                          trap_ball_speed );
            view = new View_Wide();
        }
    }

    if ( ! view
         && self_min == 3 && opp_min >= 2 )
    {
        if ( wm.ball().seenPosCount() <= 1
             || trap_ball_speed < wm.self().playerType().kickableArea() )
        {
            view = new View_Normal();
            dlog.addText( Logger::TEAM,
                          __FILE__": set normal. trap_ball_speed=%.3f",
                          trap_ball_speed );
        }
    }

    if ( ! view
         && ! ball_in_visible
         && self_min >= 4
         && angle_diff > ViewWidth::width( ViewWidth::NORMAL ) * 0.5 - 3.0 )
    {
        view = new View_Wide();
        dlog.addText( Logger::TEAM,
                      __FILE__": set wide for ball check" );
    }

    if ( ! view
         && ! ball_in_visible
         && self_min >= 3
         && angle_diff > ViewWidth::width( ViewWidth::NARROW ) * 0.5 - 3.0 )
    {
        view = new View_Normal();
        dlog.addText( Logger::TEAM,
                      __FILE__": set normal for ball check" );
    }

    if ( ! view )
    {
        if ( self_min > 15 )
        {
            view = new View_Normal();
            dlog.addText( Logger::TEAM,
                          __FILE__": set wide (2)" );
        }
        else if ( self_min > 10 )
        {
            view = new View_Normal();
            dlog.addText( Logger::TEAM,
                          __FILE__": set normal (2)" );
        }
        else if ( self_min <= 6 )
        {
            view = new View_Synch();
            dlog.addText( Logger::TEAM,
                          __FILE__": set synch" );
        }
    }

    //
    //
    //

    if ( doTurnNeckToShootPoint( agent ) )
    {
        return true;
    }

    //
    //
    //

    if ( ! ActionChainHolder::instance().initialized() )
    {
        return false;
    }

    if ( self_min == 5
         && trap_ball_speed > wm.self().playerType().kickableArea() * 1.7 )
    {
        agent->debugClient().addMessage( "OffenseNeck:ForceBall" );
        dlog.addText( Logger::TEAM,
                      __FILE__": look ball" );
        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToBall() ).execute( agent );
    }
    else if ( next_ball_pos.x > 34.0
              && next_ball_pos.absY() < 17.0
              && ( ball_in_visible
                   || ( wm.ball().posCount() <= 1
                        && ball_safety ) )
              )
    {
        int count_thr = 0;

        if ( ServerParam::i().theirTeamGoalPos().dist( next_ball_pos ) < 13.0
             && next_ball_pos.x > ServerParam::i().pitchHalfLength() - 7.0
             && next_ball_pos.absY() > ServerParam::i().goalHalfWidth() + 3.0 )
        {
            count_thr = -1;
        }

        agent->debugClient().addMessage( "OffenseNeck:Goalie" );
        dlog.addText( Logger::TEAM,
                      __FILE__": default neck: goalie or scan. count_thr=%d",
                      count_thr );

        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToGoalieOrScan( count_thr ) ).execute( agent );

    }
    //else if ( ball_in_visible )
    else if ( wm.ball().posCount() <= 1 )
    {
        agent->debugClient().addMessage( "OffenseNeck:Teammate" );
        dlog.addText( Logger::TEAM,
                      __FILE__": default neck: low conf teammate" );
        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToLowConfTeammate() ).execute( agent );
    }
    // TODO:
    // else if ( self_min == 5 && can_look_ball_with_current_view )
    //    Neck_DefaultInterceptNeck( view, new Neck_TurnToBall() ).execute();
    //
    else
    {
        agent->debugClient().addMessage( "OffenseNeck:Ball" );
        dlog.addText( Logger::TEAM,
                      __FILE__": default neck: ball or scan" );
        int count_thr = 0;
        if ( self_min <= 5
             && wm.ball().seenPosCount() >= 2
             && ( next_view_width == ViewWidth::WIDE
                  || next_view_width == ViewWidth::NORMAL
                  || wm.ball().seenVelCount() >= 2 ) )
        {
            count_thr = -1;
        }

        Neck_DefaultInterceptNeck( view,
                                   new Neck_TurnToBallOrScan( count_thr ) ).execute( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_OffensiveInterceptNeck::doTurnNeckToShootPoint( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const int self_min = wm.interceptTable()->selfReachCycle();

    if ( self_min > 1 )
    {
        return false;
    }

    const ShootGenerator::Container & cont = ShootGenerator::instance().courses( wm );

    if ( cont.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": empty shoot candidate" );
        return false;
    }

    ShootGenerator::Container::const_iterator best_shoot
        = std::min_element( cont.begin(),
                            cont.end(),
                            ShootGenerator::ScoreCmp() );

    if ( best_shoot == cont.end() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no best shoot?" );
        return false;
    }

    const double angle_buf = 10.0; // Magic Number

    if ( ! agent->effector().queuedNextCanSeeWithTurnNeck( best_shoot->target_point_,
                                                           angle_buf ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": cannot look the shoot point(%.2f %.2f)",
                      best_shoot->target_point_.x,
                      best_shoot->target_point_.y );
        return false;
    }

    if ( wm.seeTime() == wm.time() )
    {
        double current_width = wm.self().viewWidth().width();
        AngleDeg target_angle = ( best_shoot->target_point_ - wm.self().pos() ).th();
        double angle_diff = ( target_angle - wm.self().face() ).abs();

        if ( angle_diff < current_width*0.5 - angle_buf )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": already seen. width=%.1f, diff=%.1f. shoot point(%.2f %.2f)",
                          current_width,
                          angle_diff,
                          best_shoot->target_point_.x,
                          best_shoot->target_point_.y );
            return false;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": turn_neck to the shoot point(%.2f %.2f)",
                  best_shoot->target_point_.x,
                  best_shoot->target_point_.y );
    agent->debugClient().addMessage( "OffenseNeck:Shoot" );

    Neck_TurnToPoint( best_shoot->target_point_ ).execute( agent );

    return true;
}
