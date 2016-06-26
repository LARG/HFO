// -*-c++-*-

/*!
  \file shoot_generator.cpp
  \brief shoot course generator class Source File
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "shoot_generator.h"

#include "field_analyzer.h"

#include <rcsc/action/kick_table.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/math_util.h>
#include <rcsc/timer.h>

#define SEARCH_UNTIL_MAX_SPEED_AT_SAME_POINT

#define DEBUG_PROFILE
// #define DEBUG_PRINT

// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE

// #define DEBUG_PRINT_EVALUATE

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
ShootGenerator::ShootGenerator()
{
    M_courses.reserve( 32 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
ShootGenerator &
ShootGenerator::instance()
{
#ifdef __APPLE__
    static ShootGenerator s_instance;
#else
    static thread_local ShootGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootGenerator::clear()
{
    M_total_count = 0;
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootGenerator::generate( const WorldModel & wm, bool consider_shot_distance )
{
#ifdef __APPLE__
    static GameTime s_update_time( 0, 0 );
#else
    static thread_local GameTime s_update_time( 0, 0 );
#endif

    if ( s_update_time == wm.time() )
    {
        return;
    }
    s_update_time = wm.time();

    clear();

    if ( ! wm.self().isKickable()
         && wm.interceptTable()->selfReachCycle() > 1 )
    {
        return;
    }

    if ( wm.time().stopped() > 0
         || wm.gameMode().type() == GameMode::KickOff_
         // || wm.gameMode().type() == GameMode::KickIn_
         || wm.gameMode().type() == GameMode::IndFreeKick_ )
    {
        return;
    }

    const ServerParam & SP = ServerParam::i();

    if ( consider_shot_distance &&
         wm.self().pos().dist2( SP.theirTeamGoalPos() ) > std::pow( 30.0, 2 ) )
    {
#ifdef DEBUG_PRINT
      dlog.addText( Logger::SHOOT,
                    __FILE__": over shootable distance" );
#endif
      return;
    }

    M_first_ball_pos = ( wm.self().isKickable()
                         ? wm.ball().pos()
                         : wm.ball().pos() + wm.ball().vel() );

#ifdef DEBUG_PROFILE
    MSecTimer timer;
#endif

    Vector2D goal_l( SP.pitchHalfLength(), -SP.goalHalfWidth() );
    Vector2D goal_r( SP.pitchHalfLength(), +SP.goalHalfWidth() );

    goal_l.y += std::min( 1.5,
                          0.6 + goal_l.dist( M_first_ball_pos ) * 0.042 );
    goal_r.y -= std::min( 1.5,
                          0.6 + goal_r.dist( M_first_ball_pos ) * 0.042 );

    if ( wm.self().pos().x > SP.pitchHalfLength() - 1.0
         && wm.self().pos().absY() < SP.goalHalfWidth() )
    {
        goal_l.x = wm.self().pos().x + 1.5;
        goal_r.x = wm.self().pos().x + 1.5;
    }

    const int DIST_DIVS = 25;
    const double dist_step = std::fabs( goal_l.y - goal_r.y ) / ( DIST_DIVS - 1 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::SHOOT,
                  __FILE__": ===== Shoot search range=(%.1f %.1f)-(%.1f %.1f) dist_step=%.1f =====",
                  goal_l.x, goal_l.y, goal_r.x, goal_r.y, dist_step );
#endif

    for ( int i = 0; i < DIST_DIVS; ++i )
    {
        ++M_total_count;

        Vector2D target_point = goal_l;
        target_point.y += dist_step * i;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: ===== shoot target(%.2f %.2f) ===== ",
                      M_total_count,
                      target_point.x, target_point.y );
#endif
        createShoot( wm, target_point );
    }


    evaluateCourses( wm );


#ifdef DEBUG_PROFILE
    dlog.addText( Logger::SHOOT,
                  __FILE__": PROFILE %d/%d. elapsed=%.3f [ms]",
                  (int)M_courses.size(),
                  DIST_DIVS,
                  timer.elapsedReal() );
#endif

}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootGenerator::createShoot( const WorldModel & wm,
                             const Vector2D & target_point )
{
    const AngleDeg ball_move_angle = ( target_point - M_first_ball_pos ).th();

    const PlayerObject * goalie = wm.getOpponentGoalie();
    if ( goalie
         && 5 < goalie->posCount()
         && goalie->posCount() < 30
         && wm.dirCount( ball_move_angle ) > 3 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: __ xxx goalie_count=%d, low dir accuracy",
                      M_total_count,
                      goalie->posCount() );
#endif
        return;
    }

    const ServerParam & SP = ServerParam::i();

    const double ball_speed_max = ( wm.gameMode().type() == GameMode::PlayOn
                                    || wm.gameMode().isPenaltyKickMode()
                                    ? SP.ballSpeedMax()
                                    : wm.self().kickRate() * SP.maxPower() );

    const double ball_move_dist = M_first_ball_pos.dist( target_point );

    const Vector2D max_one_step_vel
        = ( wm.self().isKickable()
            ? KickTable::calc_max_velocity( ball_move_angle,
                                            wm.self().kickRate(),
                                            wm.ball().vel() )
            : ( target_point - M_first_ball_pos ).setLengthVector( 0.1 ) );
    const double max_one_step_speed = max_one_step_vel.r();

    double first_ball_speed
        = std::max( ( ball_move_dist + 5.0 ) * ( 1.0 - SP.ballDecay() ),
                    std::max( max_one_step_speed,
                              1.5 ) );

    bool over_max = false;
#ifdef DEBUG_PRINT_FAILED_COURSE
    bool success = false;
#endif
    while ( ! over_max )
    {
        if ( first_ball_speed > ball_speed_max - 0.001 )
        {
            over_max = true;
            first_ball_speed = ball_speed_max;
        }

        if ( createShoot( wm,
                          target_point,
                          first_ball_speed,
                          ball_move_angle,
                          ball_move_dist ) )
        {
            Course & course = M_courses.back();

            if ( first_ball_speed <= max_one_step_speed + 0.001 )
            {
                course.kick_step_ = 1;
            }

#ifdef DEBUG_PRINT_SUCCESS_COURSE
            dlog.addText( Logger::SHOOT,
                          "%d: ok shoot target=(%.2f %.2f)"
                          " speed=%.3f angle=%.1f",
                          M_total_count,
                          target_point.x, target_point.y,
                          first_ball_speed,
                          ball_move_angle.degree() );
            dlog.addRect( Logger::SHOOT,
                          target_point.x - 0.1, target_point.y - 0.1,
                          0.2, 0.2,
                          "#00ff00" );
            char num[8];
            snprintf( num, 8, "%d", M_total_count );
            dlog.addMessage( Logger::SHOOT,
                             target_point, num, "#ffffff" );
#endif
#ifdef DEBUG_PRINT_FAILED_COURSE
            success = true;
#endif
#ifdef SEARCH_UNTIL_MAX_SPEED_AT_SAME_POINT
            if ( course.goalie_never_reach_
                 && course.opponent_never_reach_ )
            {
                return;
            }
            ++M_total_count;
#else
            return;
#endif
        }

        first_ball_speed += 0.3;
    }

#ifdef DEBUG_PRINT_FAILED_COURSE
    if ( success )
    {
        return;
    }

    dlog.addText( Logger::SHOOT,
                  "%d: xxx shoot target=(%.2f %.2f)"
                  " speed=%.3f angle=%.1f",
                  M_total_count,
                  target_point.x, target_point.y,
                  first_ball_speed,
                  ball_move_angle.degree() );
    dlog.addRect( Logger::SHOOT,
                  target_point.x - 0.1, target_point.y - 0.1,
                  0.2, 0.2,
                  "#ff0000" );
    char num[8];
    snprintf( num, 8, "%d", M_total_count );
    dlog.addMessage( Logger::SHOOT,
                     target_point, num, "#ffffff" );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootGenerator::createShoot( const WorldModel & wm,
                             const Vector2D & target_point,
                             const double & first_ball_speed,
                             const rcsc::AngleDeg & ball_move_angle,
                             const double & ball_move_dist )
{
    const ServerParam & SP = ServerParam::i();

    const int ball_reach_step
        = static_cast< int >( std::ceil( calc_length_geom_series( first_ball_speed,
                                                                   ball_move_dist,
                                                                   SP.ballDecay() ) ) );
#ifdef DEBUG_PRINT
    dlog.addText( Logger::SHOOT,
                  "%d: target=(%.2f %.2f) speed=%.3f angle=%.1f"
                  " ball_reach_step=%d",
                  M_total_count,
                  target_point.x, target_point.y,
                  first_ball_speed,
                  ball_move_angle.degree(),
                  ball_reach_step );
#endif

    Course course( M_total_count,
                   target_point,
                   first_ball_speed,
                   ball_move_angle,
                   ball_move_dist,
                   ball_reach_step );

    if ( ball_reach_step <= 1 )
    {
        course.ball_reach_step_ = 1;
        M_courses.push_back( course );
#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: one step to the goal" );
#endif
        return true;
    }

    // estimate opponent interception

    const double opponent_x_thr = SP.theirPenaltyAreaLineX() - 30.0;
    const double opponent_y_thr = SP.penaltyAreaHalfWidth();

    const PlayerPtrCont::const_iterator end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != end;
          ++o )
    {
        if ( (*o)->isTackling() ) continue;
        if ( (*o)->pos().x < opponent_x_thr ) continue;
        if ( (*o)->pos().absY() > opponent_y_thr ) continue;

        // behind of shoot course
        if ( ( ball_move_angle - (*o)->angleFromSelf() ).abs() > 90.0 )
        {
            continue;
        }

        if ( (*o)->goalie() )
        {
            if ( maybeGoalieCatch( *o, course ) )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::SHOOT,
                              "%d: maybe goalie", M_total_count );
#endif
                return false;
            }

            continue;
        }

        //
        // check field player
        //

        if ( (*o)->posCount() > 10 ) continue;
        if ( (*o)->isGhost() && (*o)->posCount() > 5 ) continue;

        if ( opponentCanReach( *o, course ) )
        {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::SHOOT,
                              "%d: maybe opponent", M_total_count );
#endif
            return false;
        }
    }

    M_courses.push_back( course );
    return true;

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootGenerator::maybeGoalieCatch( const PlayerObject * goalie,
                                  Course & course )
{
    static const Rect2D penalty_area( Vector2D( ServerParam::i().theirPenaltyAreaLineX(),
                                                -ServerParam::i().penaltyAreaHalfWidth() ),
                                      Size2D( ServerParam::i().penaltyAreaLength(),
                                              ServerParam::i().penaltyAreaWidth() ) );
    static const double CONTROL_AREA_BUF = 0.15;  // buffer for kick table

    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = goalie->playerTypePtr();

    const int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( goalie->pos(),
                                                                   ptype->realSpeedMax(),
                                                                   M_first_ball_pos,
                                                                   course.ball_move_angle_ );
    if ( min_cycle < 0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: (goalie) never reach" );
#endif
        return false;
    }

    const double goalie_speed = goalie->vel().r();
    const double seen_dist_noise = goalie->distFromSelf() * 0.02;

    const int max_cycle = course.ball_reach_step_;

#ifdef DEBUG_PRINT
    dlog.addText( Logger::SHOOT,
                  "%d: (goalie) minCycle=%d maxCycle=%d",
                  M_total_count,
                  min_cycle, max_cycle );
#endif

    for ( int cycle = min_cycle; cycle < max_cycle; ++cycle )
    {
        const Vector2D ball_pos = inertia_n_step_point( M_first_ball_pos,
                                                        course.first_ball_vel_,
                                                        cycle,
                                                        SP.ballDecay() );
        if ( ball_pos.x > SP.pitchHalfLength() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: (goalie) cycle=%d in the goal",
                          M_total_count, cycle );
#endif
            break;
        }

        const bool in_penalty_area = penalty_area.contains( ball_pos );

        const double control_area = ( in_penalty_area
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        Vector2D inertia_pos = goalie->inertiaPoint( cycle );
        double target_dist = inertia_pos.dist( ball_pos );

        if ( in_penalty_area )
        {
            target_dist -= seen_dist_noise;
        }

        if ( target_dist - control_area - CONTROL_AREA_BUF < 0.001 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: xxx (goalie) can catch. cycle=%d ball_pos(%.2f %.2f)"
                          " dist_from_goalie=%.3f",
                          M_total_count,
                          cycle,
                          ball_pos.x, ball_pos.y,
                          target_dist );
#endif
            return true;
        }

        double dash_dist = target_dist;
        if ( cycle > 1 )
        {
            //dash_dist -= control_area * 0.6;
            //dash_dist *= 0.95;
            dash_dist -= control_area * 0.9;
            dash_dist *= 0.999;
        }

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > cycle + goalie->posCount() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: (goalie) cycle=%d dash_dist=%.3f n_dash=%d posCount=%d",
                          M_total_count,
                          cycle,
                          dash_dist,
                          n_dash, goalie->posCount() );
#endif
            continue;
        }

        int n_turn = ( goalie->bodyCount() > 1
                       ? 0
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   goalie->body(),
                                                                   goalie_speed,
                                                                   target_dist,
                                                                   ( ball_pos - inertia_pos ).th(),
                                                                   control_area + 0.1,
                                                                   true ) );
        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 );

        int bonus_step = ( in_penalty_area
                           ? bound( 0, goalie->posCount(), 5 )
                           : bound( 0, goalie->posCount() - 1, 1 ) );
        if ( ! in_penalty_area )
        {
            bonus_step -= 1;
        }

        if ( n_step <= cycle + bonus_step )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: xxx (goalie) can catch. cycle=%d ball_pos(%.1f %.1f)"
                          " goalie target_dist=%.3f(noise=%.3f dash=%.3f ctrl=%.3f) step=%d(t:%d,d%d) bonus=%d",
                          M_total_count,
                          cycle,
                          ball_pos.x, ball_pos.y,
                          target_dist, seen_dist_noise, dash_dist, control_area,
                          n_step, n_turn, n_dash, bonus_step );
#endif
            return true;
        }

#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: (goalie) cycle=%d ball_pos(%.1f %.1f)"
                      " goalieStep=%d(t:%d,d%d) bonus=%d",
                      M_total_count,
                      cycle,
                      ball_pos.x, ball_pos.y,
                      n_step, n_turn, n_dash, bonus_step );
#endif

        if ( in_penalty_area
             && n_step <= cycle + goalie->posCount() + 1 )
        {
            course.goalie_never_reach_ = false;

#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: (goalie) may be reach",
                          M_total_count );
#endif
        }
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
ShootGenerator::opponentCanReach( const PlayerObject * opponent,
                                  Course & course )
{
    const ServerParam & SP = ServerParam::i();

    const PlayerType * ptype = opponent->playerTypePtr();
    const double control_area = ptype->kickableArea();

    const int min_cycle = FieldAnalyzer::estimate_min_reach_cycle( opponent->pos(),
                                                                   ptype->realSpeedMax(),
                                                                   M_first_ball_pos,
                                                                   course.ball_move_angle_ );
    if ( min_cycle < 0 )
    {
// #ifdef DEBUG_PRINT
//         dlog.addText( Logger::SHOOT,
//                       "%d: (opponent) [%d](%.2f %.2f) never reach",
//                       M_total_count,
//                       opponent->unum(),
//                       opponent->pos().x, opponent->pos().y );
// #endif
        return false;
    }

    const double opponent_speed = opponent->vel().r();
    const int max_cycle = course.ball_reach_step_;

    bool maybe_reach = false;
#ifdef DEBUG_PRINT
    int nearest_cycle = 1000;
#endif
    int nearest_step_diff = 1000;

    for ( int cycle = min_cycle; cycle < max_cycle; ++cycle )
    {
        Vector2D ball_pos = inertia_n_step_point( M_first_ball_pos,
                                                  course.first_ball_vel_,
                                                  cycle,
                                                  SP.ballDecay() );

        Vector2D inertia_pos = opponent->inertiaPoint( cycle );
        double target_dist = inertia_pos.dist( ball_pos );

        if ( target_dist - control_area < 0.001 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: (opponent) [%d] inertiaPos=(%.2f %.2f) can kick without dash",
                          M_total_count,
                          opponent->unum(),
                          inertia_pos.x, inertia_pos.y );
#endif
            return true;
        }

        double dash_dist = target_dist;
        if ( cycle > 1 )
        {
            dash_dist -= control_area*0.8;
        }

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        if ( n_dash > cycle + opponent->posCount() )
        {
            continue;
        }

        int n_turn = ( opponent->bodyCount() > 0
                       ? 1
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   opponent->body(),
                                                                   opponent_speed,
                                                                   target_dist,
                                                                   ( ball_pos - inertia_pos ).th(),
                                                                   control_area,
                                                                   true ) );
        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 );

        //int bonus_step = bound( 0, opponent->posCount() - 1, 1 );
        int bonus_step = bound( 0, opponent->posCount(), 1 );
        int penalty_step = -1; //-3;

        if ( opponent->isTackling() )
        {
            penalty_step -= 5;
        }

        if ( n_step <= cycle + bonus_step + penalty_step )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::SHOOT,
                          "%d: xxx (opponent) can reach. cycle=%d ball_pos(%.1f %.1f)"
                          " oppStep=%d(t:%d,d%d) bonus=%d",
                          M_total_count,
                          cycle,
                          ball_pos.x, ball_pos.y,
                          n_step, n_turn, n_dash, bonus_step );
#endif
            return true;
        }

        if ( n_step <= cycle + opponent->posCount() + 1 )
        {
            maybe_reach = true;
            int diff = cycle + opponent->posCount() - n_step;
            if ( diff < nearest_step_diff )
            {
#ifdef DEBUG_PRINT
                nearest_cycle = cycle;
#endif
                nearest_step_diff = diff;
            }
        }
    }

    if ( maybe_reach )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::SHOOT,
                      "%d: (opponent) maybe reach. nearest_step=%d diff=%d",
                      M_total_count,
                      nearest_cycle, nearest_step_diff );
#endif
        course.opponent_never_reach_ = false;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShootGenerator::evaluateCourses( const WorldModel & wm )
{
    const double y_dist_thr2 = std::pow( 8.0, 2 );

    const ServerParam & SP = ServerParam::i();
    const PlayerObject * goalie = wm.getOpponentGoalie();
    const AngleDeg goalie_angle = ( goalie
                                    ? ( goalie->pos() - M_first_ball_pos ).th()
                                    : 180.0 );

    const Container::iterator end = M_courses.end();
    for ( Container::iterator it = M_courses.begin();
          it != end;
          ++it )
    {
        double score = 1.0;

        if ( it->kick_step_ == 1 )
        {
            score += 50.0;
        }

        if ( it->goalie_never_reach_ )
        {
            score += 100.0;
        }

        if ( it->opponent_never_reach_ )
        {
            score += 100.0;
        }

        double goalie_rate = 1.0;
        if ( goalie )
        {
#if 1
            double variance2 = ( it->goalie_never_reach_
                                 ? 1.0 // 1.0*1.0
                                 : std::pow( 10.0, 2 ) );
            double angle_diff = ( it->ball_move_angle_ - goalie_angle ).abs();
            goalie_rate = 1.0 - std::exp( - std::pow( angle_diff, 2 )
                                          / ( 2.0 * variance2 ) );
#else
            double angle_diff = ( it->ball_move_angle_ - goalie_angle ).abs();
            goalie_rate = 1.0 - std::exp( - std::pow( angle_diff * 0.1, 2 )
                                          // / ( 2.0 * 90.0 * 0.1 ) );
                                          // / ( 2.0 * 40.0 * 0.1 ) ); // 2009-07
                                              // / ( 2.0 * 90.0 * 0.1 ) ); // 2009-12-13
                                          / ( 2.0 * 20.0 * 0.1 ) ); // 2010-06-09
#endif
        }

        double y_rate = 1.0;
        if ( it->target_point_.dist2( M_first_ball_pos ) > y_dist_thr2 )
        {
            double y_dist = std::max( 0.0, it->target_point_.absY() - 4.0 );
            y_rate = std::exp( - std::pow( y_dist, 2.0 )
                               / ( 2.0 * std::pow( SP.goalHalfWidth() - 1.5, 2 ) ) );
        }

#ifdef DEBUG_PRINT_EVALUATE
        dlog.addText( Logger::SHOOT,
                      "(shoot eval) %d: score=%f(%f) pos(%.2f %.2f) speed=%.3f goalie_rate=%f y_rate=%f",
                      it->index_,
                      score * goalie_rate * y_rate, score,
                      it->target_point_.x, it->target_point_.y,
                      it->first_ball_speed_,
                      goalie_rate,
                      y_rate );
#endif
        score *= goalie_rate;
        score *= y_rate;
        it->score_ = score;
    }
}
