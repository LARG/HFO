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

#include "bhv_set_play_free_kick.h"

#include "strategy.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayFreeKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayFreeKick" );
    //---------------------------------------------------
    if ( Bhv_SetPlay::is_kicker( agent ) )
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
Bhv_SetPlayFreeKick::doKick( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": (doKick)" );
    //
    // go to the ball position
    //
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
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->debugClient().addMessage( "FreeKick:Chain" );
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return;
    }
    // {
    //     Vector2D target_point;
    //     double ball_speed = 0.0;
    //     if ( Body_Pass::get_best_pass( wm,
    //                                    &target_point,
    //                                    &ball_speed,
    //                                    NULL )
    //          && ( target_point.x > -25.0
    //               || target_point.x > wm.ball().pos().x + 10.0 )
    //          && ball_speed < max_ball_speed * 1.1 )
    //     {
    //         ball_speed = std::min( ball_speed, max_ball_speed );
    //         agent->debugClient().addMessage( "FreeKick:Pass%.3f", ball_speed );
    //         agent->debugClient().setTarget( target_point );
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__":  pass target=(%.1f %.1f) speed=%.2f",
    //                       target_point.x, target_point.y,
    //                       ball_speed );
    //         Body_KickOneStep( target_point, ball_speed ).execute( agent );
    //         agent->setNeckAction( new Neck_ScanField() );
    //         return;
    //     }
    // }

    //
    // kick to the nearest teammate
    //
    {
        const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if ( nearest_teammate
             && nearest_teammate->distFromSelf() < 20.0
             && ( nearest_teammate->pos().x > -30.0
                  || nearest_teammate->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point = nearest_teammate->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
            return;
        }
    }

    //
    // clear
    //

    if ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() > 1.5 )
    {
        agent->debugClient().addMessage( "FreeKick:Clear:TurnToBall" );
        dlog.addText( Logger::TEAM,
                      __FILE__":  clear. turn to ball" );

        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    agent->debugClient().addMessage( "FreeKick:Clear" );
    dlog.addText( Logger::TEAM,
                  __FILE__":  clear" );

    Body_ClearBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayFreeKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait)" );


    const int real_set_play_count
        = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

    if ( real_set_play_count >= ServerParam::i().dropBallTime() - 5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) real set play count = %d > drop_time-10, force kick mode",
                      real_set_play_count );
        return false;
    }

    const Vector2D face_point( 40.0, 0.0 );
    const AngleDeg face_angle = ( face_point - wm.self().pos() ).th();

    if ( wm.time().stopped() != 0 )
    {
        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( Bhv_SetPlay::is_delaying_tactics_situation( agent ) )
    {
        agent->debugClient().addMessage( "FreeKick:Delaying" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) delaying" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromBall().empty() )
    {
        agent->debugClient().addMessage( "FreeKick:NoTeammate" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() <= 3 )
    {
        agent->debugClient().addMessage( "FreeKick:Wait%d", wm.setplayCount() );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() >= 15
         && wm.seeTime() == wm.time()
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) set play count = %d, force kick mode",
                      wm.setplayCount() );
        return false;
    }

    if ( ( face_angle - wm.self().body() ).abs() > 5.0 )
    {
        agent->debugClient().addMessage( "FreeKick:Turn" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.seeTime() != wm.time()
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9 )
    {
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );

        agent->debugClient().addMessage( "FreeKick:Wait%d", wm.setplayCount() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no see or recover" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayFreeKick::doMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doMove)" );

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.setplayCount() > 0
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.9 )
    {
        const PlayerObject * nearest_opp = agent->world().getOpponentNearestToSelf( 5 );

        if ( nearest_opp && nearest_opp->distFromSelf() < 3.0 )
        {
            Vector2D add_vec = ( wm.ball().pos() - target_point );
            add_vec.setLength( 3.0 );

            long time_val = agent->world().time().cycle() % 60;
            if ( time_val < 20 )
            {

            }
            else if ( time_val < 40 )
            {
                target_point += add_vec.rotatedVector( 90.0 );
            }
            else
            {
                target_point += add_vec.rotatedVector( -90.0 );
            }

            target_point.x = min_max( - ServerParam::i().pitchHalfLength(),
                                      target_point.x,
                                      + ServerParam::i().pitchHalfLength() );
            target_point.y = min_max( - ServerParam::i().pitchHalfWidth(),
                                      target_point.y,
                                      + ServerParam::i().pitchHalfWidth() );
        }
    }

    target_point.x = std::min( target_point.x,
                               agent->world().offsideLineX() - 0.5 );

    double dash_power
        = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "SetPlayMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    if ( wm.self().pos().dist( target_point )
         > std::max( wm.ball().pos().dist( target_point ) * 0.2, dist_thr ) + 6.0
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.7 )
    {
        if ( ! wm.self().staminaModel().capacityIsEmpty() )
        {
            agent->debugClient().addMessage( "Sayw" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );
}
