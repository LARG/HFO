// -*-c++-*-

/*!
  \file cross_generator.cpp
  \brief cross pass generator Source File
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

#include "cross_generator.h"

#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/rect_2d.h>
#include <rcsc/soccer_math.h>
#include <rcsc/timer.h>

#define USE_ONLY_MAX_ANGLE_WIDTH

#define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE

using namespace rcsc;

namespace {

inline
void
debug_paint_failed( const int count,
                    const Vector2D & receive_point )
{
    dlog.addRect( Logger::CROSS,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
                  "#ff0000" );
    char num[8];
    snprintf( num, 8, "%d", count );
    dlog.addMessage( Logger::CROSS,
                     receive_point, num );
}

}

/*-------------------------------------------------------------------*/
/*!

 */
CrossGenerator::CrossGenerator()
{
    M_courses.reserve( 1024 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
CrossGenerator &
CrossGenerator::instance()
{
#ifdef __APPLE__
    static CrossGenerator s_instance;
#else
    static thread_local CrossGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::clear()
{
    M_total_count = 0;
    M_passer = static_cast< const AbstractPlayerObject * >( 0 );
    M_first_point.invalidate();
    M_receiver_candidates.clear();
    M_opponents.clear();
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::generate( const WorldModel & wm )
{
#ifdef __APPLE__
    static GameTime s_update_time( -1, 0 );
#else
    static thread_local GameTime s_update_time( -1, 0 );
#endif
    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

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
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) passer not found." );
        return;
    }

    if ( ServerParam::i().theirTeamGoalPos().dist( M_first_point ) > 35.0 )
    {
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) first point(%.1f %.1f) is too far from the goal.",
                      M_first_point.x, M_first_point.y );
        return;
    }

    updateReceivers( wm );

    if ( M_receiver_candidates.empty() )
    {
        dlog.addText( Logger::CROSS,
                      __FILE__" (generate) no receiver." );
        return;
    }

    updateOpponents( wm );

    createCourses( wm );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::CROSS,
                  __FILE__" (generate) PROFILE course_size=%d/%d elapsed %f [ms]",
                  (int)M_courses.size(),
                  M_total_count,
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::updatePasser( const WorldModel & wm )
{
    if ( wm.self().isKickable()
         && ! wm.self().isFrozen() )
    {
        M_passer = &wm.self();
        M_first_point = wm.ball().pos();
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
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
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
                      __FILE__" (updatePasser) opponent ball." );
#endif
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
#ifdef DEBUG_UPDATE_PASSER
        dlog.addText( Logger::CROSS,
                      __FILE__" (updatePasser) no passer." );
#endif
        return;
    }

    if ( M_passer->unum() != wm.self().unum() )
    {
        if ( M_first_point.dist2( wm.self().pos() ) > std::pow( 20.0, 2 ) )
        {
            M_passer = static_cast< const AbstractPlayerObject * >( 0 );
#ifdef DEBUG_UPDATE_PASSER
            dlog.addText( Logger::CROSS,
                          __FILE__" (updatePasser) passer is too far." );
#endif
            return;
        }
    }

#ifdef DEBUG_UPDATE_PASSER
    dlog.addText( Logger::CROSS,
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
void
CrossGenerator::updateReceivers( const WorldModel & wm )
{
    static const double shootable_dist2 = std::pow( 16.0, 2 ); // Magic Number
    static const double min_cross_dist2
        = std::pow( ServerParam::i().defaultKickableArea() * 2.2, 2 );
    static const double max_cross_dist2
        = std::pow( inertia_n_step_distance( ServerParam::i().ballSpeedMax(),
                                             9,
                                             ServerParam::i().ballDecay() ),
                    2 );

    const Vector2D goal = ServerParam::i().theirTeamGoalPos();

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
            if ( (*p)->isGhost() ) continue;
            if ( (*p)->posCount() >= 4 ) continue;
            if ( (*p)->pos().x > wm.offsideLineX() ) continue;
        }
        else
        {
            // ignore other players
            if ( (*p)->unum() != wm.self().unum() )
            {
                continue;
            }
        }

        if ( (*p)->pos().dist2( goal ) > shootable_dist2 ) continue;

        double d2 = (*p)->pos().dist2( M_first_point );
        if ( d2 < min_cross_dist2 ) continue;
        if ( max_cross_dist2 < d2 ) continue;

        M_receiver_candidates.push_back( *p );

#ifdef DEBUG_UPDATE_OPPONENT
        dlog.addText( Logger::CROSS,
                      "Cross receiver %d pos(%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::updateOpponents( const WorldModel & wm )
{
    const double opponent_dist_thr2 = std::pow( 20.0, 2 );

    const Vector2D goal = ServerParam::i().theirTeamGoalPos();
    const AngleDeg goal_angle_from_ball = ( goal - M_first_point ).th();

    for ( AbstractPlayerCont::const_iterator
              p = wm.theirPlayers().begin(),
              end = wm.theirPlayers().end();
          p != end;
          ++p )
    {
        AngleDeg opponent_angle_from_ball = ( (*p)->pos() - M_first_point ).th();
        if ( ( opponent_angle_from_ball - goal_angle_from_ball ).abs() > 90.0 )
        {
            continue;
        }

        if ( (*p)->pos().dist2( M_first_point ) > opponent_dist_thr2 )
        {
            continue;
        }

        M_opponents.push_back( *p );

#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "Cross opponent %d pos(%.1f %.1f)",
                      (*p)->unum(),
                      (*p)->pos().x, (*p)->pos().y );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::createCourses( const WorldModel & wm )
{
    for ( AbstractPlayerCont::const_iterator p = M_receiver_candidates.begin();
          p != M_receiver_candidates.end();
          ++p )
    {
        createCross( wm, *p );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
CrossGenerator::createCross( const WorldModel & wm,
                             const AbstractPlayerObject * receiver )
{
    static const int MIN_RECEIVE_STEP = 2;
    static const int MAX_RECEIVE_STEP = 12; // Magic Number

    static const double MIN_RECEIVE_BALL_SPEED
        = ServerParam::i().defaultPlayerSpeedMax();
    // = std::max( ServerParam::i().defaultPlayerSpeedMax(),
    //             ServerParam::i().ballSpeedMax()
    //             * std::pow( ServerParam::i().ballDecay(), MAX_RECEIVE_STEP ) );
    // static const MAX_RECEIVE_BALL_SPEED
    //     = ServerParam::i().ballSpeedMax()
    //     * std::pow( ServerParam::i().ballDecay(), MIN_RECEIVE_STEP );

    static const double ANGLE_STEP = 3.0;
    static const double DIST_STEP = 0.9;

    const ServerParam & SP = ServerParam::i();

    const double min_first_ball_speed = SP.ballSpeedMax() * 0.67; // Magic Number
    const double max_first_ball_speed = ( wm.gameMode().type() == GameMode::PlayOn
                                          ? SP.ballSpeedMax()
                                          : wm.self().isKickable()
                                          ? wm.self().kickRate() * SP.maxPower()
                                          : SP.kickPowerRate() * SP.maxPower() );

    const PlayerType * ptype = receiver->playerTypePtr();
    const Vector2D receiver_pos = receiver->inertiaFinalPoint();
    const double receiver_dist = M_first_point.dist( receiver_pos );
    const AngleDeg receiver_angle_from_ball = ( receiver_pos - M_first_point ).th();

#ifdef USE_ONLY_MAX_ANGLE_WIDTH
    double max_angle_diff = -1.0;
    CooperativeAction::Ptr best_action;
#endif

    //
    // angle loop
    //
    for ( int a = -2; a < 3; ++a )
    {
        const AngleDeg cross_angle = receiver_angle_from_ball + ( ANGLE_STEP * a );

        //
        // distance loop
        //
        for ( int d = 0; d < 5; ++d )
        {
            const double sub_dist = DIST_STEP * d;
            const double ball_move_dist = receiver_dist - sub_dist;
            const Vector2D receive_point
                = M_first_point
                + Vector2D::from_polar( ball_move_dist, cross_angle );

#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "==== receiver=%d receivePos=(%.2f %.2f) loop=%d angle=%.1f",
                          receiver->unum(),
                          receive_point.x, receive_point.y,
                          a, cross_angle.degree() );
#endif

            if ( receive_point.x > SP.pitchHalfLength() - 0.5
                 || receive_point.absY() > SP.pitchHalfWidth() - 3.0 )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::CROSS,
                              "%d: xxx unum=%d (%.2f %.2f) outOfBounds",
                              M_total_count, receiver->unum(),
                              receive_point.x, receive_point.y );
                debug_paint_failed( M_total_count, receive_point );
#endif
                continue;
            }

            const int receiver_step = ptype->cyclesToReachDistance( sub_dist ) + 1;

            //
            // step loop
            //

            for ( int step = std::max( MIN_RECEIVE_STEP, receiver_step );
                  step <= MAX_RECEIVE_STEP;
                  ++step )
            {
                ++M_total_count;

                double first_ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                                       SP.ballDecay(),
                                                                       step );
                if ( first_ball_speed < min_first_ball_speed )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f < min=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  first_ball_speed, min_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point );
#endif
                    break;
                }

                if ( max_first_ball_speed < first_ball_speed )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d firstSpeed=%.3f > max=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  first_ball_speed, max_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point );
#endif
                    continue;
                }

                double receive_ball_speed = first_ball_speed * std::pow( SP.ballDecay(), step );
                if ( receive_ball_speed < MIN_RECEIVE_BALL_SPEED )
                {
#ifdef DEBUG_PRINT_FAILED_COURSE
                    dlog.addText( Logger::CROSS,
                                  "%d: xxx unum=%d (%.1f %.1f) step=%d recvSpeed=%.3f < min=%.3f",
                                  M_total_count,
                                  receiver->unum(),
                                  receive_point.x, receive_point.y,
                                  step,
                                  receive_ball_speed, min_first_ball_speed );
                    //debug_paint_failed( M_total_count, receive_point );
#endif
                    break;
                }


                int kick_count = FieldAnalyzer::predict_kick_count( wm,
                                                                    M_passer,
                                                                    first_ball_speed,
                                                                    cross_angle );

                if ( ! checkOpponent( M_first_point,
                                      receiver,
                                      receive_point,
                                      first_ball_speed,
                                      cross_angle,
                                      step + kick_count - 1 ) ) // 1 step penalty for observation delay
                {

                    break;
                }

#ifdef USE_ONLY_MAX_ANGLE_WIDTH
                double min_angle_diff = getMinimumAngleWidth( ball_move_dist, cross_angle );
                if ( min_angle_diff > max_angle_diff )
                {
                    CooperativeAction::Ptr ptr( new Pass( M_passer->unum(),
                                                          receiver->unum(),
                                                          receive_point,
                                                          first_ball_speed,
                                                          step + kick_count,
                                                          kick_count,
                                                          false,
                                                          "cross" ) );
                    ptr->setIndex( M_total_count );
                    max_angle_diff = min_angle_diff;
                    best_action = ptr;

                }
#else
                CooperativeAction::Ptr ptr( new Pass( M_passer->unum(),
                                                      receiver->unum(),
                                                      receive_point,
                                                      first_ball_speed,
                                                      step + kick_count,
                                                      kick_count,
                                                      false,
                                                      "cross" ) );
                ptr->setIndex( M_total_count );
                M_courses.push_back( ptr );
#endif
                // M_courses.push_back( ptr );
#ifdef DEBUG_PRINT_SUCCESS_COURSE
                dlog.addText( Logger::CROSS,
                              "%d: ok Cross step=%d pos=(%.1f %.1f) speed=%.3f->%.3f nKick=%d",
                              M_total_count,
                              step, kick_count,
                              receive_point.x, receive_point.y,
                              first_ball_speed, receive_ball_speed,
                              kick_count );
                char num[8];
                snprintf( num, 8, "%d", M_total_count );
                dlog.addMessage( Logger::CROSS,
                                 receive_point, num );
                dlog.addRect( Logger::CROSS,
                              receive_point.x - 0.1, receive_point.y - 0.1,
                              0.2, 0.2,
                              "#00ff00" );
#endif
                break;
            }
        }
    }

#ifdef USE_ONLY_MAX_ANGLE_WIDTH
    if ( best_action )
    {
        M_courses.push_back( best_action );
    }
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
CrossGenerator::checkOpponent( const Vector2D & first_ball_pos,
                               const rcsc::AbstractPlayerObject * receiver,
                               const Vector2D & receive_pos,
                               const double & first_ball_speed,
                               const AngleDeg & ball_move_angle,
                               const int max_cycle )
{
    static const double CONTROL_AREA_BUF = 0.15;  // buffer for kick table

    const ServerParam & SP = ServerParam::i();

    const double receiver_dist = receiver->pos().dist( first_ball_pos );
    const Vector2D first_ball_vel
        = Vector2D( receive_pos - first_ball_pos ).setLength( first_ball_speed );

    const AbstractPlayerCont::const_iterator end = M_opponents.end();
    for ( AbstractPlayerCont::const_iterator o = M_opponents.begin();
          o != end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();
        const double control_area = ( (*o)->goalie()
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        const Vector2D opponent_pos = (*o)->inertiaFinalPoint();
        const int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( opponent_pos,
                                                                       ptype->realSpeedMax(),
                                                                       first_ball_pos,
                                                                       ball_move_angle );


        if ( opponent_pos.dist( first_ball_pos ) > receiver_dist + 1.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::CROSS,
                          "__ opponent[%d](%.2f %.2f) skip. distance over",
                          (*o)->unum(),
                          opponent_pos.x, opponent_pos.y );
#endif
            continue;
        }

        for ( int cycle = std::max( 1, min_cycle );
              cycle <= max_cycle;
              ++cycle )
        {
            Vector2D ball_pos = inertia_n_step_point( first_ball_pos,
                                                      first_ball_vel,
                                                      cycle,
                                                      SP.ballDecay() );
            double target_dist = opponent_pos.dist( ball_pos );

            if ( target_dist - control_area - CONTROL_AREA_BUF < 0.001 )
            {
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::CROSS,
                              "%d: xxx recvPos(%.2f %.2f) step=%d/%d"
                              " opponent(%d)(%.2f %.2f) kickable"
                              " ballPos(%.2f %.2f)",
                              M_total_count,
                              receive_pos.x, receive_pos.y,
                              cycle, max_cycle,
                              (*o)->unum(), opponent_pos.x, opponent_pos.y ,
                              ball_pos.x, ball_pos.y );
                debug_paint_failed( M_total_count, receive_pos );
#endif
                return false;
            }

            double dash_dist = target_dist;

            if ( cycle > 1 )
            {
                //dash_dist -= control_area*0.8;
                dash_dist -= control_area*0.6;
                //dash_dist -= control_area*0.5;
            }

            if ( dash_dist > ptype->realSpeedMax() * cycle )
            {
                continue;
            }

            //
            // dash
            //

            int n_dash = ptype->cyclesToReachDistance( dash_dist * 1.05 ); // add penalty

            if ( n_dash > cycle )
            {
                continue;
            }

            //
            // turn
            //
            int n_turn = ( (*o)->bodyCount() >= 3
                           ? 2
                           : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                       (*o)->body(),
                                                                       (*o)->vel().r(),
                                                                       target_dist,
                                                                       ( ball_pos - opponent_pos ).th(),
                                                                       control_area,
                                                                       true ) );

            int n_step = n_turn + n_dash + 1; // 1 step penalty for observation delay
            if ( (*o)->isTackling() )
            {
                n_step += 5; // Magic Number
            }

            if ( n_step <= cycle )
            {
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::CROSS,
                              "%d: xxx recvPos(%.1f %.1f) step=%d/%d"
                              " opponent(%d)(%.1f %.1f) can reach"
                              " ballPos(%.2f %.2f)",
                              M_total_count,
                              receive_pos.x, receive_pos.y,
                              cycle, max_cycle,
                              (*o)->unum(), opponent_pos.x, opponent_pos.y,
                              ball_pos.x, ball_pos.y );
                debug_paint_failed( M_total_count, receive_pos );
#endif
                return false;
            }
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
CrossGenerator::getMinimumAngleWidth( const double & target_dist,
                                      const AngleDeg & target_angle )
{
    double min_angle_diff = 180.0;

    const AbstractPlayerCont::const_iterator end = M_opponents.end();
    for ( AbstractPlayerCont::const_iterator o = M_opponents.begin();
          o != end;
          ++ o )
    {
        if ( (*o)->isGhost() ) continue;

        double opponent_dist = M_first_point.dist( (*o)->pos() );
        if ( opponent_dist > target_dist + 1.0 )
        {
            continue;
        }

        AngleDeg opponent_angle = ( (*o)->pos() - M_first_point ).th();
        double angle_diff = ( opponent_angle - target_angle ).abs();

        if ( angle_diff < min_angle_diff )
        {
            min_angle_diff = angle_diff;
        }
    }

    return min_angle_diff;
}
