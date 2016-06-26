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

#include "bhv_basic_tackle.h"

#include "tackle_generator.h"

#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_point.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/ray_2d.h>

//#define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicTackle::execute( PlayerAgent * agent )
{
    const ServerParam & SP = ServerParam::i();
    const WorldModel & wm = agent->world();

    bool use_foul = false;
    double tackle_prob = wm.self().tackleProbability();

    if ( agent->config().version() >= 14.0
         && wm.self().card() == NO_CARD
         && ( wm.ball().pos().x > SP.ourPenaltyAreaLineX() + 0.5
              || wm.ball().pos().absY() > SP.penaltyAreaHalfWidth() + 0.5 )
         && tackle_prob < wm.self().foulProbability() )
    {
        tackle_prob = wm.self().foulProbability();
        use_foul = true;
    }

    if ( tackle_prob < M_min_probability )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": failed. low tackle_prob=%.2f < %.2f",
                      wm.self().tackleProbability(),
                      M_min_probability );
        return false;
    }

    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    const Vector2D self_reach_point = wm.ball().inertiaPoint( self_min );

    //
    // check where the ball shall be gone without tackle
    //

    bool ball_will_be_in_our_goal = false;

    if ( self_reach_point.x < -SP.pitchHalfLength() )
    {
        const Ray2D ball_ray( wm.ball().pos(), wm.ball().vel().th() );
        const Line2D goal_line( Vector2D( -SP.pitchHalfLength(), 10.0 ),
                                Vector2D( -SP.pitchHalfLength(), -10.0 ) );

        const Vector2D intersect = ball_ray.intersection( goal_line );
        if ( intersect.isValid()
             && intersect.absY() < SP.goalHalfWidth() + 1.0 )
        {
            ball_will_be_in_our_goal = true;

            dlog.addText( Logger::TEAM,
                          __FILE__": ball will be in our goal. intersect=(%.2f %.2f)",
                          intersect.x, intersect.y );
        }
    }

    if ( wm.existKickableOpponent()
         || ball_will_be_in_our_goal
         || ( opp_min < self_min - 3
              && opp_min < mate_min - 3 )
         || ( self_min >= 5
              && wm.ball().pos().dist2( SP.theirTeamGoalPos() ) < std::pow( 10.0, 2 )
              && ( ( SP.theirTeamGoalPos() - wm.self().pos() ).th() - wm.self().body() ).abs() < 45.0 )
         )
    {
        // try tackle
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_BasicTackle. not necessary" );
        return false;
    }

    if ( agent->config().version() < 12.0 )
    {
        // v11 or older
        return executeOld( agent );

    }
    else if ( agent->config().version() < 14.0 )
    {
        // v12-13
        return executeV12( agent );
    }

    // v14+
    return executeV14( agent, use_foul );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicTackle::executeOld( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();
    const ServerParam & SP = ServerParam::i();

    double tackle_power = SP.maxTacklePower();

    if ( wm.self().body().abs() < M_body_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_BasicTackle. (old) body dir tackle" );

        agent->debugClient().addMessage( "Tackle+" );

        agent->doTackle( tackle_power );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
        return true;
    }

    tackle_power = - SP.maxBackTacklePower();

    if ( tackle_power < 0.0
         && wm.self().body().abs() > 180.0 - M_body_thr )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Bhv_BasicTackle. (old) body reverse dir tackle" );
        agent->debugClient().addMessage( "Tackle-" );

        agent->doTackle( tackle_power );
        agent->setNeckAction( new Neck_TurnToBallOrScan() );
        return true;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicTackle. (old) failed." );
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicTackle::executeV12( PlayerAgent * agent )
{
#ifdef __APPLE__
    static GameTime s_last_execute_time( 0, 0 );
    static bool s_result = false;
    static AngleDeg s_best_angle = 0.0;
#else
    static thread_local GameTime s_last_execute_time( 0, 0 );
    static thread_local bool s_result = false;
    static thread_local AngleDeg s_best_angle = 0.0;
#endif

    const WorldModel & wm = agent->world();

    if ( s_last_execute_time == wm.time() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": called several times" );
        if ( s_result )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": executeV12() tackle angle=%.0f",
                          s_best_angle.degree() );
            agent->debugClient().addMessage( "BasicTackle%.0f",
                                             s_best_angle.degree() );

            double tackle_dir = ( s_best_angle - wm.self().body() ).degree();
            agent->doTackle( tackle_dir );
            agent->setNeckAction( new Neck_TurnToBallOrScan() );
        }
        return s_result;
    }
    s_last_execute_time = wm.time();
    s_result = false;

    const ServerParam & SP = ServerParam::i();

    const Vector2D opp_goal( + SP.pitchHalfLength(), 0.0 );
    const Vector2D our_goal( - SP.pitchHalfLength(), 0.0 );
    const Vector2D virtual_accel = ( wm.existKickableOpponent()
                                     ? ( our_goal - wm.ball().pos() ).setLengthVector( 0.5 )
                                     : Vector2D( 0.0, 0.0 ) );
    const bool shoot_chance = ( wm.ball().pos().dist( opp_goal ) < 20.0 );


    const AngleDeg ball_rel_angle
        = wm.ball().angleFromSelf() - wm.self().body();
    const double tackle_rate
        = SP.tacklePowerRate()
        * ( 1.0 - 0.5 * ball_rel_angle.abs() / 180.0 );

    AngleDeg best_angle = 0.0;
    double max_speed = -1.0;

    for ( double a = -180.0; a < 180.0; a += 10.0 )
    {
        AngleDeg rel_angle = a - wm.self().body().degree();

        double eff_power = SP.maxBackTacklePower()
            + ( ( SP.maxTacklePower() - SP.maxBackTacklePower() )
                * ( 1.0 - rel_angle.abs() / 180.0 ) );
        eff_power *= tackle_rate;

        Vector2D vel = wm.ball().vel()
            + Vector2D::polar2vector( eff_power, AngleDeg( a ) );
        vel += virtual_accel;

        double speed = vel.r();
        if ( speed > SP.ballSpeedMax() )
        {
            vel *= ( SP.ballSpeedMax() / speed );
            speed = SP.ballSpeedMax();
        }

        if ( vel.th().abs() > 90.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)"
                          " not a forward tackle.",
                          a, vel.x, vel.y );
#endif
            continue;
        }

        const Vector2D ball_next = wm.ball().pos() + vel;

        bool maybe_opponent_get_ball = false;

        const PlayerPtrCont::const_iterator o_end = wm.opponentsFromBall().end();
        for ( PlayerPtrCont::const_iterator o = wm.opponentsFromBall().begin();
              o != o_end;
              ++o )
        {
            if ( (*o)->posCount() > 10 ) continue;
            if ( (*o)->isGhost() ) continue;
            if ( (*o)->isTackling() ) continue;
            if ( (*o)->distFromBall() > 6.0 ) break;

            Vector2D opp_pos = (*o)->pos() + (*o)->vel();
            if ( opp_pos.dist( ball_next ) < SP.defaultKickableArea() + 0.1 )
            {
                maybe_opponent_get_ball = true;
                break;
            }
        }

        if ( maybe_opponent_get_ball )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)"
                          " maybe opponent get ball.",
                          a, vel.x, vel.y );
#endif
            continue;
        }

        if ( shoot_chance )
        {
            const Ray2D ball_ray( wm.ball().pos(), vel.th() );
            const Line2D goal_line( Vector2D( SP.pitchHalfLength(), 10.0 ),
                                    Vector2D( SP.pitchHalfLength(), -10.0 ) );
            const Vector2D intersect =  ball_ray.intersection( goal_line );
            if ( intersect.isValid()
                 && intersect.absY() < SP.goalHalfWidth() - 3.0 )
            {
                speed += 10.0;
#ifdef DEBUG_PRINT
                dlog.addText( Logger::TEAM,
                              __FILE__": executeV12() shoot_chance. angle=%.1f vel=(%.1f %.1f)%.2f"
                              " update.",
                              a, vel.x, vel.y, speed );
#endif
            }
        }

        if ( speed > max_speed )
        {

            max_speed = speed;
            best_angle = a;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::TEAM,
                          __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)%.2f"
                          " update.",
                          a, vel.x, vel.y, speed );
#endif

        }
#ifdef DEBUG_PRINT
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": executeV12() angle=%.1f vel=(%.1f %.1f)%.2f"
                          " no update.",
                          a, vel.x, vel.y, speed );
        }
#endif
    }


    //
    // never accelerate the ball
    //

    if ( max_speed < 0.0 )
    {
        s_result = false;
        dlog.addText( Logger::TEAM,
                      __FILE__": failed executeV12. max_speed=%.2f", max_speed );
        return false;
    }

    //
    // execute tackle
    //

    s_result = true;
    s_best_angle = best_angle;

    dlog.addText( Logger::TEAM,
                  __FILE__": executeV12() angle=%.0f",
                  best_angle.degree() );
    agent->debugClient().addMessage( "BasicTackle%.0f",
                                     best_angle.degree() );

    double tackle_dir = ( best_angle - wm.self().body() ).degree();
    agent->doTackle( tackle_dir );
    agent->setNeckAction( new Neck_TurnToBallOrScan() );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicTackle::executeV14( PlayerAgent * agent,
                             const bool use_foul )
{
    const WorldModel & wm = agent->world();

    const TackleGenerator::TackleResult & result
        = TackleGenerator::instance().bestResult( wm );

    Vector2D ball_next = wm.ball().pos() + result.ball_vel_;

    dlog.addText( Logger::TEAM,
                  __FILE__": (executeV14) %s angle=%.0f resultVel=(%.2f %.2f) ballNext=(%.2f %.2f)",
                  use_foul ? "foul" : "tackle",
                  result.tackle_angle_.degree(),
                  result.ball_vel_.x, result.ball_vel_.y,
                  ball_next.x, ball_next.y );
    agent->debugClient().addMessage( "Basic%s%.0f",
                                     use_foul ? "Foul" : "Tackle",
                                     result.tackle_angle_.degree() );

    double tackle_dir = ( result.tackle_angle_ - wm.self().body() ).degree();

    agent->doTackle( tackle_dir, use_foul );
    agent->setNeckAction( new Neck_TurnToPoint( ball_next ) );

    return true;
}
