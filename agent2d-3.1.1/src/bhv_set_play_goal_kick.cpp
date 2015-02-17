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

#include "bhv_set_play_goal_kick.h"

#include "strategy.h"

#include "bhv_chain_action.h"
#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_pass.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include <rcsc/geom/rect_2d.h>

#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayGoalKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayGoalKick" );

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
Bhv_SetPlayGoalKick::doKick( PlayerAgent * agent )
{
    if ( doSecondKick( agent ) )
    {
        return;
    }

    // go to ball
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKick) go to ball" );
        // tring to reach the ball yet
        return;
    }

    // already ball point

    if ( doKickWait( agent ) )
    {
        return;
    }

    if ( doPass( agent ) )
    {
        return;
    }

    if ( doKickToFarSide( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();


    int real_set_play_count = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );
    if ( real_set_play_count <= ServerParam::i().dropBallTime() - 10 )
    {
        agent->debugClient().addMessage( "GoalKick:FinalWait%d", real_set_play_count );
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    agent->debugClient().addMessage( "GoalKick:Clear" );
    dlog.addText( Logger::TEAM,
                  __FILE__": clear ball" );

    Body_ClearBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doSecondKick( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < -ServerParam::i().pitchHalfLength() + ServerParam::i().goalAreaLength() + 1.0
         && wm.ball().pos().absY() < ServerParam::i().goalAreaWidth() * 0.5 + 1.0 )
         //|| wm.ball().vel().r2() < std::pow( 0.3, 2 ) )
    {
        return false;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": (doSecondKick) ball is moving." );

    if ( wm.self().isKickable() )
    {
        if ( doPass( agent ) )
        {
            return true;
        }

        agent->debugClient().addMessage( "GoalKick:Clear" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doSecondKick) clear ball" );

        Body_ClearBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( doIntercept( agent ) )
    {
        return true;
    }

    Vector2D ball_final = wm.ball().inertiaFinalPoint();

    agent->debugClient().addMessage( "GoalKick:GoTo" );
    agent->debugClient().setTarget( ball_final );
    dlog.addText( Logger::TEAM,
                  __FILE__": (doSecondKick) go to ball final point(%.2f %.2f)",
                  ball_final.x, ball_final.y );

    if ( ! Body_GoToPoint( ball_final,
                           2.0,
                           ServerParam::i().maxDashPower() ).execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doSecondKick) turn to center" );
        Body_TurnToPoint( Vector2D( 0.0, 0.0 ) ).execute( agent );
    }

    agent->setNeckAction( new Neck_ScanField() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const int real_set_play_count
        = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

    if ( real_set_play_count >= ServerParam::i().dropBallTime() - 10 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) real set play count = %d > drop_time-10, no wait",
                      real_set_play_count );
        return false;
    }

    if ( Bhv_SetPlay::is_delaying_tactics_situation( agent ) )
    {
        agent->debugClient().addMessage( "GoalKick:Delaying" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) delaying" );

        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    // face to the ball
    if ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() > 3.0 )
    {
        agent->debugClient().addMessage( "GoalKick:TurnToBall" );
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() <= 6 )
    {
        agent->debugClient().addMessage( "GoalKick:Wait%d", wm.setplayCount() );
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() <= 30
         && wm.teammatesFromSelf().empty() )
    {
        agent->debugClient().addMessage( "GoalKick:NoTeammate%d", wm.setplayCount() );
        Body_TurnToBall().execute( agent );
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

    if ( wm.setplayCount() <= 3
         || wm.seeTime() != wm.time()
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9 )
    {
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );

        agent->debugClient().addMessage( "GoalKick:Wait%d", wm.setplayCount() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no see or recover" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doPass( PlayerAgent * agent )
{
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return true;
    }

    // const WorldModel & wm = agent->world();

    // Vector2D target_point;
    // double ball_speed = 0.0;
    // if  ( Body_Pass::get_best_pass( wm,
    //                                 &target_point,
    //                                 &ball_speed,
    //                                 NULL )
    //       && target_point.dist( Vector2D(-50.0, 0.0) ) > 20.0
    //       )
    // {
    //     double opp_dist = 100.0;
    //     const PlayerObject * opp = wm.getOpponentNearestTo( target_point,
    //                                                         10,
    //                                                         &opp_dist );
    //     if ( ! opp
    //          || opp_dist > 5.0 )
    //     {
    //         ball_speed = std::min( ball_speed, wm.self().kickRate() * ServerParam::i().maxPower() );

    //         dlog.addText( Logger::TEAM,
    //                       __FILE__": pass to (%.1f, %.1f) ball_speed=%.3f",
    //                       target_point.x, target_point.y,
    //                       ball_speed );

    //         agent->debugClient().addMessage( "GoalKick:Pass%.3f", ball_speed );
    //         agent->debugClient().setTarget( target_point );
    //         agent->debugClient().addLine( wm.ball().pos(), target_point );

    //         Body_KickOneStep( target_point,
    //                           ball_speed ).execute( agent );
    //         if ( agent->effector().queuedNextBallKickable() )
    //         {
    //             agent->setNeckAction( new Neck_ScanField() );
    //         }
    //         else
    //         {
    //             agent->setNeckAction( new Neck_TurnToBall() );
    //         }

    //         return true;
    //     }

    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": pass (%.1f, %.1f) found. but opponent exists.",
    //                   target_point.x, target_point.y );
    // }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doIntercept( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.ball().pos().x < -ServerParam::i().pitchHalfLength() + ServerParam::i().goalAreaLength() + 1.0
         && wm.ball().pos().absY() < ServerParam::i().goalAreaWidth() * 0.5 + 1.0 )
    {
        return false;
    }

    if ( wm.self().isKickable() )
    {
        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    int mate_min = wm.interceptTable()->teammateReachCycle();
    if ( self_min > mate_min )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) other ball kicker" );
        return false;
    }

    Vector2D trap_pos = wm.ball().inertiaPoint( self_min );
    if ( ( trap_pos.x > ServerParam::i().ourPenaltyAreaLineX() - 8.0
           && trap_pos.absY() > ServerParam::i().penaltyAreaHalfWidth() - 5.0 )
         || wm.ball().vel().r2() < std::pow( 0.5, 2 ) )
    {
        agent->debugClient().addMessage( "GoalKick:Intercept" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doIntercept) intercept" );

        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayGoalKick::doMove( PlayerAgent * agent )
{
    if ( doIntercept( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    double dash_power = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    target_point.y += wm.ball().pos().y * 0.5;
    if ( target_point.absY() > ServerParam::i().pitchHalfWidth() - 1.0 )
    {
        target_point.y = sign( target_point.y ) * ( ServerParam::i().pitchHalfWidth() - 1.0 );
    }

    if ( wm.self().stamina() > ServerParam::i().staminaMax() * 0.9 )
    {
        const PlayerObject * nearest_opp = wm.getOpponentNearestToSelf( 5 );

        if ( nearest_opp
             && nearest_opp->distFromSelf() < 3.0 )
        {
            Vector2D add_vec = ( wm.ball().pos() - target_point );
            add_vec.setLength( 3.0 );

            long time_val = wm.time().cycle() % 60;
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

    agent->debugClient().addMessage( "GoalKickMove" );
    agent->debugClient().setTarget( target_point );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    if ( wm.self().pos().dist( target_point ) > wm.ball().pos().dist( target_point ) * 0.2 + 6.0
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.7 )
    {
        if ( ! wm.self().staminaModel().capacityIsEmpty() )
        {
            agent->debugClient().addMessage( "Sayw" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayGoalKick::doKickToFarSide( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point( ServerParam::i().ourPenaltyAreaLineX() - 5.0,
                           ServerParam::i().penaltyAreaHalfWidth() );
    if ( wm.ball().pos().y > 0.0 )
    {
        target_point.y *= -1.0;
    }

    double ball_move_dist = wm.ball().pos().dist( target_point );
    double ball_first_speed = calc_first_term_geom_series_last( 0.7,
                                                                ball_move_dist,
                                                                ServerParam::i().ballDecay() );
    ball_first_speed = std::min( ServerParam::i().ballSpeedMax(),
                                 ball_first_speed );
    ball_first_speed = std::min( wm.self().kickRate() * ServerParam::i().maxPower(),
                                 ball_first_speed );

    Vector2D accel = target_point - wm.ball().pos();
    accel.setLength( ball_first_speed );

    double kick_power = std::min( ServerParam::i().maxPower(),
                                  accel.r() / wm.self().kickRate() );
    AngleDeg kick_angle = accel.th();


    dlog.addText( Logger::TEAM,
                  __FILE__" (doKickToFarSide) target=(%.2f %.2f) dist=%.3f ball_speed=%.3f",
                  target_point.x, target_point.y,
                  ball_move_dist,
                  ball_first_speed );
    dlog.addText( Logger::TEAM,
                  __FILE__" (doKickToFarSide) kick_power=%f kick_angle=%.1f",
                  kick_power,
                  kick_angle.degree() );

    agent->doKick( kick_power, kick_angle - wm.self().body() );

    agent->setNeckAction( new Neck_ScanField() );
    return true;
}
