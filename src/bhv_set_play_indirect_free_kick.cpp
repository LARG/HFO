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

#include "bhv_set_play_indirect_free_kick.h"

#include "strategy.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_kick_collide_with_ball.h>
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

 */
bool
Bhv_SetPlayIndirectFreeKick::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const bool our_kick = ( ( wm.gameMode().type() == GameMode::BackPass_
                              && wm.gameMode().side() == wm.theirSide() )
                            || ( wm.gameMode().type() == GameMode::IndFreeKick_
                                 && wm.gameMode().side() == wm.ourSide() )
                            || ( wm.gameMode().type() == GameMode::FoulCharge_
                                 && wm.gameMode().side() == wm.theirSide() )
                            || ( wm.gameMode().type() == GameMode::FoulPush_
                                 && wm.gameMode().side() == wm.theirSide() )
                            );

    if ( our_kick )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) our kick" );

        if ( Bhv_SetPlay::is_kicker( agent ) )
        {
            doKicker( agent );
        }
        else
        {
            doOffenseMove( agent );
        }
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (execute) their kick" );

        doDefenseMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doKicker( PlayerAgent * agent )
{
    // go to ball
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
    // kick to the teammate exist at the front of their goal
    //

    if ( doKickToShooter( agent ) )
    {
        return;
    }

    const WorldModel & wm = agent->world();

    const double max_kick_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return;
    }
    // {
    //     const double max_kick_speed = wm.self().kickRate() * ServerParam::i().maxPower();
    //     Vector2D target_point;
    //     double ball_speed = 0.0;
    //     if  ( Body_Pass::get_best_pass( wm,
    //                                     &target_point,
    //                                     &ball_speed,
    //                                     NULL )
    //           && target_point.x > 35.0
    //           && ( target_point.x > wm.self().pos().x - 1.0
    //                || target_point.x > 48.0 ) )
    //     {
    //         ball_speed = std::min( ball_speed, max_kick_speed );
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__":  pass to (%.1f %.1f) speed=%.2f",
    //                       target_point.x, target_point.y,
    //                       ball_speed );
    //         Body_KickOneStep( target_point, ball_speed ).execute( agent );
    //         agent->setNeckAction( new Neck_ScanField() );
    //         return;
    //     }
    // }

    //
    // wait(2)
    //
    if ( wm.setplayCount() <= 3 )
    {
        Body_TurnToPoint( Vector2D( 50.0, 0.0 ) ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    //
    // no teammate
    //
    if ( wm.teammatesFromBall().empty()
         || wm.teammatesFromBall().front()->distFromSelf() > 35.0
         || wm.teammatesFromBall().front()->pos().x < -30.0 )
    {
        const int real_set_play_count
            = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

        if ( real_set_play_count <= ServerParam::i().dropBallTime() - 3 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": (doKick) real set play count = %d <= drop_time-3, wait...",
                          real_set_play_count );
            Body_TurnToPoint( Vector2D( 50.0, 0.0 ) ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
            return;
        }

        Vector2D target_point( ServerParam::i().pitchHalfLength(),
                               static_cast< double >( -1 + 2 * wm.time().cycle() % 2 )
                               * ( ServerParam::i().goalHalfWidth() - 0.8 ) );
        double ball_speed = max_kick_speed;

        agent->debugClient().addMessage( "IndKick:ForceShoot" );
        agent->debugClient().setTarget( target_point );
        dlog.addText( Logger::TEAM,
                      __FILE__":  kick to goal (%.1f %.1f) speed=%.2f",
                      target_point.x, target_point.y,
                      ball_speed );

        Body_KickOneStep( target_point, ball_speed ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    //
    // kick to the teammate nearest to opponent goal
    //

    const Vector2D goal( ServerParam::i().pitchHalfLength(),
                         wm.self().pos().y * 0.8 );

    double min_dist = 100000.0;
    const PlayerObject * receiver = static_cast< const PlayerObject * >( 0 );

    const PlayerPtrCont::const_iterator t_end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->posCount() > 5 ) continue;
        if ( (*t)->distFromBall() < 1.5 ) continue;
        if ( (*t)->distFromBall() > 20.0 ) continue;
        if ( (*t)->pos().x > wm.offsideLineX() ) continue;

        double dist = (*t)->pos().dist( goal ) + (*t)->distFromBall();
        if ( dist < min_dist )
        {
            min_dist = dist;
            receiver = (*t);
        }
    }

    Vector2D target_point = goal;
    double target_dist = 10.0;
    if ( ! receiver )
    {
        target_dist = wm.teammatesFromSelf().front()->distFromSelf();
        target_point = wm.teammatesFromSelf().front()->pos();
    }
    else
    {
        target_dist = receiver->distFromSelf();
        target_point = receiver->pos();
        target_point.x += 0.6;
    }

    double ball_speed = calc_first_term_geom_series_last( 1.8, // end speed
                                                          target_dist,
                                                          ServerParam::i().ballDecay() );
    ball_speed = std::min( ball_speed, max_kick_speed );

    agent->debugClient().addMessage( "IndKick:ForcePass%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );
    dlog.addText( Logger::TEAM,
                  __FILE__":  pass to nearest teammate (%.1f %.1f) speed=%.2f",
                  target_point.x, target_point.y,
                  ball_speed );


    Body_KickOneStep( target_point, ball_speed ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
    agent->addSayMessage( new BallMessage( agent->effector().queuedNextBallPos(),
                                           agent->effector().queuedNextBallVel() ) );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const Vector2D face_point( 50.0, 0.0 );
    const AngleDeg face_angle = ( face_point - wm.self().pos() ).th();

    //     if ( wm.setplayCount() <= 3 )
    //     {
    //         Body_TurnToPoint( face_point ).execute( agent );
    //         agent->setNeckAction( new Neck_ScanField() );
    //         return;
    //     }

    if ( wm.time().stopped() > 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) stoppage time" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( ( face_angle - wm.self().body() ).abs() > 5.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) turn to the front of goal" );

        agent->debugClient().addMessage( "IndKick:TurnTo" );
        agent->debugClient().setTarget( face_point );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() <= 10
         && wm.teammatesFromSelf().empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        agent->debugClient().addMessage( "IndKick:NoTeammate" );
        agent->debugClient().setTarget( face_point );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayIndirectFreeKick::doKickToShooter( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();


    const Vector2D goal( ServerParam::i().pitchHalfLength(),
                         wm.self().pos().y * 0.8 );

    double min_dist = 100000.0;
    const PlayerObject * receiver = static_cast< const PlayerObject * >( 0 );

    const PlayerPtrCont::const_iterator t_end = wm.teammatesFromBall().end();
    for ( PlayerPtrCont::const_iterator t = wm.teammatesFromBall().begin();
          t != t_end;
          ++t )
    {
        if ( (*t)->posCount() > 5 ) continue;
        //if ( (*t)->distFromBall() < 1.5 ) continue;
        if ( (*t)->distFromBall() > 20.0 ) continue;
        if ( (*t)->pos().x > wm.offsideLineX() ) continue;
        if ( (*t)->pos().x < wm.ball().pos().x - 3.0 ) continue;
        //if ( (*t)->pos().absY() > ServerParam::i().goalHalfWidth() * 0.5 ) continue;

        double goal_dist = (*t)->pos().dist( goal );
        if ( goal_dist > 16.0 )
        {
            continue;
        }

        double dist = goal_dist * 0.4 + (*t)->distFromBall() * 0.6;

        if ( dist < min_dist )
        {
            min_dist = dist;
            receiver = (*t);
        }
    }

    if ( ! receiver )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKicToShooter) no shooter" );
        return false;
    }

    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    Vector2D target_point = receiver->pos() + receiver->vel();
    target_point.x += 0.6;

    double target_dist = wm.ball().pos().dist( target_point );

    int ball_reach_step
        = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                  target_dist,
                                                                  ServerParam::i().ballDecay() ) ) );
    double ball_speed = calc_first_term_geom_series( target_dist,
                                                     ServerParam::i().ballDecay(),
                                                     ball_reach_step );

    ball_speed = std::min( ball_speed, max_ball_speed );

    agent->debugClient().addMessage( "IndKick:KickToShooter%.3f", ball_speed );
    agent->debugClient().setTarget( target_point );
    dlog.addText( Logger::TEAM,
                  __FILE__":  pass to nearest teammate (%.1f %.1f) ball_speed=%.2f reach_step=%d",
                  target_point.x, target_point.y,
                  ball_speed, ball_reach_step );

    Body_KickOneStep( target_point, ball_speed ).execute( agent );
    agent->setNeckAction( new Neck_ScanField() );

    return true;

}

namespace {

Vector2D
get_avoid_circle_point( const WorldModel & wm,
                        Vector2D point )
{
    const ServerParam & SP = ServerParam::i();

    const double circle_r
        = wm.gameMode().type() == GameMode::BackPass_
        ? SP.goalAreaLength() + 0.5
        : SP.centerCircleR() + 0.5;
    const double circle_r2 = std::pow( circle_r, 2 );

    dlog.addText( Logger::TEAM,
                  __FILE__": (get_avoid_circle_point) point=(%.1f %.1f)",
                  point.x, point.y );


    if ( point.x < -SP.pitchHalfLength() + 3.0
         && point.absY() < SP.goalHalfWidth() )
    {
        while ( point.x < wm.ball().pos().x
                && point.x > - SP.pitchHalfLength()
                && wm.ball().pos().dist2( point ) < circle_r2 )
        {
            //point.x -= 0.2;
            point.x = ( point.x - SP.pitchHalfLength() ) * 0.5 - 0.01;
            dlog.addText( Logger::TEAM,
                          __FILE__": adjust x (%.1f %.1f)",
                          point.x, point.y );
        }
    }

    if ( point.x < -SP.pitchHalfLength() + 0.5
         && point.absY() < SP.goalHalfWidth() + 0.5
         && wm.self().pos().x < -SP.pitchHalfLength()
         && wm.self().pos().absY() < SP.goalHalfWidth() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) ok. already in our goal",
                      point.x, point.y );
        return point;
    }

    if ( wm.ball().pos().dist2( point ) < circle_r2 )
    {
        Vector2D rel = point - wm.ball().pos();
        rel.setLength( circle_r );
        point = wm.ball().pos() + rel;

        dlog.addText( Logger::TEAM,
                      __FILE__": (get_avoid_circle_point) circle contains target. adjusted=(%.2f %.2f)",
                      point.x, point.y );
    }

    // if ( wm.ball().pos().x - circle_r < point.x
    //      && point.x < wm.ball().pos().x
    //      && wm.ball().pos().dist2( point ) < circle_r2 )
    // {
    //     if ( wm.self().pos().x < - SP.pitchHalfLength() + 0.1
    //          && wm.self().pos().absY() < SP.goalHalfWidth() - 0.5 )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__": can go to the point directly." );
    //         return point;
    //     }
    //     if ( wm.self().pos().x < wm.ball().pos().x - circle_r )
    //     {
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__": can go to the Y. my_pos.x=%.1f < ball_x-r=%f",
    //                       wm.self().pos().x,
    //                       wm.ball().pos().x - circle_r );
    //         return Vector2D( wm.self().pos().x, point.y );
    //     }
    //     Vector2D tmp_point( wm.ball().pos().x - circle_r, point.y );
    //     dlog.addText( Logger::TEAM,
    //                   __FILE__": modified recursive. tmp_point=(%.1f %.1f)",
    //                   tmp_point.x, tmp_point.y );
    //     return get_avoid_circle_point( wm, tmp_point );
    // }


    return Bhv_SetPlay::get_avoid_circle_point( wm, point );
}

} // end noname namespace

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doOffenseMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    target_point.x = std::min( wm.offsideLineX() - 1.0, target_point.x );

    double nearest_dist = 1000.0;
    const PlayerObject * teammate = wm.getTeammateNearestTo( target_point, 10, &nearest_dist );
    if ( nearest_dist < 2.5 )
    {
        target_point += ( target_point - teammate->pos() ).setLengthVector( 2.5 );
        target_point.x = std::min( wm.offsideLineX() - 1.0, target_point.x );
    }

    double dash_power = wm.self().getSafetyDashPower( ServerParam::i().maxDashPower() );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    agent->debugClient().addMessage( "IndFK:OffenseMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Vector2D turn_point
            = ( ServerParam::i().theirTeamGoalPos() + wm.ball().pos() ) * 0.5;

        Body_TurnToPoint( turn_point ).execute( agent );
        dlog.addText( Logger::TEAM,
                      __FILE__":  our kick. turn to (%.1f %.1f)",
                      turn_point.x, turn_point.y );
    }

    if ( target_point.x > 36.0
         && ( wm.self().pos().dist( target_point )
              > std::max( wm.ball().pos().dist( target_point ) * 0.2, dist_thr ) + 6.0
              || wm.self().stamina() < ServerParam::i().staminaMax() * 0.7 )
         )
    {
        if ( ! wm.self().staminaModel().capacityIsEmpty() )
        {
            agent->debugClient().addMessage( "Sayw" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayIndirectFreeKick::doDefenseMove( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    Vector2D adjusted_point = get_avoid_circle_point( wm, target_point );

    dlog.addText( Logger::TEAM,
                  __FILE__": their kick adjust target to (%.1f %.1f)->(%.1f %.1f) ",
                  target_point.x, target_point.y,
                  adjusted_point.x, adjusted_point.y );

    double dash_power = wm.self().getSafetyDashPower( SP.maxDashPower() );

    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 0.5 ) dist_thr = 0.5;

    if ( adjusted_point != target_point
         && wm.ball().pos().dist( target_point ) > 10.0
         && wm.self().inertiaFinalPoint().dist( adjusted_point ) < dist_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": reverted to the first target point" );
        adjusted_point = target_point;
    }

    {
        const double collision_dist
            = wm.self().playerType().playerSize()
            + SP.goalPostRadius()
            + 0.2;

        Vector2D goal_post_l( -SP.pitchHalfLength() + SP.goalPostRadius(),
                              -SP.goalHalfWidth() - SP.goalPostRadius() );
        Vector2D goal_post_r( -SP.pitchHalfLength() + SP.goalPostRadius(),
                              +SP.goalHalfWidth() + SP.goalPostRadius() );
        double dist_post_l = wm.self().pos().dist( goal_post_l );
        double dist_post_r = wm.self().pos().dist( goal_post_r );

        const Vector2D & nearest_post = ( dist_post_l < dist_post_r
                                          ? goal_post_l
                                          : goal_post_r );
        double dist_post = std::min( dist_post_l, dist_post_r );

        if ( dist_post < collision_dist + wm.self().playerType().realSpeedMax() + 0.5 )
        {
            Circle2D post_circle( nearest_post, collision_dist );
            Segment2D move_line( wm.self().pos(), adjusted_point );

            if ( post_circle.intersection( move_line, NULL, NULL ) > 0 )
            {
                AngleDeg post_angle = ( nearest_post - wm.self().pos() ).th();
                if ( nearest_post.y < wm.self().pos().y )
                {
                    adjusted_point = nearest_post;
                    adjusted_point += Vector2D::from_polar( collision_dist + 0.1, post_angle - 90.0 );
                    // Vector2D rel = adjusted_point - wm.self().pos();
                    // rel.rotate( -45.0 );
                    // adjusted_point = wm.self().pos() + rel;
                }
                else
                {
                    adjusted_point = nearest_post;
                    adjusted_point += Vector2D::from_polar( collision_dist + 0.1, post_angle + 90.0 );
                    // Vector2D rel = adjusted_point - wm.self().pos();
                    // rel.rotate( +45.0 );
                    // adjusted_point = wm.self().pos() + rel;
                }

                dist_thr = 0.05;
                dlog.addText( Logger::TEAM,
                              __FILE__": adjust to avoid goal post. (%.2f %.2f)",
                              adjusted_point.x, adjusted_point.y );
            }
        }
    }

    agent->debugClient().addMessage( "IndFKMove" );
    agent->debugClient().setTarget( adjusted_point );
    agent->debugClient().addCircle( adjusted_point, dist_thr );

    if ( ! Body_GoToPoint( adjusted_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
        dlog.addText( Logger::TEAM,
                      __FILE__":  their kick. turn to ball" );
    }

    agent->setNeckAction( new Neck_TurnToBall() );
}
