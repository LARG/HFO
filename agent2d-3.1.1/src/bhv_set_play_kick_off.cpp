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

#include "bhv_set_play_kick_off.h"

#include "strategy.h"

#include "bhv_go_to_static_ball.h"
#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"

#include <rcsc/action/body_smart_kick.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
//#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayKickOff::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PlayerPtrCont & teammates = wm.teammatesFromBall();


    if ( teammates.empty()
         || teammates.front()->distFromBall() > wm.self().distFromBall() )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayKickOff::doKick( PlayerAgent * agent )
{
    //
    // go to the ball position
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        return;
    }

    //
    // wait
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // kick
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = ServerParam::i().maxPower() * wm.self().kickRate();

    Vector2D target_point;
    double ball_speed = max_ball_speed;

            // teammate not found
    if ( wm.teammatesFromSelf().empty() )
    {
        target_point.assign( ServerParam::i().pitchHalfLength(),
                             static_cast< double >
                             ( -1 + 2 * wm.time().cycle() % 2 )
                             * 0.8 * ServerParam::i().goalHalfWidth() );
        dlog.addText( Logger::TEAM,
                      __FILE__": no teammate. target=(%.1f %.1f)",
                      target_point.x, target_point.y );
    }
    else
    {
        const PlayerObject * teammate = wm.teammatesFromSelf().front();
        double dist = teammate->distFromSelf();

        if ( dist > 35.0 )
        {
            // too far
            target_point.assign( ServerParam::i().pitchHalfLength(),
                                 static_cast< double >
                                 ( -1 + 2 * wm.time().cycle() % 2 )
                                 * 0.8 * ServerParam::i().goalHalfWidth() );
            dlog.addText( Logger::TEAM,
                          __FILE__": nearest teammate is too far. target=(%.1f %.1f)",
                          target_point.x, target_point.y );
        }
        else
        {
            target_point = teammate->inertiaFinalPoint();

            ball_speed = std::min( max_ball_speed,
                                   calc_first_term_geom_series_last( 1.8,
                                                                     dist,
                                                                     ServerParam::i().ballDecay() ) );
            dlog.addText( Logger::TEAM,
                          __FILE__": nearest teammate %d target=(%.1f %.1f) speed=%.3f",
                          teammate->unum(),
                          target_point.x, target_point.y,
                          ball_speed );
        }
    }

    Vector2D ball_vel = Vector2D::polar2vector( ball_speed,
                                                ( target_point - wm.ball().pos() ).th() );
    Vector2D ball_next = wm.ball().pos() + ball_next;
    while ( wm.self().pos().dist( ball_next ) < wm.self().playerType().kickableArea() + 0.2 )
    {
        ball_vel.setLength( ball_speed + 0.1 );
        ball_speed += 0.1;
        ball_next = wm.ball().pos() + ball_vel;

        dlog.addText( Logger::TEAM,
                      __FILE__": kickable in next cycle. adjust ball speed to %.3f",
                      ball_speed );
    }

    ball_speed = std::min( ball_speed, max_ball_speed );

    agent->debugClient().setTarget( target_point );

    // enforce one step kick
    Body_SmartKick( target_point,
                    ball_speed,
                    ball_speed * 0.96,
                    1 ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Bhv_SetPlayKickOff::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int real_set_play_count
        = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

    if ( real_set_play_count >= ServerParam::i().dropBallTime() - 5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) real set play count = %d > drop_time-10, force kick mode",
                      real_set_play_count );
        return false;
    }

    if ( Bhv_SetPlay::is_delaying_tactics_situation( agent ) )
    {
        agent->debugClient().addMessage( "KickOff:Delaying" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) delaying" );

        Body_TurnToAngle( 180.0 ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.self().body().abs() < 175.0 )
    {
        agent->debugClient().addMessage( "KickOff:Turn" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) turn" );

        Body_TurnToAngle( 180.0 ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromBall().empty() )
    {
        agent->debugClient().addMessage( "KickOff:NoTeammate" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        Body_TurnToAngle( 180.0 ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromSelf().size() < 9 )
    {
        agent->debugClient().addMessage( "FreeKick:Wait%d", real_set_play_count );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) wait..." );

        Body_TurnToAngle( 180.0 ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.seeTime() != wm.time()
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9 )
    {
        agent->debugClient().addMessage( "KickOff:WaitX" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no see or recover" );

        //Body_TurnToBall().execute( agent );
        Body_TurnToAngle( 180.0 ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
Bhv_SetPlayKickOff::doMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    target_point.x = std::min( -0.5, target_point.x );

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }
    agent->setNeckAction( new Neck_ScanField() );

    agent->debugClient().setTarget( target_point );
}
