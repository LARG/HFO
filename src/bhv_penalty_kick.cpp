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

#include "bhv_penalty_kick.h"

#include "strategy.h"

#include "bhv_goalie_chase_ball.h"
#include "bhv_goalie_basic_move.h"
#include "bhv_go_to_static_ball.h"

#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_dribble2008.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_smart_kick.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_go_to_point_look_ball.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_stop_dash.h>
#include <rcsc/action/body_stop_ball.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/penalty_kick_state.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/geom/ray_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/math_util.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PenaltyKickState * state = wm.penaltyKickState();

    switch ( wm.gameMode().type() ) {
    case GameMode::PenaltySetup_:
        if ( state->currentTakerSide() == wm.ourSide() )
        {
            if ( state->isKickTaker( wm.ourSide(), wm.self().unum() ) )
            {
                return doKickerSetup( agent );
            }
        }
        // their kick phase
        else
        {
            if ( wm.self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case GameMode::PenaltyReady_:
        if ( state->currentTakerSide() == wm.ourSide() )
        {
            if ( state->isKickTaker( wm.ourSide(), wm.self().unum() ) )
            {
                return doKickerReady( agent );
            }
        }
        // their kick phase
        else
        {
            if ( wm.self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case GameMode::PenaltyTaken_:
        if ( state->currentTakerSide() == agent->world().ourSide() )
        {
            if ( state->isKickTaker( wm.ourSide(), wm.self().unum() ) )
            {
                return doKicker( agent );
            }
        }
        // their kick phase
        else
        {
            if ( wm.self().goalie() )
            {
                return doGoalie( agent );
            }
        }
        break;
    case GameMode::PenaltyScore_:
    case GameMode::PenaltyMiss_:
        if ( state->currentTakerSide() == wm.ourSide() )
        {
            if ( wm.self().goalie() )
            {
                return doGoalieSetup( agent );
            }
        }
        break;
    case GameMode::PenaltyOnfield_:
    case GameMode::PenaltyFoul_:
        break;
    default:
        // nothing to do.
        std::cerr << "Current playmode is NOT a Penalty Shootout???" << std::endl;
        return false;
    }


    if ( wm.self().goalie() )
    {
        return doGoalieWait( agent );
    }
    else
    {
        return doKickerWait( agent );
    }

    // never reach here
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doKickerWait( PlayerAgent * agent )
{
#if 1
    //int myid = agent->world().self().unum() - 1;
    //Vector2D wait_pos(1, 6.5, 15.0 * myid);
    //Vector2D wait_pos(1, 5.5, 90.0 + (180.0 / 12.0) * agent->world().self().unum());
    double dist_step = ( 9.0 + 9.0 ) / 12;
    Vector2D wait_pos( -2.0, -9.8 + dist_step * agent->world().self().unum() );

    // already there
    if ( agent->world().self().pos().dist( wait_pos ) < 0.7 )
    {
        Bhv_NeckBodyToBall().execute( agent );
    }
    else
    {
        // no dodge
        Body_GoToPoint( wait_pos,
                        0.3,
                        ServerParam::i().maxDashPower()
                        ).execute( agent );
        agent->setNeckAction( new Neck_TurnToRelative( 0.0 ) );
    }
#else
    const WorldModel & wm = agent->world();
    const PenaltyKickState * state = wm.penaltyKickState();

    const int taker_unum = 11 - ( ( state->ourTakerCounter() - 1 ) % 11 );
    const double circle_r = ServerParam::i().centerCircleR() - 1.0;
    const double dir_step = 360.0 / 9.0;
    //const AngleDeg base_angle = ( wm.time().cycle() % 360 ) * 4;
    const AngleDeg base_angle = wm.time().cycle() % 90;

    AngleDeg wait_angle;
    Vector2D wait_pos( 0.0, 0.0 );

    int i = 0;
    for ( int unum = 2; unum <= 11; ++unum )
    {
        if ( taker_unum == unum )
        {
            continue;
        }

        // TODO: goalie check

        if ( i >= 9 )
        {
            wait_pos.assign( 0.0, 0.0 );
            break;
        }

        if ( wm.self().unum() == unum )
        {
            wait_angle = base_angle + dir_step * i;
            if ( wm.ourSide() == RIGHT )
            {
                wait_angle += dir_step;
            }
            wait_pos = Vector2D::from_polar( circle_r, wait_angle );
            break;
        }

        ++i;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": penalty wait. count=%d pos=(%.1f %.1f) angle=%.1f",
                  i,
                  wait_pos.x, wait_pos.y,
                  wait_angle.degree() );

    Body_GoToPoint( wait_pos,
                    0.5,
                    ServerParam::i().maxDashPower()
                    ).execute( agent );
    agent->setNeckAction( new Neck_TurnToRelative( 0.0 ) );
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doKickerSetup( PlayerAgent * agent )
{
    const Vector2D goal_c = ServerParam::i().theirTeamGoalPos();
    const PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    AngleDeg place_angle = 0.0;

    // ball is close enoughly.
    if ( ! Bhv_GoToStaticBall( place_angle ).execute( agent ) )
    {
        Body_TurnToPoint( goal_c ).execute( agent );
        if ( opp_goalie )
        {
            agent->setNeckAction( new Neck_TurnToPoint( opp_goalie->pos() ) );
        }
        else
        {
            agent->setNeckAction( new Neck_TurnToPoint( goal_c ) );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doKickerReady( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PenaltyKickState * state = wm.penaltyKickState();

    // stamina recovering...
    if ( wm.self().stamina() < ServerParam::i().staminaMax() - 10.0
         && ( wm.time().cycle() - state->time().cycle() > ServerParam::i().penReadyWait() - 3 ) )
    {
        return doKickerSetup( agent );
    }

    if ( ! wm.self().isKickable() )
    {
        return doKickerSetup( agent );
    }

    return doKicker( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doKicker( PlayerAgent * agent )
{
    //
    // server does NOT allow multiple kicks
    //

    if ( ! ServerParam::i().penAllowMultKicks() )
    {
        return doOneKickShoot( agent );
    }

    //
    // server allows multiple kicks
    //

    const WorldModel & wm = agent->world();

    // get ball
    if ( ! wm.self().isKickable() )
    {
        if ( ! Body_Intercept().execute( agent ) )
        {
            Body_GoToPoint( wm.ball().pos(),
                            0.4,
                            ServerParam::i().maxDashPower()
                            ).execute( agent );
        }

        if ( wm.ball().posCount() > 0 )
        {
            agent->setNeckAction( new Neck_TurnToBall() );
        }
        else
        {
            const PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
            if ( opp_goalie )
            {
                agent->setNeckAction( new Neck_TurnToPoint( opp_goalie->pos() ) );
            }
            else
            {
                agent->setNeckAction( new Neck_ScanField() );
            }
        }

        return true;
    }

    // kick decision
    if ( doShoot( agent ) )
    {
        return true;
    }

    return doDribble( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doOneKickShoot( PlayerAgent* agent )
{
    const double ball_speed = agent->world().ball().vel().r();
    // ball is moveng --> kick has taken.
    if ( ! ServerParam::i().penAllowMultKicks()
         && ball_speed > 0.3 )
    {
        return false;
    }

    // go to the ball side
    if ( ! agent->world().self().isKickable() )
    {
        Body_GoToPoint( agent->world().ball().pos(),
                        0.4,
                        ServerParam::i().maxDashPower()
                        ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    // turn to the ball to get the maximal kick rate.
    if ( ( agent->world().ball().angleFromSelf() - agent->world().self().body() ).abs()
         > 3.0 )
    {
        Body_TurnToBall().execute( agent );
        const PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
        if ( opp_goalie )
        {
            agent->setNeckAction( new Neck_TurnToPoint( opp_goalie->pos() ) );
        }
        else
        {
            Vector2D goal_c = ServerParam::i().theirTeamGoalPos();
            agent->setNeckAction( new Neck_TurnToPoint( goal_c ) );
        }
        return true;
    }

    // decide shot target point
    Vector2D shoot_point = ServerParam::i().theirTeamGoalPos();

    const PlayerObject * opp_goalie = agent->world().getOpponentGoalie();
    if ( opp_goalie )
    {
        shoot_point.y = ServerParam::i().goalHalfWidth() - 1.0;
        if ( opp_goalie->pos().absY() > 0.5 )
        {
            if ( opp_goalie->pos().y > 0.0 )
            {
                shoot_point.y *= -1.0;
            }
        }
        else if ( opp_goalie->bodyCount() < 2 )
        {
            if ( opp_goalie->body().degree() > 0.0 )
            {
                shoot_point.y *= -1.0;
            }
        }
    }

    // enforce one step kick
    Body_KickOneStep( shoot_point,
                      ServerParam::i().ballSpeedMax()
                      ).execute( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doShoot( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const PenaltyKickState * state = wm.penaltyKickState();

    if ( wm.time().cycle() - state->time().cycle() > ServerParam::i().penTakenWait() - 25 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doShoot) time limit. stateTime=%d spentTime=%d timeThr=%d force shoot.",
                      state->time().cycle(),
                      wm.time().cycle() - state->time().cycle(),
                      ServerParam::i().penTakenWait() - 25 );
        return doOneKickShoot( agent );
    }

    Vector2D shot_point;
    double shot_speed;

    if ( getShootTarget( agent, &shot_point, &shot_speed ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (doShoot) shoot to (%.1f %.1f) speed=%f",
                      shot_point.x, shot_point.y,
                      shot_speed );

        Body_SmartKick( shot_point,
                        shot_speed,
                        shot_speed * 0.96,
                        2 ).execute( agent );
        agent->setNeckAction( new Neck_TurnToPoint( shot_point ) );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::getShootTarget( const PlayerAgent * agent,
                                 Vector2D * point,
                                 double * first_speed )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    if ( SP.theirTeamGoalPos().dist2( wm.ball().pos() ) > std::pow( 35.0, 2 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__" (getShootTarget) too far" );
        return false;
    }

    const PlayerObject * opp_goalie = wm.getOpponentGoalie();

    // goalie is not found.
    if ( ! opp_goalie )
    {
        Vector2D shot_c = SP.theirTeamGoalPos();
        if ( point ) *point = shot_c;
        if ( first_speed ) *first_speed = SP.ballSpeedMax();

        dlog.addText( Logger::TEAM,
                      __FILE__" (getShootTarget) no goalie" );
        return true;
    }

    int best_l_or_r = 0;
    double best_speed = SP.ballSpeedMax() + 1.0;

    double post_buf = 1.0
        + std::min( 2.0,
                    ( SP.pitchHalfLength() - wm.self().pos().absX() ) * 0.1 );

    // consder only 2 angle
    Vector2D shot_l( SP.pitchHalfLength(), -SP.goalHalfWidth() + post_buf );
    Vector2D shot_r( SP.pitchHalfLength(), +SP.goalHalfWidth() - post_buf );

    const AngleDeg angle_l = ( shot_l - wm.ball().pos() ).th();
    const AngleDeg angle_r = ( shot_r - wm.ball().pos() ).th();

    // !!! Magic Number !!!
    const double goalie_max_speed = 1.0;
    // default player speed max * conf decay
    const double goalie_dist_buf
        = goalie_max_speed * std::min( 5, opp_goalie->posCount() )
        + SP.catchAreaLength()
        + 0.2;

    const Vector2D goalie_next_pos = opp_goalie->pos() + opp_goalie->vel();

    for ( int i = 0; i < 2; i++ )
    {
        const Vector2D target = ( i == 0 ? shot_l : shot_r );
        const AngleDeg angle = ( i == 0 ? angle_l : angle_r );

        double dist2goal = wm.ball().pos().dist( target );

        // set minimum speed to reach the goal line
        double tmp_first_speed =  ( dist2goal + 5.0 ) * ( 1.0 - SP.ballDecay() );
        tmp_first_speed = std::max( 1.2, tmp_first_speed );

        bool over_max = false;
        while ( ! over_max )
        {
            if ( tmp_first_speed > SP.ballSpeedMax() )
            {
                over_max = true;
                tmp_first_speed = SP.ballSpeedMax();
            }

            Vector2D ball_pos = wm.ball().pos();
            Vector2D ball_vel = Vector2D::polar2vector( tmp_first_speed, angle );
            ball_pos += ball_vel;
            ball_vel *= SP.ballDecay();

            bool goalie_can_reach = false;

            // goalie move at first step is ignored (cycle is set to ZERO),
            // because goalie must look the ball velocity before chasing action.
            double cycle = 0.0;
            while ( ball_pos.absX() < SP.pitchHalfLength() )
            {
                if ( goalie_next_pos.dist( ball_pos )
                     < goalie_max_speed * cycle + goalie_dist_buf )
                {
                    dlog.addText( Logger::TEAM,
                                  __FILE__" (getShootTarget) goalie can reach. cycle=%.0f"
                                  " target=(%.1f, %.1f) speed=%.1f",
                                  cycle + 1.0, target.x, target.y, tmp_first_speed );
                    goalie_can_reach = true;
                    break;
                }

                ball_pos += ball_vel;
                ball_vel *= SP.ballDecay();
                cycle += 1.0;
            }

            if ( ! goalie_can_reach )
            {
                dlog.addText( Logger::TEAM,
                              __FILE__" (getShootTarget) goalie never reach. target=(%.1f, %.1f) speed=%.1f",
                              target.x, target.y,
                              tmp_first_speed );
                if ( tmp_first_speed < best_speed )
                {
                    best_l_or_r = i;
                    best_speed = tmp_first_speed;
                }
                break; // end of this angle
            }
            tmp_first_speed += 0.4;
        }
    }


    if ( best_speed <= SP.ballSpeedMax() )
    {
        if ( point )
        {
            *point = ( best_l_or_r == 0 ? shot_l : shot_r );
        }
        if ( first_speed )
        {
            *first_speed = best_speed;
        }

        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!
  dribble to the shootable point
*/
bool
Bhv_PenaltyKick::doDribble( PlayerAgent * agent )
{
    static const int CONTINUAL_COUNT = 20;
#ifdef __APPLE__
    static int S_target_continual_count = CONTINUAL_COUNT;
#else
    static thread_local int S_target_continual_count = CONTINUAL_COUNT;
#endif

    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    const Vector2D goal_c = SP.theirTeamGoalPos();

    const double penalty_abs_x = ServerParam::i().theirPenaltyAreaLineX();

    const PlayerObject * opp_goalie = wm.getOpponentGoalie();
    const double goalie_max_speed = 1.0;

    const double my_abs_x = wm.self().pos().absX();

    const double goalie_dist = ( opp_goalie
                                 ? ( opp_goalie->pos().dist( wm.self().pos() )
                                     - goalie_max_speed * std::min( 5, opp_goalie->posCount() ) )
                                 : 200.0 );
    const double goalie_abs_x = ( opp_goalie
                                  ? opp_goalie->pos().absX()
                                  : 200.0 );


    /////////////////////////////////////////////////
    // dribble parametors

    const double base_target_abs_y = ServerParam::i().goalHalfWidth() + 4.0;
    Vector2D drib_target = goal_c;
    double drib_power = ServerParam::i().maxDashPower();
    int drib_dashes = 6;

    /////////////////////////////////////////////////

    // it's too far to the goal.
    // dribble to the shootable area
    if ( my_abs_x < penalty_abs_x - 3.0
         && goalie_dist > 10.0 )
    {
        //drib_power *= 0.6;
    }
    else
    {
        if ( goalie_abs_x > my_abs_x )
        {
            if ( goalie_dist < 4.0 )
            {
                if ( S_target_continual_count == 1 )
                {
                    S_target_continual_count = -CONTINUAL_COUNT;
                }
                else if ( S_target_continual_count == -1 )
                {
                    S_target_continual_count = +CONTINUAL_COUNT;
                }
                else if ( S_target_continual_count > 0 )
                {
                    S_target_continual_count--;
                }
                else
                {
                    S_target_continual_count++;
                }
            }

            if ( S_target_continual_count > 0 )
            {
                if ( agent->world().self().pos().y < -base_target_abs_y + 2.0 )
                {
                    drib_target.y = base_target_abs_y;
                    dlog.addText( Logger::TEAM,
                                  __FILE__": dribble(1). target=(%.1f, %.1f)",
                                  drib_target.x, drib_target.y );
                }
                else
                {
                    drib_target.y = -base_target_abs_y;
                    dlog.addText( Logger::TEAM,
                                  __FILE__": dribble(2). target=(%.1f, %.1f)",
                                  drib_target.x, drib_target.y );
                }
            }
            else // == if ( ! S_target_reversed )
            {
                if ( agent->world().self().pos().y > base_target_abs_y - 2.0 )
                {
                    drib_target.y = -base_target_abs_y;
                    dlog.addText( Logger::TEAM,
                                  __FILE__": dribble(3). target=(%.1f, %.1f)",
                                  drib_target.x, drib_target.y );
                }
                else
                {
                    drib_target.y = base_target_abs_y;
                    dlog.addText( Logger::TEAM,
                                  __FILE__": dribble(4). target=(%.1f, %.1f)",
                                  drib_target.x, drib_target.y );
                }
            }

            drib_target.x = goalie_abs_x + 1.0;
            drib_target.x = min_max( penalty_abs_x - 2.0,
                                     drib_target.x,
                                     ServerParam::i().pitchHalfLength() - 4.0 );

            double dashes = ( agent->world().self().pos().dist( drib_target ) * 0.8
                              / ServerParam::i().defaultPlayerSpeedMax() );
            drib_dashes = static_cast<int>(floor(dashes));
            drib_dashes = min_max( 1, drib_dashes, 6 );
            dlog.addText( Logger::TEAM,
                          __FILE__": dribble. target=(%.1f, %.1f) dashes=%d",
                          drib_target.x, drib_target.y, drib_dashes );
        }
    }


    if ( opp_goalie && goalie_dist < 5.0 )
    {
        AngleDeg drib_angle = ( drib_target - agent->world().self().pos() ).th();
        AngleDeg goalie_angle = ( opp_goalie->pos() - agent->world().self().pos() ).th();
        drib_dashes = 6;
        if ( (drib_angle - goalie_angle).abs() < 80.0 )
        {
            drib_target = agent->world().self().pos();
            drib_target += Vector2D::polar2vector( 10.0,
                                                   goalie_angle
                                                   + ( wm.self().pos().y > 0
                                                       ? -1.0
                                                       : +1.0 ) * 55.0 );
            dlog.addText( Logger::TEAM,
                          __FILE__": dribble. avoid goalie. target=(%.1f, %.1f)",
                          drib_target.x, drib_target.y );
        }
        dlog.addText( Logger::TEAM,
                      __FILE__": dribble. goalie near. dashes=%d",
                      drib_dashes );
    }

    Vector2D target_rel = drib_target - agent->world().self().pos();
    double buf = 2.0;
    if ( drib_target.absX() < penalty_abs_x )
    {
        buf += 2.0;
    }

    if ( target_rel.absX() < 5.0
         && ( opp_goalie == NULL
              || opp_goalie->pos().dist( drib_target ) > target_rel.r() - buf )
         )
    {
        if ( ( target_rel.th() - agent->world().self().body() ).abs() < 5.0 )
        {
            double first_speed
                = calc_first_term_geom_series_last
                ( 0.5,
                  target_rel.r(),
                  ServerParam::i().ballDecay() );

            first_speed = std::min( first_speed, ServerParam::i().ballSpeedMax() );
            Body_SmartKick( drib_target,
                            first_speed,
                            first_speed * 0.96,
                            3 ).execute( agent );
            //             Body_KickMultiStep( drib_target, first_speed ).execute( agent );
            dlog.addText( Logger::TEAM,
                          __FILE__": kick. to=(%.1f, %.1f) first_speed=%.1f",
                          drib_target.x, drib_target.y, first_speed );
        }
        else if ( ( agent->world().ball().rpos()
                    + agent->world().ball().vel()
                    - agent->world().self().vel() ).r()
                  < agent->world().self().playerType().kickableArea() - 0.2 )
        {
            Body_TurnToPoint( drib_target ).execute( agent );
        }
        else
        {
            Body_StopBall().execute( agent );
        }
    }
    else
    {
#if 0
        bool dodge_mode = true;
        if ( opp_goalie == NULL
             || ( ( opp_goalie->pos() - agent->world().self().pos() ).th()
                  - ( drib_target - agent->world().self().pos() ).th() ).abs() > 45.0 )
        {
            dodge_mode = false;
        }
#else
        bool dodge_mode = false;
#endif
        Body_Dribble2008( drib_target,
                          2.0,
                          drib_power,
                          drib_dashes,
                          dodge_mode
                          ).execute( agent );
    }

    if ( opp_goalie )
    {
        agent->setNeckAction( new Neck_TurnToPoint( opp_goalie->pos() ) );
    }
    else
    {
        agent->setNeckAction( new Neck_ScanField() );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doGoalieWait( PlayerAgent* agent )
{
#if 0
    Vector2D wait_pos( - ServerParam::i().pitchHalfLength() - 2.0, -25.0 );

    if ( agent->world().self().pos().absX()
         > ServerParam::i().pitchHalfLength()
         && wait_pos.y * agent->world().self().pos().y < 0.0 )
    {
        wait_pos.y *= -1.0;
    }

    double dash_power = ServerParam::i().maxDashPower();

    if ( agent->world().self().stamina()
         < ServerParam::i().staminaMax() * 0.8 )
    {
        dash_power *= 0.2;
    }
    else
    {
        dash_power *= ( 0.2 + ( ( agent->world().self().stamina()
                                  / ServerParam::i().staminaMax() ) - 0.8 ) / 0.2 * 0.8 );
    }

    Vector2D face_point( wait_pos.x * 0.5, 0.0 );
    if ( agent->world().self().pos().dist2( wait_pos ) < 1.0 )
    {
        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        Body_GoToPoint( wait_pos,
                        0.5,
                        dash_power
                        ).execute( agent );
        agent->setNeckAction( new Neck_TurnToPoint( face_point ) );
    }
#else
    Body_TurnToBall().execute( agent );
    agent->setNeckAction( new Neck_TurnToBall() );
#endif
    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doGoalieSetup( PlayerAgent * agent )
{
    Vector2D move_point( ServerParam::i().ourTeamGoalLineX() + ServerParam::i().penMaxGoalieDistX() - 0.1,
                         0.0 );

    if ( Body_GoToPoint( move_point,
                         0.5,
                         ServerParam::i().maxDashPower()
                         ).execute( agent ) )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    // already there
    if ( std::fabs( agent->world().self().body().abs() ) > 2.0 )
    {
        Vector2D face_point( 0.0, 0.0 );
        Body_TurnToPoint( face_point ).execute( agent );
    }

    agent->setNeckAction( new Neck_TurnToBall() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doGoalie( PlayerAgent* agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    ///////////////////////////////////////////////
    // check if catchabale
    Rect2D our_penalty( Vector2D( -SP.pitchHalfLength(),
                                  -SP.penaltyAreaHalfWidth() + 1.0 ),
                        Size2D( SP.penaltyAreaLength() - 1.0,
                                SP.penaltyAreaWidth() - 2.0 ) );

    if ( wm.ball().distFromSelf() < SP.catchableArea() - 0.05
         && our_penalty.contains( wm.ball().pos() ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": goalie try to catch" );
        return agent->doCatch();
    }

    if ( wm.self().isKickable() )
    {
        Body_ClearBall().execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    ///////////////////////////////////////////////
    // if taker can only one kick, goalie should stay the front of goal.
    if ( ! SP.penAllowMultKicks() )
    {
        // kick has not been taken.
        if ( wm.ball().vel().r2() < 0.01
             && wm.ball().pos().absX() < SP.pitchHalfLength() - SP.penDistX() - 1.0 )
        {
            return doGoalieSetup( agent );
        }

        if ( wm.ball().vel().r2() > 0.01 )
        {
            return doGoalieSlideChase( agent );
        }
    }

    return doGoalieBasicMove( agent );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doGoalieBasicMove( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    Rect2D our_penalty( Vector2D( -SP.pitchHalfLength(),
                                  -SP.penaltyAreaHalfWidth() + 1.0 ),
                        Size2D( SP.penaltyAreaLength() - 1.0,
                                SP.penaltyAreaWidth() - 2.0 ) );

    dlog.addText( Logger::TEAM,
                  __FILE__": goalieBasicMove. " );

    ////////////////////////////////////////////////////////////////////////
    // get active interception catch point
    const int self_min = wm.interceptTable()->selfReachCycle();
    Vector2D move_pos = wm.ball().inertiaPoint( self_min );

    if ( our_penalty.contains( move_pos ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": goalieBasicMove. exist intercept point " );
        agent->debugClient().addMessage( "ExistIntPoint" );
        if ( wm.interceptTable()->opponentReachCycle() < wm.interceptTable()-> selfReachCycle()
             || wm.interceptTable()-> selfReachCycle() <=4 )
        {
            if ( Body_Intercept( false ).execute( agent ) )
            {
                agent->debugClient().addMessage( "Intercept" );
                dlog.addText( Logger::TEAM,
                              __FILE__": goalieBasicMove. do intercept " );
                agent->setNeckAction( new Neck_TurnToBall() );
                return true;
            }
        }
    }


    Vector2D my_pos = wm.self().pos();
    Vector2D ball_pos;
    if ( wm.existKickableOpponent() )
    {
        ball_pos = wm.opponentsFromBall().front()->pos();
        ball_pos += wm.opponentsFromBall().front()->vel();
    }
    else
    {
        ball_pos = inertia_n_step_point( wm.ball().pos(),
                                         wm.ball().vel(),
                                         3,
                                         SP.ballDecay() );
    }

    move_pos = getGoalieMovePos( ball_pos, my_pos );

    dlog.addText( Logger::TEAM,
                  __FILE__": goalie basic move to (%.1f, %.1f)",
                  move_pos.x, move_pos.y );
    agent->debugClient().setTarget( move_pos );
    agent->debugClient().addMessage( "BasicMove" );

    if ( ! Body_GoToPoint( move_pos,
                           0.5,
                           SP.maxDashPower()
                           ).execute( agent ) )
    {
        // already there
        AngleDeg face_angle = wm.ball().angleFromSelf();
        if ( wm.ball().angleFromSelf().isLeftOf( wm.self().body() ) )
        {
            face_angle += 90.0;
        }
        else
        {
            face_angle -= 90.0;
        }
        Body_TurnToAngle( face_angle ).execute( agent );
    }
    agent->setNeckAction( new Neck_TurnToBall() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!
  ball_pos & my_pos is set to self localization oriented.
  if ( onfiled_side != our_side ), these coordinates must be reversed.
*/
Vector2D
Bhv_PenaltyKick::getGoalieMovePos( const Vector2D & ball_pos,
                                   const Vector2D & my_pos )
{
    const ServerParam & SP = ServerParam::i();
    const double min_x = -SP.pitchHalfLength() + SP.catchAreaLength()*0.9;

    if ( ball_pos.x < -49.0 )
    {
        if ( ball_pos.absY() < SP.goalHalfWidth() )
        {
            return Vector2D( min_x, ball_pos.y );
        }
        else
        {
            return Vector2D( min_x,
                             sign( ball_pos.y ) * SP.goalHalfWidth() );
        }
    }

    Vector2D goal_l( -SP.pitchHalfLength(), -SP.goalHalfWidth() );
    Vector2D goal_r( -SP.pitchHalfLength(), +SP.goalHalfWidth() );

    AngleDeg ball2post_angle_l = ( goal_l - ball_pos ).th();
    AngleDeg ball2post_angle_r = ( goal_r - ball_pos ).th();

    // NOTE: post_angle_r < post_angle_l
    AngleDeg line_dir = AngleDeg::bisect( ball2post_angle_r,
                                          ball2post_angle_l );

    Line2D line_mid( ball_pos, line_dir );
    Line2D goal_line( goal_l, goal_r );

    Vector2D intersection = goal_line.intersection( line_mid );
    if ( intersection.isValid() )
    {
        Line2D line_l( ball_pos, goal_l );
        Line2D line_r( ball_pos, goal_r );

        AngleDeg alpha = AngleDeg::atan2_deg( SP.goalHalfWidth(),
                                              SP.penaltyAreaLength() - 2.5 );
        double dist_from_goal
            = ( ( line_l.dist( intersection ) + line_r.dist( intersection ) ) * 0.5 )
            / alpha.sin();

        dlog.addText( Logger::TEAM,
                      __FILE__": goalie move. intersection=(%.1f, %.1f) dist_from_goal=%.1f",
                      intersection.x, intersection.y, dist_from_goal );
        if ( dist_from_goal <= SP.goalHalfWidth() )
        {
            dist_from_goal = SP.goalHalfWidth();
            dlog.addText( Logger::TEAM,
                          __FILE__": goalie move. outer of goal. dist_from_goal=%.1f",
                          dist_from_goal );
        }

        if ( ( ball_pos - intersection ).r() + 1.5 < dist_from_goal )
        {
            dist_from_goal = ( ball_pos - intersection ).r() + 1.5;
            dlog.addText( Logger::TEAM,
                          __FILE__": goalie move. near than ball. dist_from_goal=%.1f",
                          dist_from_goal );
        }

        AngleDeg position_error = line_dir - ( intersection - my_pos ).th();

        const double danger_angle = 21.0;
        dlog.addText( Logger::TEAM,
                      __FILE__": goalie move position_error_angle=%.1f",
                      position_error.degree() );
        if ( position_error.abs() > danger_angle )
        {
            dist_from_goal *= ( ( 1.0 - ((position_error.abs() - danger_angle)
                                         / (180.0 - danger_angle)) )
                                * 0.5 );
            dlog.addText( Logger::TEAM,
                          __FILE__": goalie move. error is big. dist_from_goal=%.1f",
                          dist_from_goal );
        }

        Vector2D result = intersection;
        Vector2D add_vec = ball_pos - intersection;
        add_vec.setLength( dist_from_goal );
        dlog.addText( Logger::TEAM,
                      __FILE__": goalie move. intersection=(%.1f, %.1f) add_vec=(%.1f, %.1f)%.2f",
                      intersection.x, intersection.y,
                      add_vec.x, add_vec.y,
                      add_vec.r() );
        result += add_vec;
        if ( result.x < min_x )
        {
            result.x = min_x;
        }
        return result;
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": goalie move. shot line has no intersection with goal line" );

        if ( ball_pos.x > 0.0 )
        {
            return Vector2D(min_x , goal_l.y);
        }
        else if ( ball_pos.x < 0.0 )
        {
            return Vector2D(min_x , goal_r.y);
        }
        else
        {
            return Vector2D(min_x , 0.0);
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PenaltyKick::doGoalieSlideChase( PlayerAgent* agent )
{
    const WorldModel & wm = agent->world();

    if ( std::fabs( 90.0 - wm.self().body().abs() ) > 2.0 )
    {
        Vector2D face_point( wm.self().pos().x, 100.0);
        if ( wm.self().body().degree() < 0.0 )
        {
            face_point.y = -100.0;
        }
        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    Ray2D ball_ray( wm.ball().pos(), wm.ball().vel().th() );
    Line2D ball_line( ball_ray.origin(), ball_ray.dir() );
    Line2D my_line( wm.self().pos(), wm.self().body() );

    Vector2D intersection = my_line.intersection( ball_line );
    if ( ! intersection.isValid()
         || ! ball_ray.inRightDir( intersection ) )
    {
        Body_Intercept( false ).execute( agent ); // goalie mode
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    if ( wm.self().pos().dist( intersection )
         < ServerParam::i().catchAreaLength() * 0.7 )
    {
        Body_StopDash( false ).execute( agent ); // not save recovery
        agent->setNeckAction( new Neck_TurnToBall() );
        return true;
    }

    AngleDeg angle = ( intersection - wm.self().pos() ).th();
    double dash_power = ServerParam::i().maxDashPower();
    if ( ( angle - wm.self().body() ).abs() > 90.0 )
    {
        dash_power = ServerParam::i().minDashPower();
    }
    agent->doDash( dash_power );
    agent->setNeckAction( new Neck_TurnToBall() );
    return true;
}
