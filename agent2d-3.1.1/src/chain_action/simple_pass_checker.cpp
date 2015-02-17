// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA, Hidehisa AKIYAMA

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

#include "simple_pass_checker.h"

#include "predict_state.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

using namespace rcsc;

static const double GOALIE_PASS_EVAL_THRESHOLD = 17.5;
static const double BACK_PASS_EVAL_THRESHOLD = 17.5;
static const double PASS_EVAL_THRESHOLD = 14.5;
static const double CHANCE_PASS_EVAL_THRESHOLD = 14.5;
static const double PASS_RECEIVER_PREDICT_STEP = 2.5;

static const double PASS_REFERENCE_SPEED = 2.5;

static const double PASS_SPEED_THRESHOLD = 1.5;

static const double NEAR_PASS_DIST_THR = 4.0;
static const double FAR_PASS_DIST_THR = 35.0;

static const long VALID_TEAMMATE_ACCURACY = 8;
static const long VALID_OPPONENT_ACCURACY = 20;
static const double OPPONENT_DIST_THR2 = std::pow( 5.0, 2 );

/*-------------------------------------------------------------------*/
/*!

 */
bool
SimplePassChecker::operator()( const PredictState & state,
                               const AbstractPlayerObject & from,
                               const AbstractPlayerObject & to,
                               const Vector2D & receive_point,
                               const double & first_ball_speed ) const
{
    //
    // inhibit self pass
    //
    if ( from.unum() == to.unum() )
    {
        return false;
    }

    if ( first_ball_speed < PASS_SPEED_THRESHOLD )
    {
        return false;
    }

    if ( from.isGhost()
         || to.isGhost()
         || from.posCount() > VALID_TEAMMATE_ACCURACY
         || to.posCount() > VALID_TEAMMATE_ACCURACY )
    {
        return false;
    }

    const Vector2D from_pos = ( from.isSelf()
                                ? state.ball().pos()
                                : from.pos() );
    const double pass_dist = from_pos.dist( receive_point );

    if ( pass_dist <= NEAR_PASS_DIST_THR )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) too near. dist=%.2f",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_dist );
#endif
        return false;
    }

    if ( pass_dist >= FAR_PASS_DIST_THR )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) too far. dist=%.2f",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_dist );
#endif
        return false;
    }

    if ( to.pos().x >= state.offsideLineX() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) offsideX=%.2f",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      state.offsideLineX() );
#endif
        return false;
    }

    if ( receive_point.x <= ServerParam::i().ourPenaltyAreaLineX() + 3.0
         && receive_point.absY() <= ServerParam::i().penaltyAreaHalfWidth() + 3.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) in penalty area",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return false;
    }

    if ( receive_point.absX() >= ServerParam::i().pitchHalfLength()
         || receive_point.absY() >= ServerParam::i().pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) out of field",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return false;
    }

    if ( to.goalie()
         && receive_point.x < ServerParam::i().ourPenaltyAreaLineX() + 1.0
         && receive_point.absY() < ServerParam::i().penaltyAreaHalfWidth() + 1.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) our goalie can catch",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return false;
    }

    if ( to.goalie()
         && receive_point.x > state.ourDefenseLineX() - 3.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) our goalie over defense line",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y );
#endif
        return false;
    }

    const double receiver_move_dist = to.pos().dist( receive_point );

    const AngleDeg pass_angle = ( receive_point - from_pos ).th();
    const double OVER_TEAMMATE_IGNORE_DISTANCE2
        = std::pow( pass_dist + ( receive_point.x >= +25.0 ? 2.0 : 6.0 ), 2 );

    double pass_course_cone = + 360.0;

    const PlayerPtrCont::const_iterator o_end = state.opponentsFromSelf().end();
    for ( PlayerPtrCont::const_iterator opp = state.opponentsFromSelf().begin();
          opp != o_end;
          ++opp )
    {
        if ( (*opp)->posCount() > VALID_OPPONENT_ACCURACY )
        {
            continue;
        }

        if ( ( (*opp)->pos() - receive_point ).r2() < OPPONENT_DIST_THR2 )
        {
            return false;
        }

        Vector2D opp_pos = (*opp)->inertiaFinalPoint();

        const double opp_dist2 = from_pos.dist2( opp_pos );

        if ( opp_dist2 > OVER_TEAMMATE_IGNORE_DISTANCE2 )
        {
            continue;
        }

        const double opp_move_dist = opp_pos.dist( receive_point );

        if ( opp_move_dist < receiver_move_dist * 0.85 )
        {
            return false;
        }

        double angle_diff = ( ( opp_pos - from_pos ).th() - pass_angle ).abs();

        if ( from.isSelf() )
        {
            const double control_area = (*opp)->playerTypePtr()->kickableArea();
            const double hide_radian = std::asin( std::min( control_area / std::sqrt( opp_dist2 ),
                                                      1.0 ) );
            angle_diff = std::max( angle_diff - AngleDeg::rad2deg( hide_radian ), 0.0 );
        }

        if ( pass_course_cone > angle_diff )
        {
            pass_course_cone = angle_diff;
        }
    }


    double eval_threshold = PASS_EVAL_THRESHOLD;

    if ( to.pos().x - 2.0 <= from.pos().x )
    {
        eval_threshold = BACK_PASS_EVAL_THRESHOLD;
    }

    if ( from.pos().x >= +25.0
         && to.pos().x >= +25.0 )
    {
        eval_threshold = CHANCE_PASS_EVAL_THRESHOLD;
    }

    if ( from.goalie() )
    {
        eval_threshold = GOALIE_PASS_EVAL_THRESHOLD;
    }


    // adjust angle threshold by ball speed
    eval_threshold *= ( PASS_REFERENCE_SPEED / first_ball_speed );

    if ( pass_course_cone <= eval_threshold )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::PASS,
                      "(SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) too narrow. angleWidth=%.3f",
                      from.unum(), to.unum(),
                      from_pos.x, from_pos.y,
                      receive_point.x, receive_point.y,
                      pass_course_cone );
#endif
        return false;
    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::PASS,
                  "ok (SimplePassChecker) %d to %d: (%.2f %.2f)->(%.2f %.2f) angleWidth=%.3f",
                  from.unum(), to.unum(),
                  from_pos.x, from_pos.y,
                  receive_point.x, receive_point.y,
                  pass_course_cone );
#endif
    return true;
}
