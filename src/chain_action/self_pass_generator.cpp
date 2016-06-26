// -*-c++-*-

/*!
  \file self_pass_generator.cpp
  \brief self pass generator Source File
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

#include "self_pass_generator.h"

#include "dribble.h"
#include "field_analyzer.h"

#include <rcsc/action/kick_table.h>
#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <limits>
#include <cmath>

#define DEBUG_PROFILE
// #define DEBUG_PRINT
// #define DEBUG_PRINT_SELF_CACHE
// #define DEBUG_PRINT_OPPONENT
// #define DEBUG_PRINT_OPPONENT_LEVEL2

// #define DEBUG_PRINT_SUCCESS_COURSE
// #define DEBUG_PRINT_FAILED_COURSE

using namespace rcsc;


namespace {

inline
void
debug_paint_failed( const int count,
                    const Vector2D & receive_point )
{
    dlog.addRect( Logger::DRIBBLE,
                  receive_point.x - 0.1, receive_point.y - 0.1,
                  0.2, 0.2,
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
SelfPassGenerator::SelfPassGenerator()
{
    M_courses.reserve( 128 );

    clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
SelfPassGenerator &
SelfPassGenerator::instance()
{
#ifdef __APPLE__
    static SelfPassGenerator s_instance;
#else
    static thread_local SelfPassGenerator s_instance;
#endif
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SelfPassGenerator::clear()
{
    M_total_count = 0;
    M_courses.clear();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SelfPassGenerator::generate( const WorldModel & wm )
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

    if ( ! wm.self().isKickable()
         || wm.self().isFrozen() )
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
SelfPassGenerator::createCourses( const WorldModel & wm )
{
    static const int ANGLE_DIVS = 60;
    static const double ANGLE_STEP = 360.0 / ANGLE_DIVS;

#ifdef __APPLE__
    static std::vector< Vector2D > self_cache( 24 );
#else
    static thread_local std::vector< Vector2D > self_cache( 24 );
#endif

    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = wm.ball().pos();
    const AngleDeg body_angle = wm.self().body();

    const int min_dash = 5;
    const int max_dash = ( ball_pos.x < -20.0 ? 6
                           : ball_pos.x < 0.0 ? 7
                           : ball_pos.x < 10.0 ? 13
                           : ball_pos.x < 20.0 ? 15
                           : 20 );

    const PlayerType & ptype = wm.self().playerType();

    const double max_effective_turn
        = ptype.effectiveTurn( SP.maxMoment(),
                               wm.self().vel().r() * ptype.playerDecay() );

    const Vector2D our_goal = ServerParam::i().ourTeamGoalPos();
    const double goal_dist_thr2 = std::pow( 18.0, 2 ); // Magic Number

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const double add_angle = ANGLE_STEP * a;

        int n_turn = 0;
        if ( a != 0 )
        {
            if ( AngleDeg( add_angle ).abs() > max_effective_turn )
            {
                // cannot turn by 1 step
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::DRIBBLE,
                              "?: xxx SelfPass rel_angle=%.1f > maxTurn=%.1f cannot turn by 1 step.",
                              add_angle, max_effective_turn );
#endif
                continue;
            }

            n_turn = 1;
        }

        const AngleDeg dash_angle = body_angle + add_angle;

        if ( ball_pos.x < SP.theirPenaltyAreaLineX() + 5.0
             && dash_angle.abs() > 85.0 ) // Magic Number
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "?: xxx SelfPass angle=%.1f over angle.",
                          dash_angle.degree() );
#endif
            continue;
        }

        createSelfCache( wm, dash_angle,
                         n_turn, max_dash,
                         self_cache );

        int n_dash = self_cache.size() - n_turn;

        if ( n_dash < min_dash )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "?: xxx SelfPass angle=%.1f turn=%d dash=%d too short dash step.",
                          dash_angle.degree(),
                          n_turn, n_dash );
#endif
            continue;
        }

#if (defined DEBUG_PRINT_SUCCESS_COURSE) || (defined DEBUG_PRINT_SUCCESS_COURSE)
        dlog.addText( Logger::DRIBBLE,
                      "===== SelfPass angle=%.1f turn=%d dash=%d =====",
                      dash_angle.degree(),
                      n_turn,
                      n_dash );
#endif

        int count = 0;
        int dash_dec = 2;
        for ( ; n_dash >= min_dash; n_dash -= dash_dec )
        {
            ++M_total_count;

            if ( n_dash <= 10 )
            {
                dash_dec = 1;
            }

            const Vector2D receive_pos = self_cache[n_turn + n_dash - 1];

            if ( receive_pos.dist2( our_goal ) < goal_dist_thr2 )
            {
#ifdef DEBUG_PRINT_FAILED_COURSE
                dlog.addText( Logger::DRIBBLE,
                              "%d: xxx SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) near our goal",
                              M_total_count,
                              1 + n_turn, n_turn, n_dash,
                              receive_pos.x, receive_pos.y );
#endif
                continue;
            }

            if ( ! canKick( wm, n_turn, n_dash, receive_pos ) )
            {
                continue;
            }

            if ( ! checkOpponent( wm, n_turn, n_dash, ball_pos, receive_pos ) )
            {
                continue;
            }

            // double first_speed = calc_first_term_geom_series( ball_pos.dist( receive_pos ),
            //                                                   ServerParam::i().ballDecay(),
            //                                                   1 + n_turn + n_dash );
            double first_speed = SP.firstBallSpeed( ball_pos.dist( receive_pos ),
                                                    1 + n_turn + n_dash );

            CooperativeAction::Ptr ptr( new Dribble( wm.self().unum(),
                                                     receive_pos,
                                                     first_speed,
                                                     1, // 1 kick
                                                     n_turn,
                                                     n_dash,
                                                     "SelfPass" ) );
            ptr->setIndex( M_total_count );
            M_courses.push_back( ptr );

#ifdef DEBUG_PRINT_SUCCESS_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: ok SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f) speed=%.3f",
                          M_total_count,
                          1 + n_turn + n_dash, n_turn, n_dash,
                          receive_pos.x, receive_pos.y,
                          first_speed );
            char num[8];
            snprintf( num, 8, "%d", M_total_count );
            dlog.addMessage( Logger::DRIBBLE,
                             receive_pos, num );
            dlog.addRect( Logger::DRIBBLE,
                          receive_pos.x - 0.1, receive_pos.y - 0.1,
                          0.2, 0.2,
                          "#00ff00" );
#endif

            ++count;
            if ( count >= 10 )
            {
                break;
            }
        }
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
void
SelfPassGenerator::createSelfCache( const WorldModel & wm,
                                    const AngleDeg & dash_angle,
                                    const int n_turn,
                                    const int n_dash,
                                    std::vector< Vector2D > & self_cache )
{
    self_cache.clear();

    const PlayerType & ptype = wm.self().playerType();

    const double dash_power = ServerParam::i().maxDashPower();
    const double stamina_thr = ( wm.self().staminaModel().capacityIsEmpty()
                                 ? -ptype.extraStamina() // minus value to set available stamina
                                 : ServerParam::i().recoverDecThrValue() + 350.0 );

    StaminaModel stamina_model = wm.self().staminaModel();

    Vector2D my_pos = wm.self().pos();
    Vector2D my_vel = wm.self().vel();

    //
    // 1 kick
    //
    my_pos += my_vel;
    my_vel *= ptype.playerDecay();
    stamina_model.simulateWait( ptype );

    //
    // turns
    //
    for ( int i = 0; i < n_turn; ++i )
    {
        my_pos += my_vel;
        my_vel *= ptype.playerDecay();
        stamina_model.simulateWait( ptype );

        self_cache.push_back( my_pos );
    }

    //
    // simulate dashes
    //
    for ( int i = 0; i < n_dash; ++i )
    {
        if ( stamina_model.stamina() < stamina_thr )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d. stamina=%.1f < threshold",
                          n_turn, n_dash, stamina_model.stamina() );
#endif
            break;
        }

        double available_stamina =  std::max( 0.0, stamina_model.stamina() - stamina_thr );
        double actual_dash_power = std::min( dash_power, available_stamina );
        double accel_mag = actual_dash_power * ptype.dashPowerRate() * stamina_model.effort();
        Vector2D dash_accel = Vector2D::polar2vector( accel_mag, dash_angle );

        // TODO: check playerSpeedMax & update actual_dash_power if necessary
        // if ( ptype.normalizeAccel( my_vel, &dash_accel ) ) actual_dash_power = ...

        my_vel += dash_accel;
        my_pos += my_vel;

// #ifdef DEBUG_PRINT_SELF_CACHE
//         dlog.addText( Logger::DRIBBLE,
//                       "___ dash=%d accel=(%.2f %.2f)r=%.2f th=%.1f pos=(%.2f %.2f) vel=(%.2f %.2f)",
//                       i + 1,
//                       dash_accel.x, dash_accel.y, dash_accel.r(), dash_accel.th().degree(),
//                       my_pos.x, my_pos.y,
//                       my_vel.x, my_vel.y );
// #endif

        if ( my_pos.x > ServerParam::i().pitchHalfLength() - 2.5 )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d. my_x=%.2f. over goal line",
                          n_turn, n_dash, my_pos.x );
#endif
            break;
        }

        if ( my_pos.absY() > ServerParam::i().pitchHalfWidth() - 3.0
             && ( ( my_pos.y > 0.0 && dash_angle.degree() > 0.0 )
                  || ( my_pos.y < 0.0 && dash_angle.degree() < 0.0 ) )
             )
        {
#ifdef DEBUG_PRINT_SELF_CACHE
            dlog.addText( Logger::DRIBBLE,
                          "?: SelfPass (createSelfCache) turn=%d dash=%d."
                          " my_pos=(%.2f %.2f). dash_angle=%.1f",
                          n_turn, n_dash,
                          my_pos.x, my_pos.y,
                          dash_angle.degree() );
            dlog.addText( Logger::DRIBBLE,
                          "__ dash_accel=(%.2f %.2f)r=%.2f  vel=(%.2f %.2f)r=%.2f th=%.1f",
                          dash_accel.x, dash_accel.y, accel_mag,
                          my_vel.x, my_vel.y, my_vel.r(), my_vel.th().degree() );

#endif

            break;
        }

        my_vel *= ptype.playerDecay();
        stamina_model.simulateDash( ptype, actual_dash_power );

        self_cache.push_back( my_pos );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SelfPassGenerator::canKick( const WorldModel & wm,
                            const int n_turn,
                            const int n_dash,
                            const Vector2D & receive_pos )
{
    const ServerParam & SP = ServerParam::i();

    const Vector2D ball_pos = wm.ball().pos();
    const Vector2D ball_vel = wm.ball().vel();
    const AngleDeg target_angle = ( receive_pos - ball_pos ).th();

    //
    // check kick possibility
    //
    double first_speed = calc_first_term_geom_series( ball_pos.dist( receive_pos ),
                                                      SP.ballDecay(),
                                                      1 + n_turn + n_dash );
    Vector2D max_vel = KickTable::calc_max_velocity( target_angle,
                                                     wm.self().kickRate(),
                                                     ball_vel );
    if ( max_vel.r2() < std::pow( first_speed, 2 ) )
    {
#ifdef DEBUG_PRINT_FAILED_COURSE
        dlog.addText( Logger::DRIBBLE,
                      "%d: xxx SelfPass step=%d(t:%d,d=%d) cannot kick by 1 step."
                      " first_speed=%.2f > max_speed=%.2f",
                      M_total_count,
                      1 + n_turn + n_dash, n_turn, n_dash,
                      first_speed,
                      max_vel.r() );
        debug_paint_failed( M_total_count, receive_pos );
#endif
        return false;
    }

    //
    // check collision
    //
    const Vector2D my_next = wm.self().pos() + wm.self().vel();
    const Vector2D ball_next
        = ball_pos
        + ( receive_pos - ball_pos ).setLengthVector( first_speed );

    if ( my_next.dist2( ball_next ) < std::pow( wm.self().playerType().playerSize()
                                                + SP.ballSize()
                                                + 0.1,
                                                2 ) )
    {
#ifdef DEBUG_PRINT_FAILED_COURSE
        dlog.addText( Logger::DRIBBLE,
                      "%d: xxx SelfPass step=%d(t:%d,d=%d) maybe collision. next_dist=%.3f first_speed=%.2f",
                      M_total_count,
                      1 + n_turn + n_dash, n_turn, n_dash,
                      my_next.dist( ball_next ),
                      first_speed );
        debug_paint_failed( M_total_count, receive_pos );
#endif
        return false;
    }

    //
    // check opponent kickable area
    //

    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        const PlayerType * ptype = (*o)->playerTypePtr();
        Vector2D o_next = (*o)->pos() + (*o)->vel();

        const double control_area = ( ( (*o)->goalie()
                                        && ball_next.x > SP.theirPenaltyAreaLineX()
                                        && ball_next.absY() < SP.penaltyAreaHalfWidth() )
                                      ? SP.catchableArea()
                                      : ptype->kickableArea() );

        if ( ball_next.dist2( o_next ) < std::pow( control_area + 0.1, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx SelfPass (canKick) opponent may be kickable(1) dist=%.3f < control=%.3f + 0.1",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

        if ( (*o)->bodyCount() <= 1 )
        {
            o_next += Vector2D::from_polar( SP.maxDashPower() * ptype->dashPowerRate() * ptype->effortMax(),
                                            (*o)->body() );
        }
        else
        {
            o_next += (*o)->vel().setLengthVector( SP.maxDashPower()
                                                   * ptype->dashPowerRate()
                                                   * ptype->effortMax() );
        }

        if ( ball_next.dist2( o_next ) < std::pow( control_area, 2 ) )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d xxx SelfPass (canKick) opponent may be kickable(2) dist=%.3f < control=%.3f",
                          M_total_count, ball_next.dist( o_next ), control_area );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SelfPassGenerator::checkOpponent( const WorldModel & wm,
                                  const int n_turn,
                                  const int n_dash,
                                  const Vector2D & ball_pos,
                                  const Vector2D & receive_pos )
{
    const ServerParam & SP = ServerParam::i();

    const int self_step = 1 + n_turn + n_dash;

    const AngleDeg target_angle = ( receive_pos - ball_pos ).th();

    const bool in_penalty_area = ( receive_pos.x > SP.theirPenaltyAreaLineX()
                                   && receive_pos.absY() < SP.penaltyAreaHalfWidth() );


#ifdef DEBUG_PRINT_OPPONENT
    dlog.addText( Logger::DRIBBLE,
                  "%d: (checkOpponent) selfStep=%d(t:%d,d:%d) recvPos(%.2f %.2f)",
                  M_total_count,
                  self_step, n_turn, n_dash,
                  receive_pos.x, receive_pos.y );
#endif

    int min_step = 1000;

    const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator o = wm.opponentsFromSelf().begin();
          o != o_end;
          ++o )
    {
        const Vector2D & opos = ( (*o)->seenPosCount() <= (*o)->posCount()
                                  ? (*o)->seenPos()
                                  : (*o)->pos() );
        const Vector2D ball_to_opp_rel = ( opos - ball_pos ).rotatedVector( -target_angle );

        if ( ball_to_opp_rel.x < -4.0 )
        {
#ifdef DEBUG_PRINT_OPPONENT_LEVEL2
            dlog.addText( Logger::DRIBBLE,
                          "__ opponent[%d](%.2f %.2f) relx=%.2f",
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          ball_to_opp_rel.x );
#endif
            continue;
        }

        const Vector2D & ovel = ( (*o)->seenVelCount() <= (*o)->velCount()
                                  ? (*o)->seenVel()
                                  : (*o)->vel() );

        const PlayerType * ptype = (*o)->playerTypePtr();

        const bool goalie = ( (*o)->goalie() && in_penalty_area );
        const double control_area ( goalie
                                    ? SP.catchableArea()
                                    : ptype->kickableArea() );

        Vector2D opp_pos = ptype->inertiaPoint( opos, ovel, self_step );
        double target_dist = opp_pos.dist( receive_pos );

        if ( target_dist
             > ptype->realSpeedMax() * ( self_step + (*o)->posCount() ) + control_area )
        {
#ifdef DEBUG_PRINT_OPPONENT_LEVEL2
            dlog.addText( Logger::DRIBBLE,
                          "__ opponent[%d](%.2f %.2f) too far. ignore. dist=%.1f",
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          target_dist );
#endif
            continue;
        }


        if ( target_dist - control_area < 0.001 )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx SelfPass (checkOpponent) step=%d(t:%d,d=%d) pos=(%.1f %.1f)"
                          " opponent %d(%.1f %.1f) is already at receive point",
                          M_total_count,
                          self_step, n_turn, n_dash,
                          receive_pos.x, receive_pos.y,
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

        double dash_dist = target_dist;
        dash_dist -= control_area;
        dash_dist -= 0.2;
        dash_dist -= (*o)->distFromSelf() * 0.01;

        int opp_n_dash = ptype->cyclesToReachDistance( dash_dist );

        int opp_n_turn = ( (*o)->bodyCount() > 1
                           ? 0
                           : FieldAnalyzer::predict_player_turn_cycle( ptype,
                                                                       (*o)->body(),
                                                                       ovel.r(),
                                                                       target_dist,
                                                                       ( receive_pos - opp_pos ).th(),
                                                                       control_area,
                                                                       false ) );

        int opp_n_step = ( opp_n_turn == 0
                           ? opp_n_turn + opp_n_dash
                           : opp_n_turn + opp_n_dash + 1 );
        int bonus_step = 0;

        if ( receive_pos.x < 27.0 )
        {
            bonus_step += 1;
        }

        if ( (*o)->isTackling() )
        {
            bonus_step = -5;
        }

        if ( ball_to_opp_rel.x > 0.8 )
        {
            bonus_step += 1;
            bonus_step += bound( 0, (*o)->posCount() - 1, 8 );
#ifdef DEBUG_PRINT_OPPONENT_LEVEL2
            dlog.addText( Logger::DRIBBLE,
                          "__ opponent[%d](%.2f %.2f) forward bonus = %d",
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          bonus_step );
#endif
        }
        else
        {
            int penalty_step = ( ( receive_pos.x > wm.offsideLineX()
                                   || receive_pos.x > 35.0 )
                                 ? 1
                                 : 0 );
            bonus_step =  bound( 0, (*o)->posCount() - penalty_step, 3 );
#ifdef DEBUG_PRINT_OPPONENT_LEVEL2
            dlog.addText( Logger::DRIBBLE,
                          "__ opponent[%d](%.2f %.2f) backward bonus = %d",
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          bonus_step );
#endif
        }



        // if ( goalie )
        // {
        //     opp_n_step -= 1;
        // }

#ifdef DEBUG_PRINT_OPPONENT
        dlog.addText( Logger::DRIBBLE,
                      "__ opponent[%d](%.2f %.2f) oppStep=%d(t:%d,d:%d) selfStep=%d rel.x=%.2f",
                      (*o)->unum(),
                      (*o)->pos().x, (*o)->pos().y,
                      opp_n_step, opp_n_turn, opp_n_dash,
                      self_step,
                      ball_to_opp_rel.x );
#endif

        if ( opp_n_step - bonus_step <= self_step )
        {
#ifdef DEBUG_PRINT_FAILED_COURSE
            dlog.addText( Logger::DRIBBLE,
                          "%d: xxx SelfPass step=%d(t:%d,d:%d) pos=(%.1f %.1f)"
                          " opponent %d(%.1f %.1f) can reach."
                          " oppStep=%d(turn=%d,bonus=%d)",
                          M_total_count,
                          1 + n_turn + n_dash, n_turn, n_dash,
                          receive_pos.x, receive_pos.y,
                          (*o)->unum(),
                          (*o)->pos().x, (*o)->pos().y,
                          opp_n_step, opp_n_turn, bonus_step );
            debug_paint_failed( M_total_count, receive_pos );
#endif
            return false;
        }

        if ( min_step > opp_n_step )
        {
            min_step = opp_n_step;
        }
    }

#ifdef DEBUG_PRINT_OPPONENT
    dlog.addText( Logger::DRIBBLE,
                  "%d: (checkOpponent) selfStep=%d(t:%d,d:%d) oppStep=%d",
                  M_total_count,
                  self_step, n_turn, n_dash,
                  min_step );
#endif

    return true;
}
