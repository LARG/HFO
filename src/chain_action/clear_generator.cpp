// -*-c++-*-

/*!
  \file clear_generator.cpp
  \brief clear kick generator Source File
*/

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

#include "clear_generator.h"

#include "field_analyzer.h"
#include "clear_ball.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <limits>
#include <cmath>

#define DEBUG_PROFILE
#define DEBUG_PRINT
#define DEBUG_PREDICT_OPPONENT_REACH_STEP
// #define DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ClearGenerator::ClearGenerator()
    : M_update_time( 0, 0 )
{
    M_courses.reserve( 360 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
ClearGenerator &
ClearGenerator::instance()
{
#ifdef __APPLE__
    static ClearGenerator s_instance;
#else
    static thread_local ClearGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ClearGenerator::clear()
{
    M_total_count = 0;
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ClearGenerator::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
    {
        return;
    }

    if ( wm.time().stopped() > 0
         || wm.gameMode().type() == GameMode::KickOff_
         || wm.gameMode().type() == GameMode::CornerKick_
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    createCourses( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::CLEAR,
                  __FILE__": (generate) PROFILE total_count=%d elapsed %.3f [ms]",
                  M_total_count,
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ClearGenerator::createCourses( const WorldModel & wm )
{
    static const double SPEED_STEP = 0.3;
    static const double MIN_FIRST_SPEED
        = std::min( ServerParam::i().ballSpeedMax() - SPEED_STEP*5,
                    ServerParam::i().defaultPlayerSpeedMax() + 0.5 );

    const ServerParam & SP = ServerParam::i();
    const Vector2D left_back_corner( -SP.pitchHalfLength(),
                                     -SP.pitchHalfWidth() );
    const Vector2D right_back_corner( -SP.pitchHalfLength(),
                                      +SP.pitchHalfWidth() );

    const double min_angle = std::max( -110.0,
                                       ( left_back_corner - wm.ball().pos() ).th().degree() );
    const double max_angle = std::min( +110.0,
                                       ( right_back_corner - wm.ball().pos() ).th().degree() );

    const double angle_step = std::max( 1.0,
                                        ( max_angle - min_angle ) / 90 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::CLEAR,
                  "Clear: minAngle=%.1f maxAngle=%.1f angleStep=%.2f",
                  min_angle, max_angle, angle_step );
#endif

    int one_kick_max_opponent_reach_step = 0;
    double one_kick_best_speed = -1.0;
    AngleDeg one_kick_best_angle = 0.0;

    int max_opponent_reach_step = 0;
    double best_speed = -1.0;
    AngleDeg best_angle = 0.0;
    int best_kick_count = 0;

    for ( double a = min_angle; a < max_angle + 0.001; a += angle_step )
    {
        const AngleDeg target_angle = a;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::CLEAR,
                      "===== Clear: angle=%.1f ===== ",
                      target_angle.degree() );
#endif
        //
        // check accuracy of clear angle
        //
#if 0
        {
            int dir_max_count = 0;
            wm.dirRangeCount( target_angle,
                              30.0, // Magic Number
                              &dir_max_count, NULL, NULL );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "Clear: angle=%.1f dirMaxCount=%d",
                          target_angle.degree(), dir_max_count );
#endif
            if ( dir_max_count >= 20 )
            {
                continue;
            }
        }
#endif

        const Vector2D one_step_kick_max_vel
            = KickTable::calc_max_velocity( target_angle,
                                            wm.self().kickRate(),
                                            wm.ball().vel() );
        double ball_speed = one_step_kick_max_vel.r();
        int kick_count = 1;

        if ( ball_speed < MIN_FIRST_SPEED )
        {
            ball_speed = MIN_FIRST_SPEED;
            kick_count = 2;
        }

        bool loop = true;
        ball_speed -= SPEED_STEP;
        while ( loop )
        {
            ++M_total_count;

            ball_speed += SPEED_STEP;
            if ( ball_speed > SP.ballSpeedMax() )
            {
                loop = false;
                ball_speed = SP.ballSpeedMax();
            }

            int opponent_reach_step = predictOpponentsReachStep( wm,
                                                                 wm.ball().pos(),
                                                                 ball_speed,
                                                                 target_angle );
            if ( opponent_reach_step > max_opponent_reach_step )
            {
                max_opponent_reach_step = opponent_reach_step;
                best_speed = ball_speed;
                best_angle = target_angle;
                best_kick_count = kick_count;
            }

            if ( kick_count == 1 )
            {
                if ( opponent_reach_step > one_kick_max_opponent_reach_step )
                {
                    one_kick_max_opponent_reach_step = opponent_reach_step;
                    one_kick_best_speed = ball_speed;
                    one_kick_best_angle = target_angle;
                }
            }

#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "%d Clear: angle=%.1f speed=%.2f nKick=%d opponentStep=%d",
                          M_total_count,
                          target_angle.degree(),
                          ball_speed,
                          kick_count,
                          opponent_reach_step );
#endif

            if ( wm.gameMode().type() != GameMode::PlayOn )
            {
                break;
            }

            kick_count = 2;
            if ( ball_speed > 2.5 ) kick_count = 3;
        }
    }


    if ( best_speed > 0.0 )
    {
        {
            double move_dist = inertia_n_step_distance( best_speed,
                                                        max_opponent_reach_step,
                                                        SP.ballDecay() );
            Vector2D target_point
                = wm.ball().pos()
                + Vector2D::from_polar( move_dist, best_angle );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "<<<< Clear: BestResult(N) angle=%.1f speed=%.2f nKick=%d opponentStep=%d",
                          best_angle.degree(),
                          best_speed,
                          best_kick_count,
                          max_opponent_reach_step );
            dlog.addLine( Logger::CLEAR,
                          wm.ball().pos(), target_point,
                          "#ff0000" );
            dlog.addRect( Logger::CLEAR,
                          target_point.x - 0.2, target_point.y - 0.2,
                          0.4, 0.4,
                          "#ff0000" );
#endif
            CooperativeAction::Ptr ptr( new ClearBall( wm.self().unum(),
                                                       target_point,
                                                       best_speed,
                                                       max_opponent_reach_step,
                                                       best_kick_count,
                                                       "clearN" ) );
            M_courses.push_back( ptr );
        }

        if ( best_kick_count > 1 )
        {
            double move_dist = inertia_n_step_distance( one_kick_best_speed,
                                                        one_kick_max_opponent_reach_step,
                                                        SP.ballDecay() );
            Vector2D target_point
                = wm.ball().pos()
                + Vector2D::from_polar( move_dist, one_kick_best_angle );
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CLEAR,
                          "<<<< Clear: BestResult(1) angle=%.1f speed=%.2f nKick=1 opponentStep=%d",
                          one_kick_best_angle.degree(),
                          one_kick_best_speed,
                          one_kick_max_opponent_reach_step );
            dlog.addLine( Logger::CLEAR,
                          wm.ball().pos(), target_point,
                          "#ff0000" );
            dlog.addRect( Logger::CLEAR,
                          target_point.x - 0.2, target_point.y - 0.2,
                          0.4, 0.4,
                          "#ff0000" );
#endif
            CooperativeAction::Ptr ptr( new ClearBall( wm.self().unum(),
                                                       target_point,
                                                       one_kick_best_speed,
                                                       one_kick_max_opponent_reach_step,
                                                       1,
                                                       "clear1" ) );
            M_courses.push_back( ptr );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
namespace {

struct OpponentAngleCompare {
    const AngleDeg ball_move_angle_;
    OpponentAngleCompare( const AngleDeg & angle )
        : ball_move_angle_( angle )
      { }

    bool operator()( const AbstractPlayerObject * lhs,
                     const AbstractPlayerObject * rhs ) const
      {
          return ( lhs->angleFromBall() - ball_move_angle_ ).abs()
              < ( rhs->angleFromBall() - ball_move_angle_ ).abs();
      }
};

}

/*-------------------------------------------------------------------*/
/*!

 */
int
ClearGenerator::predictOpponentsReachStep( const WorldModel & wm,
                                           const Vector2D & first_ball_pos,
                                           const double & first_ball_speed,
                                           const AngleDeg & ball_move_angle )
{
    // AbstractPlayerCont opponents = wm.allOpponents();
    // std::sort( opponents.begin(), opponents.end(),
    //            OpponentAngleCompare( ball_move_angle ) );


    const Vector2D first_ball_vel = Vector2D::from_polar( first_ball_speed, ball_move_angle );

    int min_step = 50;
    int out_of_pitch_step = -1;

    for ( AbstractPlayerCont::const_iterator
              o = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          o != end;
          ++o )
    {
        int step = predictOpponentReachStep( *o,
                                             first_ball_pos,
                                             first_ball_vel,
                                             ball_move_angle,
                                             min_step );
        if ( step > 1000 )
        {
            out_of_pitch_step = step - 1000;
            if ( out_of_pitch_step < min_step )
            {
                min_step = out_of_pitch_step;
            }
        }
        else if ( step < min_step )
        {
            out_of_pitch_step = -1;
            min_step = step;
        }
    }

    if ( out_of_pitch_step > 0 )
    {
        return 1000 + out_of_pitch_step;
    }

    return min_step;
}


/*-------------------------------------------------------------------*/
/*!

 */
int
ClearGenerator::predictOpponentReachStep( const AbstractPlayerObject * opponent,
                                          const Vector2D & first_ball_pos,
                                          const Vector2D & first_ball_vel,
                                          const AngleDeg & ball_move_angle,
                                          const int max_cycle )
{
    const ServerParam & SP = ServerParam::i();


    const PlayerType * ptype = opponent->playerTypePtr();
    const double opponent_speed = opponent->vel().r();

    int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( opponent->pos(),
                                                             ptype->realSpeedMax(),
                                                             first_ball_pos,
                                                             ball_move_angle );
    if ( min_cycle < 0 )
    {
        min_cycle = 10;
    }

    for ( int cycle = min_cycle; cycle <= max_cycle; ++cycle )
    {
        Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                                  first_ball_vel,
                                                  cycle,
                                                  SP.ballDecay() );

        if ( ball_pos.absX() > SP.pitchHalfLength()
             || ball_pos.absY() > SP.pitchHalfWidth() )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "____ opponent=%d(%.1f %.1f) step=%d ball is out of pitch. ",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle  );
#endif
            return 1000 + cycle;
        }

        Vector2D inertia_pos = opponent->inertiaPoint( cycle );
        double target_dist = inertia_pos.dist( ball_pos );

        if ( target_dist - ptype->kickableArea() - 0.15 < 0.001 )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "____ opponent=%d(%.1f %.1f) step=%d already there. dist=%.1f",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle,
                          target_dist );
#endif
            return cycle;
        }

        double dash_dist = target_dist;
        if ( cycle > 1 )
        {
            dash_dist -= ptype->kickableArea();
            dash_dist -= 0.5; // special bonus
        }

        if ( dash_dist > ptype->realSpeedMax() * cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2
            dlog.addText( Logger::CLEAR,
                          "______ opponent=%d(%.1f %.1f) cycle=%d dash_dist=%.1f reachable=%.1f",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, dash_dist, ptype->realSpeedMax()*cycle );
#endif
            continue;
        }

        //
        // dash
        //

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP_LEVEL2
            dlog.addText( Logger::CLEAR,
                          "______ opponent=%d(%.1f %.1f) cycle=%d dash_dist=%.1f n_dash=%d",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, dash_dist, n_dash );
#endif
            continue;
        }

        //
        // turn
        //
        int n_turn = ( opponent->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   opponent->body(),
                                                                   opponent_speed,
                                                                   target_dist,
                                                                   ( ball_pos - inertia_pos ).th(),
                                                                   ptype->kickableArea(),
                                                                   true ) );

        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 ); // 1 step penalty for observation delay
        if ( opponent->isTackling() )
        {
            n_step += 5; // Magic Number
        }

        n_step -= std::min( 3, opponent->posCount() );

        if ( n_step <= cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::CLEAR,
                          "____ opponent=%d(%.1f %.1f) step=%d(t:%d,d:%d)",
                          opponent->unum(),
                          opponent->pos().x, opponent->pos().y,
                          cycle, n_turn, n_dash );
#endif
            return cycle;
        }

    }

    return 1000;
}
