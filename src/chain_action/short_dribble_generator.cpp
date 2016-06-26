// -*-c++-*-

/*!
  \file short_dribble_generator.cpp
  \brief short step dribble course generator Source File
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

#include "short_dribble_generator.h"

#include "dribble.h"
#include "field_analyzer.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/geom/segment_2d.h>
#include <rcsc/timer.h>

#include <algorithm>
#include <limits>

#include <cmath>

#define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SIMULATE_DASHES
// #define DEBUG_PRINT_OPPONENT

// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE

using namespace rcsc;

namespace {

inline
void
debug_paint_failed( const int count,
                    const Vector2D & receive_point )
{
    dlog.addCircle( Logger::DRIBBLE,
                    receive_point.x, receive_point.y, 0.1,
                    "#ff0000" );
    char num[8];
    snprintf( num, 8, "%d", count );
    dlog.addMessage( Logger::DRIBBLE,
                     receive_point, num );
}

}

/*-------------------------------------------------------------------*/
/*!

 */
ShortDribbleGenerator::ShortDribbleGenerator()
    : M_queued_action_time( 0, 0 )
{
    M_courses.reserve( 128 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
ShortDribbleGenerator &
ShortDribbleGenerator::instance()
{
#ifdef __APPLE__
    static ShortDribbleGenerator s_instance;
#else
    static thread_local ShortDribbleGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::clear()
{
    M_total_count = 0;
    M_first_ball_pos = Vector2D::INVALIDATED;
    M_first_ball_vel.assign( 0.0, 0.0 );
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::generate( const WorldModel & wm )
{
    if ( M_update_time == wm.time() )
    {
        return;
    }
    M_update_time = wm.time();

    clear();

    if ( wm.gameMode().type() != GameMode::PlayOn
         && ! wm.gameMode().isPenaltyKickMode() )
    {
        return;
    }

    //
    // check queued action
    //

    if ( M_queued_action_time != wm.time() )
    {
        M_queued_action.reset();
    }
    else
    {
        M_courses.push_back( M_queued_action );
    }

    //
    // updater ball holder
    //
    if ( wm.self().isKickable()
         && ! wm.self().isFrozen() )
    {
        M_first_ball_pos = wm.ball().pos();
        M_first_ball_vel = wm.ball().vel();
    }
    // else if ( ! wm.existKickableTeammate()
    //           && ! wm.existKickableOpponent()
    //           && wm.interceptTable()->selfReachCycle() <= 1 )
    // {
    //     M_first_ball_pos = wm.ball().pos() + wm.ball()vel();
    //     M_first_ball_vel = wm.ball().vel() + ServerParam::i().ballDecay();
    // }
    else
    {
        return;
    }

#ifdef DEBUG_PROFILE
    Timer timer;
#endif

    createCourses( wm );

    std::sort( M_courses.begin(), M_courses.end(),
               CooperativeAction::DistCompare( ServerParam::i().theirTeamGoalPos() ) );

#ifdef DEBUG_PROFILE
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (generate) PROFILE size=%d/%d elapsed %.3f [ms]",
                  (int)M_courses.size(),
                  M_total_count,
                  timer.elapsedReal() );
#endif
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::setQueuedAction( const rcsc::WorldModel & wm,
                                        CooperativeAction::Ptr action )
{
    M_queued_action_time = wm.time();
    M_queued_action = action;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::createCourses( const WorldModel & wm )
{
    static const int angle_div = 16;
    static const double angle_step = 360.0 / angle_div;

    const ServerParam & SP = ServerParam::i();

    const PlayerType & ptype = wm.self().playerType();

    const double my_first_speed = wm.self().vel().r();

    //
    // angle loop
    //

    for ( int a = 0; a < angle_div; ++a )
    {
        AngleDeg dash_angle = wm.self().body() + ( angle_step * a );

        //
        // angle filter
        //

        if ( wm.self().pos().x < 16.0
             && dash_angle.abs() > 100.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (createTargetPoints) canceled(1) dash_angle=%.1f",
                          dash_angle.degree() );
#endif
            continue;
        }

        if ( wm.self().pos().x < -36.0
             && wm.self().pos().absY() < 20.0
             && dash_angle.abs() > 45.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (createTargetPoints) canceled(2) dash_angle=%.1f",
                          dash_angle.degree() );
#endif
            continue;
        }

        int n_turn = 0;

        double my_speed = my_first_speed * ptype.playerDecay(); // first action is kick
        double dir_diff = AngleDeg( angle_step * a ).abs();

        while ( dir_diff > 10.0 )
        {
            dir_diff -= ptype.effectiveTurn( SP.maxMoment(), my_speed );
            if ( dir_diff < 0.0 ) dir_diff = 0.0;
            my_speed *= ptype.playerDecay();
            ++n_turn;
        }

        if ( n_turn >= 3 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::DRIBBLE,
                          __FILE__": (createTargetPoints) canceled dash_angle=%.1f n_turn=%d",
                          dash_angle.degree(), n_turn );
#endif
            continue;
        }

#if 0
        if ( a == 0 )
        {
            //
            // simulate only dashes (no kick, no turn)
            //
            simulateDashes( wm );
        }
#endif

        if ( angle_step * a < 180.0 )
        {
            dash_angle -= dir_diff;
        }
        else
        {
            dash_angle += dir_diff;
        }

        simulateKickTurnsDashes( wm, dash_angle, n_turn );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::simulateDashes( const WorldModel & wm )
{
    const int opponent_reach_step = wm.interceptTable()->opponentReachCycle();
    if ( opponent_reach_step <= 1 )
    {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      __FILE__": (simulateDashes) exist reachable opponent" );
#endif
        return;
    }

    const ServerParam & SP = ServerParam::i();

    const double max_x = SP.pitchHalfLength() - 0.5;
    const double max_y = SP.pitchHalfWidth() - 0.5;

    const PlayerType & ptype = wm.self().playerType();
    const double kickable_area = ptype.kickableArea();

    const AngleDeg dash_angle = wm.self().body();

    const Vector2D first_self_pos = wm.self().pos();
    const Vector2D first_ball_pos = wm.ball().pos();

    const Vector2D unit_accel = Vector2D::polar2vector( 1.0, dash_angle );

    Vector2D self_pos = first_self_pos;
    Vector2D self_vel = wm.self().vel();
    StaminaModel stamina = wm.self().staminaModel();

    Vector2D ball_pos = first_ball_pos;
    Vector2D ball_vel = wm.ball().vel();

    for ( int n_dash = 1; n_dash <= 20; ++n_dash )
    {
        if ( opponent_reach_step <= n_dash )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d <= opponent_reach_step=%d",
                          n_dash, opponent_reach_step );
#endif
            break;
        }

        ball_pos += ball_vel;

        if ( ball_pos.absX() > max_x
             || ball_pos.absY() > max_y )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d ball_pos(%.2f %.2f) out of pitch",
                          n_dash,
                          ball_pos.x, ball_pos.y );
#endif
            break;
        }

        self_pos += self_vel;

        Vector2D ball_rel = ball_pos - self_pos;
        ball_rel.rotate( -dash_angle );

        if ( ball_rel.x < -kickable_area )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d. ball_rel(%.2f %.2f) ball is back",
                          n_dash,
                          ball_rel.x, ball_rel.y );
#endif
            break;
        }

        if ( ball_rel.absY() > kickable_area )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d. ball_rel(%.2f %.2f) y > kickable=%.3f",
                          n_dash,
                          ball_rel.x, ball_rel.y,
                          kickable_area );
#endif
            break;
        }

        //
        // check kickable area
        //

        double self_noise = std::min( 0.1, first_self_pos.dist( self_pos ) * SP.playerRand() );
        double ball_noise = std::min( 0.1, first_ball_pos.dist( ball_pos ) * SP.ballRand() );

        double dash_power = stamina.getSafetyDashPower( ptype,
                                                        SP.maxDashPower() );
        double max_accel_len = dash_power * ptype.dashPowerRate() * stamina.effort();

        if ( ball_rel.r2()
             > std::pow( max_accel_len + kickable_area - self_noise - ball_noise - 0.15, 2 ) )
        {
            // never kickable
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d. ball_rel(%.2f %.2f) dist=%.3f never kickable",
                          n_dash,
                          ball_rel.x, ball_rel.y,
                          ball_rel.r() );
#endif
            break;
        }

        Vector2D max_dash_accel = unit_accel * max_accel_len;
        Segment2D accel_segment( self_pos, self_pos + max_dash_accel );

        if ( accel_segment.dist( ball_pos )
             > kickable_area - self_noise - ball_noise - 0.15 )
        {
            // never kickable
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d. ball_rel(%.2f %.2f) segment_dist=%.3f never kickable",
                          n_dash,
                          ball_rel.x, ball_rel.y,
                          accel_segment.dist( ball_pos ) );
#endif
            break;
        }

        //
        // simulate next dash
        //

#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulateDashes) n_dash=%d. self=(%.2f %.2f) ball=(%.2f %.2f)",
                      n_dash,
                      self_pos.x, self_pos.y,
                      ball_pos.x, ball_pos.y );
#endif

        double dash_accel_len = -1.0;
        const double min_accel_len = std::min( 0.3, max_accel_len - 0.001 );
        for ( double len = max_accel_len; len > min_accel_len; len -= 0.05 )
        {
            Vector2D tmp_accel = unit_accel * len;
            Vector2D tmp_self_pos = self_pos + tmp_accel;

            double ball_dist = tmp_self_pos.dist( ball_pos );

#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "____ test: accel=%.3f self=(%.2f %.2f) bdist=%.3f kickable=%.3f(%.3f s%.3f b%.3f) col=%.3f",
                          len,
                          tmp_self_pos.x, tmp_self_pos.y,
                          ball_dist,
                          kickable_area - self_noise - ball_noise - 0.15,
                          kickable_area, self_noise, ball_noise,
                          ptype.playerSize() + SP.ballSize() + 0.2 - 0.1*n_dash );
#endif

            if ( ball_dist < kickable_area - self_noise - ball_noise - 0.15
                 && ball_dist > ptype.playerSize() + SP.ballSize() + 0.2 - 0.1*n_dash )
            {
                dash_accel_len = len;
                break;
            }
        }

        if ( dash_accel_len < 0.0 )
        {
#ifdef DEBUG_PRINT_SIMULATE_DASHES
            dlog.addText( Logger::DRIBBLE,
                          "(simulateDashes) NG n_dash=%d. not found",
                          n_dash );
#endif
            break;
        }

        double adjust_dash_power = dash_accel_len / ( ptype.dashPowerRate() * stamina.effort() );
        Vector2D adjust_dash_accel = unit_accel * dash_accel_len;
        self_pos += adjust_dash_accel; // add accel
        self_vel += adjust_dash_accel;
        self_vel *= ptype.playerDecay();
        stamina.simulateDash( ptype, adjust_dash_power );

        ball_vel *= SP.ballDecay();

        CooperativeAction::Ptr ptr( new Dribble( wm.self().unum(),
                                                 ball_pos,
                                                 wm.ball().vel().r(),
                                                 0, // n_kick
                                                 0, // n_turn
                                                 n_dash,
                                                 "nokickDribble" ) );
        ptr->setIndex( M_total_count );
        ptr->setFirstDashPower( adjust_dash_power );
        M_courses.push_back( ptr );

#ifdef DEBUG_PRINT_SIMULATE_DASHES
        dlog.addText( Logger::DRIBBLE,
                      "(simulateDashes) ok n_dash=%d register dash_power=%.2f accel=(%.2f %.2f)",
                      n_dash,
                      adjust_dash_power,
                      adjust_dash_accel.x, adjust_dash_accel.y );
#endif
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::simulateKickTurnsDashes( const WorldModel & wm,
                                                const AngleDeg & dash_angle,
                                                const int n_turn )
{
    //static const int max_dash = 5;
    static const int max_dash = 4; // 2009-06-29
    static const int min_dash = 2;
    //static const int min_dash = 1;

#ifdef __APPLE__
    static std::vector< Vector2D > self_cache;
#else
    static thread_local std::vector< Vector2D > self_cache;
#endif

    //
    // create self position cache
    //
    createSelfCache( wm, dash_angle, n_turn, max_dash, self_cache );

    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    const Vector2D trap_rel
        = Vector2D::polar2vector( ptype.playerSize() + ptype.kickableMargin() * 0.2 + SP.ballSize(),
                                  dash_angle );

    const double max_x = ( SP.keepawayMode()
                           ? SP.keepawayLength() * 0.5 - 1.5
                           : SP.pitchHalfLength() - 1.0 );
    const double max_y = ( SP.keepawayMode()
                           ? SP.keepawayWidth() * 0.5 - 1.5
                           : SP.pitchHalfWidth() - 1.0 );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::DRIBBLE,
                  __FILE__": (simulateKickTurnsDashes) dash_angle=%.1f n_turn=%d",
                  dash_angle.degree(), n_turn );
#endif

    for ( int n_dash = max_dash; n_dash >= min_dash; --n_dash )
    {
        const Vector2D ball_trap_pos = self_cache[n_turn + n_dash] + trap_rel;

        ++M_total_count;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::DRIBBLE,
                      "%d: n_turn=%d n_dash=%d ball_trap=(%.3f %.3f)",
                      M_total_count,
                      n_turn, n_dash,
                      ball_trap_pos.x, ball_trap_pos.y );
#endif
        if ( ball_trap_pos.absX() > max_x
             || ball_trap_pos.absY() > max_y )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx out of pitch" );
            debug_paint_failed( M_total_count, ball_trap_pos );
#endif
            continue;
        }

        const double term
            = ( 1.0 - std::pow( SP.ballDecay(), 1 + n_turn + n_dash ) )
            / ( 1.0 - SP.ballDecay() );
        const Vector2D first_vel = ( ball_trap_pos - M_first_ball_pos ) / term;
        const Vector2D kick_accel = first_vel - M_first_ball_vel;
        const double kick_power = kick_accel.r() / wm.self().kickRate();

        if ( kick_power > SP.maxPower()
             || kick_accel.r2() > std::pow( SP.ballAccelMax(), 2 )
             || first_vel.r2() > std::pow( SP.ballSpeedMax(), 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx cannot kick. first_vel=(%.1f %.1f, r=%.2f) accel=(%.1f %.1f)r=%.2f power=%.1f",
                          M_total_count,
                          first_vel.x, first_vel.y, first_vel.r(),
                          kick_accel.x, kick_accel.y, kick_accel.r(),
                          kick_power );
            debug_paint_failed( M_total_count, ball_trap_pos );
#endif
            continue;
        }

        if ( ( M_first_ball_pos + first_vel ).dist2( self_cache[0] )
             < std::pow( ptype.playerSize() + SP.ballSize() + 0.1, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx collision. first_vel=(%.1f %.1f, r=%.2f) accel=(%.1f %.1f)r=%.2f power=%.1f",
                          M_total_count,
                          first_vel.x, first_vel.y, first_vel.r(),
                          kick_accel.x, kick_accel.y, kick_accel.r(),
                          kick_power );
            debug_paint_failed( M_total_count, ball_trap_pos );
#endif
            continue;
        }

        if ( checkOpponent( wm, ball_trap_pos, 1 + n_turn + n_dash ) )
        {
            CooperativeAction::Ptr ptr( new Dribble( wm.self().unum(),
                                                     ball_trap_pos,
                                                     first_vel.r(),
                                                     1, // n_kick
                                                     n_turn,
                                                     n_dash,
                                                     "shortDribble" ) );
            ptr->setIndex( M_total_count );
            M_courses.push_back( ptr );

#ifdef DEBUG_PRINT_SUCCESS_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: ok trap_pos=(%.2f %.2f) first_vel=(%.1f %.1f, r=%.2f) n_turn=%d n_dash=%d",
                          M_total_count,
                          ball_trap_pos.x, ball_trap_pos.y,
                          first_vel.x, first_vel.y, first_vel.r(),
                          n_turn, n_dash );
            dlog.addCircle( Logger::DRIBBLE,
                            ball_trap_pos.x, ball_trap_pos.y, 0.1,
                            "#00ff00" );
            char num[8]; snprintf( num, 8, "%d", M_total_count );
            dlog.addMessage( Logger::DRIBBLE,
                             ball_trap_pos, num );
#endif
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
ShortDribbleGenerator::createSelfCache( const WorldModel & wm,
                                        const AngleDeg & dash_angle,
                                        const int n_turn,
                                        const int n_dash,
                                        std::vector< Vector2D > & self_cache )
{
    const ServerParam & SP = ServerParam::i();
    const PlayerType & ptype = wm.self().playerType();

    self_cache.clear();

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    my_pos += my_vel;
    my_vel *= ptype.playerDecay();

    self_cache.push_back( my_pos ); // first element is next cycle just after kick

    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        self_cache.push_back( my_pos );
    }

    stamina_model.simulateWaits( ptype, 1 + n_turn );

    const Vector2D unit_vec = Vector2D::polar2vector( 1.0, dash_angle );

    for ( int i = 0; i < n_dash; ++i )
    {
        double available_stamina = std::max( 0.0,
                                             stamina_model.stamina() - SP.recoverDecThrValue() - 300.0 );
        double dash_power = std::min( available_stamina, SP.maxDashPower() );
        Vector2D dash_accel = unit_vec.setLengthVector( dash_power * ptype.dashPowerRate() * stamina_model.effort() );

        my_vel += dash_accel;
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();

        stamina_model.simulateDash( ptype, dash_power );

        self_cache.push_back( my_pos );
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
ShortDribbleGenerator::checkOpponent( const WorldModel & wm,
                                      const Vector2D & ball_trap_pos,
                                      const int dribble_step )
{
    const ServerParam & SP = ServerParam::i();

    //const double control_area = SP.tackleDist() - 0.2;
    const rcsc::AngleDeg ball_move_angle = ( ball_trap_pos - M_first_ball_pos ).th();

    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        if ( (*o)->distFromSelf() > 20.0 ) break;

        const PlayerType * ptype = (*o)->playerTypePtr();

        const double control_area
            = ( (*o)->goalie()
                && ball_trap_pos.x > SP.theirPenaltyAreaLineX()
                && ball_trap_pos.absY() < SP.penaltyAreaHalfWidth() )
            ? SP.catchableArea()
            : ptype->kickableArea();

        const Vector2D opp_pos = (*o)->inertiaPoint( dribble_step );

        const Vector2D ball_to_opp_rel = ( (*o)->pos() - M_first_ball_pos ).rotatedVector( -ball_move_angle );

        if ( ball_to_opp_rel.x < -4.0 )
        {
#ifdef DEBUG_PRINT_OPPONENT
            dlog.addText( Logger::DRIBBLE,
                          "%d: opponent[%d](%.2f %.2f) relx=%.2f",
                          M_total_count,
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          ball_to_opp_rel.x );
#endif
            continue;
        }


        double target_dist = opp_pos.dist( ball_trap_pos );

        if ( target_dist - control_area < 0.001 )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx opponent %d(%.1f %.1f) kickable.",
                          M_total_count,
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y );
            debug_paint_failed( M_total_count, ball_trap_pos );
#endif
            return false;
        }

        //
        // dash
        //

        double dash_dist = target_dist;
        dash_dist -= control_area * 0.5;
        dash_dist -= 0.2;

        int n_dash = ptype->cyclesToReachDistance( dash_dist );

        //
        // turn
        //

        int n_turn = ( (*o)->bodyCount() > 1
                       ? 1
                       : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                   (*o)->body(),
                                                                   (*o)->vel().r(),
                                                                   target_dist,
                                                                   ( ball_trap_pos - opp_pos ).th(),
                                                                   control_area,
                                                                   true ) );


        int n_step = ( n_turn == 0
                       ? n_turn + n_dash
                       : n_turn + n_dash + 1 );

        int bonus_step = 0;

        if ( ball_trap_pos.x < 30.0 )
        {
            bonus_step += 1;
        }

        if ( ball_trap_pos.x < 0.0 )
        {
            bonus_step += 1;
        }

        if ( (*o)->isTackling() )
        {
            bonus_step = -5;
        }

        if ( ball_to_opp_rel.x > 0.5 )
        {
            bonus_step += bound( 0, (*o)->posCount(), 8 );
        }
        else
        {
            bonus_step += bound( 0, (*o)->posCount(), 4 );
        }

        if ( n_step - bonus_step <= dribble_step )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx opponent %d(%.1f %.1f) can reach."
                          " myStep=%d oppStep=%d(t:%d,d:%d) bonus=%d",
                          M_total_count,
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          dribble_step,
                          n_step,
                          n_turn,
                          n_dash,
                          bonus_step );
            debug_paint_failed( M_total_count, ball_trap_pos );
#endif
            return false;
        }

#ifdef DEBUG_PRINT_OPPONENT
        dlog.addText( Logger::DRIBBLE,
                      "%d: (opponent) myStep=%d opponent[%d](%.1f %.1f)"
                      " dashDist=%.2f oppStep=%d(t:%d,d:%d) bonus=%d",
                      M_total_count,
                      dribble_step,
                      (*o)->unum(),
                      (*o)->pos().x, (*o)->pos().y,
                      dash_dist,
                      n_step,
                      n_turn,
                      n_dash,
                      bonus_step );
#endif
    }

    return true;
}
