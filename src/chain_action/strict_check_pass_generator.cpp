// -*-c++-*-

/*!
  \file strict_check_pass_generator.cpp
  \brief strict checked pass course generator Source File
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

#include "strict_check_pass_generator.h"

#include "pass.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <cmath>

#define DEBUG_PROFILE

// #define DEBUG_PRINT_COMMON

// #define DEBUG_UPDATE_PASSER
// #define DEBUG_UPDATE_RECEIVERS
// #define DEBUG_UPDATE_OPPONENT
// #define DEBUG_DIRECT_PASS
// #define DEBUG_LEADING_PASS
// #define DEBUG_THROUGH_PASS

// #define DEBUG_PREDICT_RECEIVER
// #define DEBUG_PREDICT_OPPONENT_REACH_STEP

// #define DEBUG_PRINT_SUCCESS_PASS
// #define DEBUG_PRINT_FAILED_PASS


// #define CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT

using namespace rcsc;

namespace {

inline
void
debug_paint_failed_pass( const int count,
                         const Vector2D & receive_point )
{
    dlog.addRect( Logger::PASS,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
                  "#ff0000" );
    if ( count >= 0 )
    {
        char num[8];
        snprintf( num, 8, "%d", count );
        dlog.addMessage( Logger::PASS,
                         receive_point, num );
    }
}

}

/*-------------------------------------------------------------------*/
/*!

 */
StrictCheckPassGenerator::Receiver::Receiver( const AbstractPlayerObject * p,
                                              const Vector2D & first_ball_pos )
    : player_( p ),
      pos_( p->seenPosCount() <= p->posCount() ? p->seenPos() : p->pos() ),
      vel_( p->seenVelCount() <= p->velCount() ? p->seenVel() : p->vel() ),
      inertia_pos_( p->playerTypePtr()->inertiaFinalPoint( pos_, vel_ ) ),
      speed_( vel_.r() ),
      penalty_distance_( FieldAnalyzer::estimate_virtual_dash_distance( p ) ),
      penalty_step_( p->playerTypePtr()->cyclesToReachDistance( penalty_distance_ ) ),
      angle_from_ball_( ( p->pos() - first_ball_pos ).th() )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
StrictCheckPassGenerator::Opponent::Opponent( const AbstractPlayerObject * p )
    : player_( p ),
      pos_( p->seenPosCount() <= p->posCount() ? p->seenPos() : p->pos() ),
      vel_( p->seenVelCount() <= p->velCount() ? p->seenVel() : p->vel() ),
      speed_( vel_.r() ),
      bonus_distance_( FieldAnalyzer::estimate_virtual_dash_distance( p ) )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
StrictCheckPassGenerator::StrictCheckPassGenerator()
    : M_update_time( -1, 0 ),
      M_total_count( 0 ),
      M_pass_type( '-' ),
      M_passer( static_cast< AbstractPlayerObject * >( 0 ) ),
      M_start_time( -1, 0 )
{
    M_receiver_candidates.reserve( 11 );
    M_opponents.reserve( 16 );
    M_courses.reserve( 1024 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
StrictCheckPassGenerator &
StrictCheckPassGenerator::instance()
{
#ifdef __APPLE__
    static StrictCheckPassGenerator s_instance;
#else
    static thread_local StrictCheckPassGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::clear()
{
    M_total_count = 0;
    M_pass_type = '-';
    M_passer = static_cast< AbstractPlayerObject * >( 0 );
    M_start_time.assign( -1, 0 );
    M_first_point.invalidate();
    M_receiver_candidates.clear();
    M_opponents.clear();
    M_direct_size = M_leading_size = M_through_size = 0;
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.time().stopped() > 0
         || wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    updatePasser( wm );

    if ( ! M_passer
         || ! M_first_point.isValid() )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) passer not found." );
        return;
    }

    updateReceivers( wm );

    if ( M_receiver_candidates.empty() )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) no receiver." );
        return;
    }

    updateOpponents( wm );

    createCourses( wm );

    std::sort( M_courses.begin(), M_courses.end(),
               CooperativeAction::DistCompare( ServerParam::i().theirTeamGoalPos() ) );

#ifdef DEBUG_PROFILE
    if ( M_passer->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (generate) PROFILE passer=self size=%d/%d D=%d L=%d T=%d elapsed %f [ms]",
                      (int)M_courses.size(),
                      M_total_count,
                      M_direct_size, M_leading_size, M_through_size,
                      timer.elapsedReal() );
    }
    else
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (update) PROFILE passer=%d size=%d/%d D=%d L=%d T=%d elapsed %f [ms]",
                      M_passer->unum(),
                      (int)M_courses.size(),
                      M_total_count,
                      M_direct_size, M_leading_size, M_through_size,
                      timer.elapsedReal() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::updatePasser( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         && ! wm.self().isFrozen() )
    {
        M_passer = &wm.self();
        M_start_time = wm.time();
        M_first_point = wm.ball().pos();
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) self kickable." );
#endif
        return;
    }

    int s_min = wm.interceptTable()->selfReachCycle();
    int t_min = wm.interceptTable()->teammateReachCycle();
    int o_min = wm.interceptTable()->opponentReachCycle();

    int our_min = std::min( s_min, t_min );
    if ( o_min < std::min( our_min - 4, (int)rint( our_min * 0.9 ) ) )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) opponent ball." );
        return;
    }

    if ( s_min <= t_min )
    {
        if ( s_min <= 2 )
        {
            M_passer = &wm.self();
            M_first_point = wm.ball().inertiaPoint( s_min );
        }
    }
    else
    {
        if ( t_min <= 2 )
        {
            M_passer = wm.interceptTable()->fastestTeammate();
            M_first_point = wm.ball().inertiaPoint( t_min );
        }
    }

    if ( ! M_passer )
    {
        dlog.addText( Logger::PASS,
                      __FILE__" (updatePasser) no passer." );
        return;
    }

    M_start_time = wm.time();
    if ( ! wm.gameMode().isServerCycleStoppedMode() )
    {
        M_start_time.addCycle( t_min );
    }

    if ( M_passer->unum() != wm.self().unum() )
    {
        if ( M_first_point.dist2( wm.self().pos() ) > std::pow( 30.0, 2 ) )
        {
            M_passer = static_cast< const AbstractPlayerObject * >( 0 );
            dlog.addText( Logger::PASS,
                          __FILE__" (updatePasser) passer is too far." );
            return;
        }
    }

#ifdef DEBUG_UPDATE_PASSER
    dlog.addText( Logger::PASS,
                  __FILE__" (updatePasser) passer=%d(%.1f %.1f) reachStep=%d startPos=(%.1f %.1f)",
                  M_passer->unum(),
                  M_passer->pos().x, M_passer->pos().y,
                  t_min,
                  M_first_point.x, M_first_point.y );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
struct ReceiverAngleCompare {

    bool operator()( const StrictCheckPassGenerator::Receiver & lhs,
                     const StrictCheckPassGenerator::Receiver & rhs ) const
      {
          return lhs.angle_from_ball_.degree() < rhs.angle_from_ball_.degree();
      }
};

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
struct ReceiverDistCompare {

    const Vector2D pos_;

    ReceiverDistCompare( const Vector2D & pos )
        : pos_( pos )
      { }

    bool operator()( const StrictCheckPassGenerator::Receiver & lhs,
                     const StrictCheckPassGenerator::Receiver & rhs ) const
      {
          return lhs.pos_.dist2( pos_ ) < rhs.pos_.dist2( pos_ );
      }
};
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::updateReceivers( const WorldModel & wm )
{
    const ServerParam & SP = ServerParam::i();
    const double max_dist2 = std::pow( 40.0, 2 ); // Magic Number

    const bool is_self_passer = ( M_passer->unum() == wm.self().unum() );

    for ( AbstractPlayerCont::const_iterator
              p = wm.ourPlayers().begin(),
              end = wm.ourPlayers().end();
          p != end;
          ++p )
    {
        if ( *p == M_passer ) continue;

        if ( is_self_passer )
        {
            // if ( (*p)->isGhost() ) continue;
            if ( (*p)->unum() == Unum_Unknown ) continue;
            // if ( (*p)->unumCount() > 10 ) continue;
            if ( (*p)->posCount() > 10 ) continue;
            if ( (*p)->isTackling() ) continue;
            if ( (*p)->pos().x > wm.offsideLineX() )
            {
                dlog.addText( Logger::PASS,
                              "(updateReceiver) unum=%d (%.2f %.2f) > offside=%.2f",
                              (*p)->unum(),
                              (*p)->pos().x, (*p)->pos().y,
                              wm.offsideLineX() );
                continue;
            }
            //if ( (*p)->isTackling() ) continue;
            if ( (*p)->goalie()
                 && (*p)->pos().x < SP.ourPenaltyAreaLineX() + 15.0 )
            {
                continue;
            }
        }
        else
        {
            // ignore other players
            if ( (*p)->unum() != wm.self().unum() )
            {
                continue;
            }
        }

        if ( (*p)->pos().dist2( M_first_point ) > max_dist2 ) continue;

        M_receiver_candidates.push_back( Receiver( *p, M_first_point ) );
    }

    std::sort( M_receiver_candidates.begin(),
               M_receiver_candidates.end(),
               ReceiverDistCompare( SP.theirTeamGoalPos() ) );

    // std::sort( M_receiver_candidates.begin(),
    //            M_receiver_candidates.end(),
    //            ReceiverAngleCompare() );

#ifdef DEBUG_UPDATE_RECEIVERS
    for ( ReceiverCont::const_iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        dlog.addText( Logger::PASS,
                      "StrictPass receiver %d pos(%.1f %.1f) inertia=(%.1f %.1f) vel(%.2f %.2f)"
                      " penalty_dist=%.3f penalty_step=%d"
                      " angle=%.1f",
                      p->player_->unum(),
                      p->pos_.x, p->pos_.y,
                      p->inertia_pos_.x, p->inertia_pos_.y,
                      p->vel_.x, p->vel_.y,
                      p->penalty_distance_,
                      p->penalty_step_,
                      p->angle_from_ball_.degree() );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::updateOpponents( const WorldModel & wm )
{
    for ( AbstractPlayerCont::const_iterator
              p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        M_opponents.push_back( Opponent( *p ) );
#ifdef DEBUG_UPDATE_OPPONENT
        const Opponent & o = M_opponents.back();
        dlog.addText( Logger::PASS,
                      "StrictPass opp %d pos(%.1f %.1f) vel(%.2f %.2f) bonus_dist=%.3f",
                      o.player_->unum(),
                      o.pos_.x, o.pos_.y,
                      o.vel_.x, o.vel_.y,
                      o.bonus_distance_ );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::createCourses( const WorldModel & wm )
{
    const ReceiverCont::iterator end = M_receiver_candidates.end();

    M_pass_type = 'D';
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        createDirectPass( wm, *p );
    }

    M_pass_type = 'L';
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        createLeadingPass( wm, *p );
    }

    M_pass_type = 'T';
    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != end;
          ++p )
    {
        createThroughPass( wm, *p );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::createDirectPass( const WorldModel & wm,
                                            const Receiver & receiver )
{
    static const int MIN_RECEIVE_STEP = 3;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 15; // Magic Number
#endif

    static const double MIN_DIRECT_PASS_DIST
        = ServerParam::i().defaultKickableArea() * 2.2;
    static const double MAX_DIRECT_PASS_DIST
        = 0.8 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    const ServerParam & SP = ServerParam::i();

    //
    // check receivable area
    //
    if ( receiver.pos_.x > SP.pitchHalfLength() - 1.5
         || receiver.pos_.x < - SP.pitchHalfLength() + 5.0
         || receiver.pos_.absY() > SP.pitchHalfWidth() - 1.5 )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d outOfBounds pos=(%.2f %.2f)",
                      M_total_count, receiver.player_->unum(),
                      receiver.pos_.x, receiver.pos_.y );
#endif
        return;
    }

    //
    // avoid dangerous area
    //
    if ( receiver.pos_.x < M_first_point.x + 1.0
         && receiver.pos_.dist2( SP.ourTeamGoalPos() ) < std::pow( 18.0, 2 ) )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d dangerous pos=(%.2f %.2f)",
                      M_total_count, receiver.player_->unum(),
                      receiver.pos_.x, receiver.pos_.y );
#endif
        return;
    }

    const PlayerType * ptype = receiver.player_->playerTypePtr();

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = SP.defaultRealSpeedMax();

    const Vector2D receive_point = receiver.inertia_pos_;

    const double ball_move_dist = M_first_point.dist( receive_point );

    if ( ball_move_dist < MIN_DIRECT_PASS_DIST
         || MAX_DIRECT_PASS_DIST < ball_move_dist )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                      M_total_count, receiver.player_->unum(),
                      ball_move_dist,
                      MIN_DIRECT_PASS_DIST, MAX_DIRECT_PASS_DIST );
#endif
        return;
    }

    if ( wm.gameMode().type() == GameMode::GoalKick_
         && receive_point.x < SP.ourPenaltyAreaLineX() + 1.0
         && receive_point.absY() < SP.penaltyAreaHalfWidth() + 1.0 )
    {
#ifdef DEBUG_DIRECT_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (direct) unum=%d, goal_kick",
                      M_total_count, receiver.player_->unum() );
#endif
        return;
    }

    //
    // decide ball speed range
    //

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.8 );
    const double min_receive_ball_speed = ptype->realSpeedMax();

    const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

    const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );


#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
#endif

    const int start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                               min_ball_step ),
                                     receiver.penalty_step_ );
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 2 );
#else
    const int max_step = start_step + 2;
#endif

#ifdef DEBUG_DIRECT_PASS
    dlog.addText( Logger::PASS,
                  "=== (direct) unum=%d stepRange=[%d, %d]",
                  receiver.player_->unum(),
                  start_step, max_step );
#endif

    createPassCommon( wm,
                      receiver, receive_point,
                      start_step, max_step,
                      min_ball_speed, max_ball_speed,
                      min_receive_ball_speed, max_receive_ball_speed,
                      ball_move_dist, ball_move_angle,
                      "strictDirect" );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::createLeadingPass( const WorldModel & wm,
                                             const Receiver & receiver )
{
    static const double OUR_GOAL_DIST_THR2 = std::pow( 16.0, 2 );

    static const int MIN_RECEIVE_STEP = 4;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 20;
#endif

    static const double MIN_LEADING_PASS_DIST = 3.0;
    static const double MAX_LEADING_PASS_DIST
        = 0.8 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    static const int ANGLE_DIVS = 24;
    static const double ANGLE_STEP = 360.0 / ANGLE_DIVS;
    static const double DIST_DIVS = 4;
    static const double DIST_STEP = 1.1;

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = receiver.player_->playerTypePtr();

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = SP.defaultRealSpeedMax();

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.5 );
    const double min_receive_ball_speed = 0.001;

    const Vector2D our_goal = SP.ourTeamGoalPos();

#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
    success_counts.reserve( 16 );
#endif

    //
    // distance loop
    //
    for ( int d = 1; d <= DIST_DIVS; ++d )
    {
        const double player_move_dist = DIST_STEP * d;
        const int a_step = ( player_move_dist * 2.0 * M_PI / ANGLE_DIVS < 0.6
                             ? 2
                             : 1 );
        // const int move_dist_penalty_step
        //     = static_cast< int >( std::floor( player_move_dist * 0.3 ) );

        //
        // angle loop
        //
        for ( int a = 0; a < ANGLE_DIVS; a += a_step )
        {
            ++M_total_count;

            const AngleDeg angle = receiver.angle_from_ball_ + ANGLE_STEP*a;
            const Vector2D receive_point
                = receiver.inertia_pos_
                + Vector2D::from_polar( player_move_dist, angle );

            int move_dist_penalty_step = 0;
            {
                Line2D ball_move_line( M_first_point, receive_point );
                double player_line_dist = ball_move_line.dist( receiver.pos_ );

                move_dist_penalty_step = static_cast< int >( std::floor( player_line_dist * 0.3 ) );
            }

#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::PASS,
                          ">>>> (lead) unum=%d receivePoint=(%.1f %.1f)",
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
#endif

            if ( receive_point.x > SP.pitchHalfLength() - 3.0
                 || receive_point.x < -SP.pitchHalfLength() + 5.0
                 || receive_point.absY() > SP.pitchHalfWidth() - 3.0 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d outOfBounds pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                continue;
            }

            if ( receive_point.x < M_first_point.x
                 && receive_point.dist2( our_goal ) < OUR_GOAL_DIST_THR2 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d our goal is near pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                continue;
            }

            if ( wm.gameMode().type() == GameMode::GoalKick_
                 && receive_point.x < SP.ourPenaltyAreaLineX() + 1.0
                 && receive_point.absY() < SP.penaltyAreaHalfWidth() + 1.0 )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d, goal_kick",
                              M_total_count, receiver.player_->unum() );
#endif
                return;
            }

            const double ball_move_dist = M_first_point.dist( receive_point );

            if ( ball_move_dist < MIN_LEADING_PASS_DIST
                 || MAX_LEADING_PASS_DIST < ball_move_dist )
            {
#ifdef DEBUG_LEADING_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (lead) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                              M_total_count, receiver.player_->unum(),
                              ball_move_dist,
                              MIN_LEADING_PASS_DIST, MAX_LEADING_PASS_DIST );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                continue;
            }

            {
                int nearest_receiver_unum = getNearestReceiverUnum( receive_point );
                if ( nearest_receiver_unum != receiver.player_->unum() )
                {
#ifdef DEBUG_LEADING_PASS
                    dlog.addText( Logger::PASS,
                                  "%d: xxx (lead) unum=%d otherReceiver=%d pos=(%.2f %.2f)",
                                  M_total_count, receiver.player_->unum(),
                                  nearest_receiver_unum,
                                  receive_point.x, receive_point.y );
                    debug_paint_failed_pass( M_total_count, receive_point );
#endif
                    break;
                }
            }

            const int receiver_step = predictReceiverReachStep( receiver,
                                                                receive_point,
                                                                true )
                + move_dist_penalty_step;
            const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

            const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );

#ifdef DEBUG_PRINT_SUCCESS_PASS
            success_counts.clear();
#endif

            const int start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                                       min_ball_step ),
                                             receiver_step );

// #ifdef DEBUG_LEADING_PASS
//             dlog.addText( Logger::PASS,
//                           "=== (lead) unum=%d MIN_RECEIVE_STEP=%d"
//                           " min_ball_step=%d"
//                           " receiver_step=%d",
//                           receiver.player_->unum(),
//                           MIN_RECEIVE_STEP, min_ball_step, receiver_step );
// #endif

#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
            const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 3 );
#else
            const int max_step = start_step + 3;
#endif

#ifdef DEBUG_LEADING_PASS
            dlog.addText( Logger::PASS,
                          "=== (lead) receiver=%d"
                          " receivePos=(%.1f %.1f)",
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
            dlog.addText( Logger::PASS,
                          "__ ballMove=%.3f moveAngle=%.1f",
                          ball_move_dist, ball_move_angle.degree() );
            dlog.addText( Logger::PASS,
                          "__ stepRange=[%d, %d] receiverStep=%d(penalty=%d)",
                          start_step, max_step, receiver_step, move_dist_penalty_step );
#endif

            createPassCommon( wm,
                              receiver, receive_point,
                              start_step, max_step,
                              min_ball_speed, max_ball_speed,
                              min_receive_ball_speed, max_receive_ball_speed,
                              ball_move_dist, ball_move_angle,
                              "strictLead" );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::createThroughPass( const WorldModel & wm,
                                             const Receiver & receiver )
{
    static const int MIN_RECEIVE_STEP = 6;
#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
    static const int MAX_RECEIVE_STEP = 35;
#endif

    static const double MIN_THROUGH_PASS_DIST = 5.0;
    static const double MAX_THROUGH_PASS_DIST
        = 0.9 * inertia_final_distance( ServerParam::i().ballSpeedMax(),
                                        ServerParam::i().ballDecay() );
    static const double MAX_RECEIVE_BALL_SPEED
        = ServerParam::i().ballSpeedMax()
        * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    static const int ANGLE_DIVS = 14;
    static const double MIN_ANGLE = -40.0;
    static const double MAX_ANGLE = +40.0;
    static const double ANGLE_STEP = ( MAX_ANGLE - MIN_ANGLE ) / ANGLE_DIVS;

    static const double MIN_MOVE_DIST = 6.0;
    static const double MAX_MOVE_DIST = 30.0 + 0.001;
    static const double MOVE_DIST_STEP = 2.0;

    const ServerParam & SP = ServerParam::i();
    const PlayerType * ptype = receiver.player_->playerTypePtr();
    const AngleDeg receiver_vel_angle = receiver.vel_.th();

    const double min_receive_x = std::min( std::min( std::max( 10.0, M_first_point.x + 10.0 ),
                                                     wm.offsideLineX() - 10.0 ),
                                           SP.theirPenaltyAreaLineX() - 5.0 );

    if ( receiver.pos_.x < min_receive_x - MAX_MOVE_DIST
         || receiver.pos_.x < 1.0 )
    {
#ifdef DEBUG_THROUGH_PASS
        dlog.addText( Logger::PASS,
                      "%d: xxx (through) unum=%d too back.",
                      M_total_count, receiver.player_->unum() );
#endif
        return;
    }

    //
    // initialize ball speed range
    //

    const double max_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                    ? SP.ballSpeedMax()
                                    : wm.self().isKickable()
                                    ? wm.self().kickRate() * SP.maxPower()
                                    : SP.kickPowerRate() * SP.maxPower() );
    const double min_ball_speed = 1.4; //SP.defaultPlayerSpeedMax();

    const double max_receive_ball_speed
        = std::min( MAX_RECEIVE_BALL_SPEED,
                    ptype->kickableArea() + ( SP.maxDashPower()
                                              * ptype->dashPowerRate()
                                              * ptype->effortMax() ) * 1.5 );
    const double min_receive_ball_speed = 0.001;

    //
    // check communication
    //

    bool pass_requested = false;
    AngleDeg requested_move_angle = 0.0;
    if ( wm.audioMemory().passRequestTime().cycle() > wm.time().cycle() - 10 ) // Magic Number
    {
        for ( std::vector< AudioMemory::PassRequest >::const_iterator it = wm.audioMemory().passRequest().begin();
              it != wm.audioMemory().passRequest().end();
              ++it )
        {
            if ( it->sender_ == receiver.player_->unum() )
            {
                pass_requested = true;
                requested_move_angle = ( it->pos_ - receiver.inertia_pos_ ).th();
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: (through) receiver=%d pass requested",
                              M_total_count, receiver.player_->unum() );
#endif
                break;
            }
        }
    }

    //
    //
    //
#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
    success_counts.reserve( 16 );
#endif

    //
    // angle loop
    //
    for ( int a = 0; a <= ANGLE_DIVS; ++a )
    {
        const AngleDeg angle = MIN_ANGLE + ( ANGLE_STEP * a );
        const Vector2D unit_rvec = Vector2D::from_polar( 1.0, angle );

        //
        // distance loop
        //
        for ( double move_dist = MIN_MOVE_DIST;
              move_dist < MAX_MOVE_DIST;
              move_dist += MOVE_DIST_STEP )
        {
            ++M_total_count;

            const Vector2D receive_point
                = receiver.inertia_pos_
                + unit_rvec * move_dist;

#ifdef DEBUG_THROUGH_PASS
            dlog.addText( Logger::PASS,
                          ">>>> (through) receiver=%d receivePoint=(%.1f %.1f)",
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y );
#endif

            if ( receive_point.x < min_receive_x )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d tooSmallX pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                continue;
            }

            if ( receive_point.x > SP.pitchHalfLength() - 1.5
                 || receive_point.absY() > SP.pitchHalfWidth() - 1.5 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d outOfBounds pos=(%.2f %.2f)",
                              M_total_count, receiver.player_->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                break;
            }

            const double ball_move_dist = M_first_point.dist( receive_point );

            if ( ball_move_dist < MIN_THROUGH_PASS_DIST
                 || MAX_THROUGH_PASS_DIST < ball_move_dist )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: xxx (through) unum=%d overBallMoveDist=%.3f minDist=%.3f maxDist=%.3f",
                              M_total_count, receiver.player_->unum(),
                              ball_move_dist,
                              MIN_THROUGH_PASS_DIST, MAX_THROUGH_PASS_DIST );
                debug_paint_failed_pass( M_total_count, receive_point );
#endif
                continue;
            }

            {
                int nearest_receiver_unum = getNearestReceiverUnum( receive_point );
                if ( nearest_receiver_unum != receiver.player_->unum() )
                {
#ifdef DEBUG_THROUGH_PASS
                    dlog.addText( Logger::PASS,
                                  "%d: xxx (through) unum=%d otherReceiver=%d pos=(%.2f %.2f)",
                                  M_total_count, receiver.player_->unum(),
                                  nearest_receiver_unum,
                                  receive_point.x, receive_point.y );
                    debug_paint_failed_pass( M_total_count, receive_point );
#endif
                    break;
                }            }


            const int receiver_step = predictReceiverReachStep( receiver,
                                                                receive_point,
                                                                false );
            const AngleDeg ball_move_angle = ( receive_point - M_first_point ).th();

#ifdef DEBUG_PRINT_SUCCESS_PASS
            success_counts.clear();
#endif

            int start_step = receiver_step;
            if ( pass_requested
                 && ( requested_move_angle - angle ).abs() < 20.0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: matched with requested pass. angle=%.1f",
                              M_total_count, angle.degree() );
#endif
            }
            // if ( receive_point.x > wm.offsideLineX() + 5.0
            //      || ball_move_angle.abs() < 15.0 )
            else if ( receiver.speed_ > 0.2
                      && ( receiver_vel_angle - angle ).abs() < 15.0 )
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: matched with receiver velocity. angle=%.1f",
                              M_total_count, angle.degree() );
#endif
            }
            else
            {
#ifdef DEBUG_THROUGH_PASS
                dlog.addText( Logger::PASS,
                              "%d: receiver step. one step penalty",
                              M_total_count );
#endif
                start_step += 1;
                if ( ( receive_point.x > SP.pitchHalfLength() - 5.0
                       || receive_point.absY() > SP.pitchHalfWidth() - 5.0 )
                     && ball_move_angle.abs() > 30.0
                     && start_step >= 10 )
                {
                    start_step += 1;
                }
            }

            const int min_ball_step = SP.ballMoveStep( SP.ballSpeedMax(), ball_move_dist );

            start_step = std::max( std::max( MIN_RECEIVE_STEP,
                                             min_ball_step ),
                                   start_step );

#ifdef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
            const int max_step = std::max( MAX_RECEIVE_STEP, start_step + 3 );
#else
            const int max_step = start_step + 3;
#endif

#ifdef DEBUG_THROUGH_PASS
            dlog.addText( Logger::PASS,
                          "== (through) receiver=%d"
                          " ballPos=(%.1f %.1f) receivePos=(%.1f %.1f)",
                          receiver.player_->unum(),
                          M_first_point.x, M_first_point.y,
                          receive_point.x, receive_point.y );
            dlog.addText( Logger::PASS,
                          "== ballMove=%.3f moveAngle=%.1f",
                          ball_move_dist, ball_move_angle.degree() );
            dlog.addText( Logger::PASS,
                          "== stepRange=[%d, %d] receiverMove=%.3f receiverStep=%d",
                          start_step, max_step,
                          receiver.inertia_pos_.dist( receive_point ), receiver_step );
#endif

            createPassCommon( wm,
                              receiver, receive_point,
                              start_step, max_step,
                              min_ball_speed, max_ball_speed,
                              min_receive_ball_speed, max_receive_ball_speed,
                              ball_move_dist, ball_move_angle,
                              "strictThrough" );
        }

    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
StrictCheckPassGenerator::createPassCommon( const WorldModel & wm,
                                            const Receiver & receiver,
                                            const Vector2D & receive_point,
                                            const int min_step,
                                            const int max_step,
                                            const double & min_first_ball_speed,
                                            const double & max_first_ball_speed,
                                            const double & min_receive_ball_speed,
                                            const double & max_receive_ball_speed,
                                            const double & ball_move_dist,
                                            const AngleDeg & ball_move_angle,
                                            const char * description )
{
    const ServerParam & SP = ServerParam::i();

    int success_count = 0;
#ifdef DEBUG_PRINT_SUCCESS_PASS
    std::vector< int > success_counts;
    success_counts.reserve( max_step - min_step + 1 );
#endif

    for ( int step = min_step; step <= max_step; ++step )
    {
        ++M_total_count;

        double first_ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                               SP.ballDecay(),
                                                               step );

#if (defined DEBUG_PRINT_DIRECT_PASS) || (defined DEBUG_PRINT_LEADING_PASS) || (defined DEBUG_PRINT_THROUGH_PASS) || (defined DEBUG_PRINT_FAILED_PASS)
        dlog.addText( Logger::PASS,
                      "%d: type=%c unum=%d recvPos=(%.2f %.2f) step=%d ballMoveDist=%.2f speed=%.3f",
                      M_total_count, M_pass_type,
                      receiver.player_->unum(),
                      receive_point.x, receive_point.y,
                      step,
                      ball_move_dist,
                      first_ball_speed );
#endif


        if ( first_ball_speed < min_first_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f < min=%.3f",
                          M_total_count, M_pass_type,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          first_ball_speed, min_first_ball_speed );
#endif
            break;
        }

        if ( max_first_ball_speed < first_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f > max=%.3f",
                          M_total_count, M_pass_type,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          first_ball_speed, max_first_ball_speed );
#endif

            continue;
        }

        double receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), step );
        if ( receive_ball_speed < min_receive_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f < min=%.3f",
                          M_total_count, M_pass_type,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          receive_ball_speed, min_receive_ball_speed );
#endif
            break;
        }

        if ( max_receive_ball_speed < receive_ball_speed )
        {
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f > max=%.3f",
                          M_total_count, M_pass_type,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          receive_ball_speed, max_receive_ball_speed );
#endif
            continue;
        }

        int kick_count = FieldAnalyzer::predict_kick_count( wm,
                                                            M_passer,
                                                            first_ball_speed,
                                                            ball_move_angle );

        const AbstractPlayerObject * opponent = static_cast< const AbstractPlayerObject * >( 0 );
        int o_step = predictOpponentsReachStep( wm,
                                                M_first_point,
                                                first_ball_speed,
                                                ball_move_angle,
                                                receive_point,
                                                step + ( kick_count - 1 ) + 5,
                                                &opponent );

        bool failed = false;
        if ( M_pass_type == 'T' )
        {
            if ( o_step <= step )
            {
#ifdef DEBUG_THROUGH_PASS
                 dlog.addText( Logger::PASS,
                               "%d: ThroughPass failed???",
                               M_total_count );
#endif
                 failed = true;
            }

            if ( receive_point.x > 30.0
                 && step >= 15
                 && ( ! opponent
                      || ! opponent->goalie() )
                 && o_step >= step ) // Magic Number
            {
                AngleDeg receive_move_angle = ( receive_point - receiver.pos_ ).th();
                if ( ( receiver.player_->body() - receive_move_angle ).abs() < 15.0 )
                {
#ifdef DEBUG_THROUGH_PASS
                    dlog.addText( Logger::PASS,
                                  "%d: ********** ThroughPass reset failed flag",
                                  M_total_count );
#endif
                    failed = false;
                }
            }
        }
        else
        {
            if ( o_step <= step + ( kick_count - 1 ) )
            {
                failed = true;
            }
        }

        if ( failed )
        {
            // opponent can reach the ball faster than the receiver.
            // then, break the loop, because ball speed is decreasing in the loop.
#ifdef DEBUG_PRINT_FAILED_PASS
            dlog.addText( Logger::PASS,
                          "%d: xxx type=%c unum=%d (%.1f %.1f) step=%d >= opp[%d]Step=%d,"
                          " firstSpeed=%.3f recvSpeed=%.3f nKick=%d",
                          M_total_count, M_pass_type,
                          receiver.player_->unum(),
                          receive_point.x, receive_point.y,
                          step,
                          ( opponent ? opponent->unum() : 0 ), o_step,
                          first_ball_speed, receive_ball_speed,
                          kick_count );
#endif
            break;
        }

        CooperativeAction::Ptr pass( new Pass( M_passer->unum(),
                                               receiver.player_->unum(),
                                               receive_point,
                                               first_ball_speed,
                                               step + kick_count,
                                               kick_count,
                                               FieldAnalyzer::to_be_final_action( wm ),
                                               description ) );
        pass->setIndex( M_total_count );

        switch ( M_pass_type ) {
        case 'D':
            M_direct_size += 1;
            break;
        case 'L':
            M_leading_size += 1;
            break;
        case 'T':
            M_through_size += 1;
        default:
            break;
        }
        // if ( M_pass_type == 'L'
        //      && success_count > 0 )
        // {
        //     M_courses.pop_back();
        // }

        M_courses.push_back( pass );

#ifdef DEBUG_PRINT_SUCCESS_PASS
        dlog.addText( Logger::PASS,
                      "%d: ok type=%c unum=%d step=%d  opp[%d]Step=%d"
                      " nKick=%d ball=(%.1f %.1f) recv=(%.1f %.1f) "
                      " speed=%.3f->%.3f dir=%.1f",
                      M_total_count, M_pass_type,
                      receiver.player_->unum(),
                      step,
                      ( opponent ? opponent->unum() : 0 ),
                      o_step,
                      kick_count,
                      M_first_point.x, M_first_point.y,
                      receive_point.x, receive_point.y,
                      first_ball_speed,
                      receive_ball_speed,
                      ball_move_angle.degree() );
        success_counts.push_back( M_total_count );
#endif

#ifndef CREATE_SEVERAL_CANDIDATES_ON_SAME_POINT
        break;
#endif

        if ( o_step <= step + 3 )
        {
#ifdef DEBUG_PRINT_SUCCESS_PASS
            dlog.addText( Logger::PASS,
                          "---- o_step(=%d) <= step+3(=%d) break...",
                          o_step, step+3 );
#endif
            break;
        }

        if ( min_step + 3 <= step )
        {
#ifdef DEBUG_PRINT_SUCCESS_PASS
            dlog.addText( Logger::PASS,
                          "---- step=%d >= min_step+?(=%d) break...",
                          step, min_step + 3 );
#endif
            break;
        }

        if ( M_passer->unum() != wm.self().unum() )
        {
            break;
        }

        ++success_count;
    }


#ifdef DEBUG_PRINT_SUCCESS_PASS
    if ( ! success_counts.empty() )
    {
        std::ostringstream ostr;
        std::vector< int >::const_iterator it = success_counts.begin();

        ostr << *it; ++it;
        for ( ; it != success_counts.end(); ++it )
        {
            ostr << ',' << *it;
        }

        dlog.addRect( Logger::PASS,
                      receive_point.x - 0.1, receive_point.y - 0.1,
                      0.2, 0.2,
                      "#00ff00" );
        dlog.addMessage( Logger::PASS,
                         receive_point,
                         ostr.str().c_str() );
    }
#ifdef DEBUG_PRINT_FAILED_PASS
    else
    {
        debug_paint_failed_pass( M_total_count, receive_point );
    }
#endif
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
int
StrictCheckPassGenerator::getNearestReceiverUnum( const Vector2D & pos )
{
    int unum = Unum_Unknown;
    double min_dist2 = std::numeric_limits< double >::max();

    for ( ReceiverCont::iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        double d2 = p->pos_.dist2( pos );
        if ( d2 < min_dist2 )
        {
            min_dist2 = d2;
            unum = p->player_->unum();
        }
    }

    return unum;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
StrictCheckPassGenerator::predictReceiverReachStep( const Receiver & receiver,
                                                    const Vector2D & pos,
                                                    const bool use_penalty )
{
    const PlayerType * ptype = receiver.player_->playerTypePtr();
    double target_dist = receiver.inertia_pos_.dist( pos );
    int n_turn = ( receiver.player_->bodyCount() > 0
                   ? 1
                   : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                               receiver.player_->body(),
                                                               receiver.speed_,
                                                               target_dist,
                                                               ( pos - receiver.inertia_pos_ ).th(),
                                                               ptype->kickableArea(),
                                                               false ) );
    double dash_dist = target_dist;

    // if ( receiver.pos_.x < pos.x )
    // {
    //     dash_dist -= ptype->kickableArea() * 0.5;
    // }

    if ( use_penalty )
    {
        dash_dist += receiver.penalty_distance_;
    }

    // if ( M_pass_type == 'T' )
    // {
    //     dash_dist -= ptype->kickableArea() * 0.5;
    // }

    if ( M_pass_type == 'L' )
    {
        // if ( pos.x > -20.0
        //      && dash_dist < ptype->kickableArea() * 1.5 )
        // {
        //     dash_dist -= ptype->kickableArea() * 0.5;
        // }

        // if ( pos.x < 30.0 )
        // {
        //     dash_dist += 0.3;
        // }

        dash_dist *= 1.05;

        AngleDeg dash_angle = ( pos - receiver.pos_ ).th() ;

        if ( dash_angle.abs() > 90.0
             || receiver.player_->bodyCount() > 1
             || ( dash_angle - receiver.player_->body() ).abs() > 30.0 )
        {
            n_turn += 1;
        }
    }

    int n_dash = ptype->cyclesToReachDistance( dash_dist );

#ifdef DEBUG_PREDICT_RECEIVER
    dlog.addText( Logger::PASS,
                  "== receiver=%d receivePos=(%.1f %.1f) dist=%.2f dash=%.2f penalty=%.2f turn=%d dash=%d",
                  receiver.player_->unum(),
                  pos.x, pos.y,
                  target_dist, dash_dist, receiver.penalty_distance_,
                  n_turn, n_dash );
#endif
    return ( n_turn == 0
             ? n_turn + n_dash
             : n_turn + n_dash + 1 ); // 1 step penalty for observation delay.
    // if ( ! use_penalty )
    // {
    //     return n_turn + n_dash;
    // }
    // return n_turn + n_dash + 1; // 1 step penalty for observation delay.
}

/*-------------------------------------------------------------------*/
/*!

 */
int
StrictCheckPassGenerator::predictOpponentsReachStep( const WorldModel & wm,
                                                     const Vector2D & first_ball_pos,
                                                     const double & first_ball_speed,
                                                     const AngleDeg & ball_move_angle,
                                                     const Vector2D & receive_point,
                                                     const int max_cycle,
                                                     const AbstractPlayerObject ** opponent )
{
    const Vector2D first_ball_vel = Vector2D::polar2vector( first_ball_speed, ball_move_angle );

    double bonus_dist = -10000.0;
    int min_step = 1000;
    const AbstractPlayerObject * fastest_opponent = static_cast< AbstractPlayerObject * >( 0 );

    for ( OpponentCont::const_iterator o = M_opponents.begin();
          o != M_opponents.end();
          ++o )
    {
        int step = predictOpponentReachStep( wm,
                                             *o,
                                             first_ball_pos,
                                             first_ball_vel,
                                             ball_move_angle,
                                             receive_point,
                                             std::min( max_cycle, min_step ) );
        if ( step < min_step
             || ( step == min_step
                  && o->bonus_distance_ > bonus_dist ) )
        {
            bonus_dist = o->bonus_distance_;
            min_step = step;
            fastest_opponent = o->player_;
        }
    }

#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
    dlog.addText( Logger::PASS,
                  "______ opponent=%d(%.1f %.1f) step=%d",
                  fastest_opponent->unum(),
                  fastest_opponent->pos().x, fastest_opponent->pos().y,
                  min_step );
#endif

    if ( opponent )
    {
        *opponent = fastest_opponent;
    }
    return min_step;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
StrictCheckPassGenerator::predictOpponentReachStep( const WorldModel & wm,
                                                    const Opponent & opponent,
                                                    const Vector2D & first_ball_pos,
                                                    const Vector2D & first_ball_vel,
                                                    const AngleDeg & ball_move_angle,
                                                    const Vector2D & receive_point,
                                                    const int max_cycle )
{
    static const Rect2D penalty_area( Vector2D( ServerParam::i().theirPenaltyAreaLineX(),
                                                -ServerParam::i().penaltyAreaHalfWidth() ),
                                      Size2D( ServerParam::i().penaltyAreaLength(),
                                              ServerParam::i().penaltyAreaWidth() ) );
    static const double CONTROL_AREA_BUF = 0.15;

    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = opponent.player_->playerTypePtr();
    const int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( opponent.pos_,
                                                                   ptype->realSpeedMax(),
                                                                   first_ball_pos,
                                                                   ball_move_angle );
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
    dlog.addText( Logger::PASS,
                  "++ opponent=%d(%.1f %.1f)",
                  opponent.player_->unum(),
                  opponent.pos_.x, opponent.pos_.y );
#endif

    if ( min_cycle < 0 )
    {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
        dlog.addText( Logger::PASS,
                      "__ never reach(1)",
                      opponent.player_->unum(),
                      opponent.player_->pos().x,
                      opponent.player_->pos().y );
#endif
        return 1000;
    }

    for ( int cycle = std::max( 1, min_cycle ); cycle <= max_cycle; ++cycle )
    {
        const Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                                        first_ball_vel,
                                                        cycle,
                                                        SP.ballDecay() );
        const double control_area = ( opponent.player_->goalie()
                                      && penalty_area.contains( ball_pos )
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        const Vector2D inertia_pos = ptype->inertiaPoint( opponent.pos_, opponent.vel_, cycle );
        const double target_dist = inertia_pos.dist( ball_pos );

        double dash_dist = target_dist;

        if ( M_pass_type == 'T'
             && first_ball_vel.x > 2.0
             && ( receive_point.x > wm.offsideLineX()
                  || receive_point.x > 30.0 ) )
        {
#if 0
            dlog.addText( Logger::PASS,
                          "__ step=%d no bonus",
                          cycle );
#endif
        }
        else
        {
            dash_dist -= opponent.bonus_distance_;
        }

        if ( dash_dist - control_area - CONTROL_AREA_BUF < 0.001 )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::PASS,
                          "__ step=%d already there. dist=%.1f bonusDist=%.1f",
                          cycle,
                          target_dist, opponent.bonus_distance_ );
#endif
            return cycle;
        }

        //if ( cycle > 1 )
        {
            if ( M_pass_type == 'T'
                 && first_ball_vel.x > 2.0
                 && ( receive_point.x > wm.offsideLineX()
                      || receive_point.x > 30.0 ) )
            {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
                dlog.addText( Logger::PASS,
                              "__ step=%d through pass: dash_dist=%.2f bonus=%.2f",
                              cycle,
                              dash_dist, control_area * 0.8 );
#endif
                //dash_dist -= control_area * 0.5;
                //dash_dist -= control_area * 0.8;
                dash_dist -= control_area;
            }
            else
            {
                if ( receive_point.x < 25.0 )
                {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
                    dlog.addText( Logger::PASS,
                                  "__ step=%d normal(1) dash_dist=%.2f bonus=%.2f",
                                  cycle,
                                  dash_dist, control_area + 0.5 );
#endif
                    dash_dist -= control_area;
                    dash_dist -= 0.5;
                }
                else
                {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
                    dlog.addText( Logger::PASS,
                                  "__ step=%d normal(2) dash_dist=%.2f bonus=%.2f",
                                  cycle,
                                  dash_dist, control_area + 0.8 + 0.2 );
#endif
                    //dash_dist -= control_area * 0.8;
                    dash_dist -= control_area;
                    dash_dist -= 0.2;
                }
            }
        }

        if ( dash_dist > ptype->realSpeedMax()
             * ( cycle + std::min( opponent.player_->posCount(), 5 ) ) )
        {
#if 0
            dlog.addText( Logger::PASS,
                          "__ step=%d dash_dist=%.1f reachable=%.1f",
                          cycle,
                          dash_dist, ptype->realSpeedMax()*cycle );
#endif
            continue;
        }

        //
        // dash
        //

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > cycle + opponent.player_->posCount() )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::PASS,
                          "__ step=%d dash_dist=%.1f n_dash=%d",
                          cycle,
                          dash_dist, n_dash );
#endif
            continue;
        }

        //
        // turn
        //
        int n_turn = ( opponent.player_->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   opponent.player_->body(),
                                                                   opponent.speed_,
                                                                   target_dist,
                                                                   ( ball_pos - inertia_pos ).th(),
                                                                   control_area,
                                                                   true ));

        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 ); // 1 step penalty for observation delay

        int bonus_step = 0;
        if ( opponent.player_->isTackling() )
        {
            bonus_step = -5; // Magic Number
        }

        // if ( receive_point.x < 0.0 )
        // {
        //     bonus_step += 1;
        // }

#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
        dlog.addText( Logger::PASS,
                      "__ step=%d oppStep=%d(t:%d,d:%d)"
                      " ballPos=(%.2f %.2f)"
                      " dist=%.2f ctrl=%.2f dash=%.2f bonus=%.1f"
                      " bonusStep=%d",
                      cycle,
                      n_step, n_turn, n_dash,
                      ball_pos.x, ball_pos.y,
                      target_dist, control_area, dash_dist, opponent.bonus_distance_,
                      bonus_step );
#endif
        if ( n_step - bonus_step <= cycle )
        {
#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
            dlog.addText( Logger::PASS,
                          "__ can reach" );
#endif
            return cycle;
        }
    }

#ifdef DEBUG_PREDICT_OPPONENT_REACH_STEP
    dlog.addText( Logger::PASS,
                  "__ never reach(2)" );
#endif

    return 1000;
}
