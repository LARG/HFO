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

#include "actgen_simple_dribble.h"

#include "dribble.h"
#include "field_analyzer.h"

#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/player/world_model.h>
#include <rcsc/common/logger.h>
#include <rcsc/timer.h>

#include <limits>

// #define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_SimpleDribble::generate( std::vector< ActionStatePair > * result,
                                const PredictState & state,
                                const WorldModel & current_wm,
                                const std::vector< ActionStatePair > & path ) const
{
#ifdef __APPLE__
    static GameTime s_last_call_time( 0, 0 );
    static int s_action_count = 0;
#else
    static thread_local GameTime s_last_call_time( 0, 0 );
    static thread_local int s_action_count = 0;
#endif

    if ( current_wm.time() != s_last_call_time )
    {
        s_action_count = 0;
        s_last_call_time = current_wm.time();
    }

    if ( path.empty() )
    {
        return;
    }

    const AbstractPlayerObject * holder = state.ballHolder();

    if ( ! holder )
    {
        return;
    }

    const int ANGLE_DIVS = 8;
    const double ANGLE_STEP = 360.0 / ANGLE_DIVS;
    const int DIST_DIVS = 3;
    const double DIST_STEP = 1.75;

    const ServerParam & SP = ServerParam::i();

    const double max_x= SP.pitchHalfLength() - 1.0;
    const double max_y= SP.pitchHalfWidth() - 1.0;

    const int bonus_step = 2;

    const PlayerType * ptype = holder->playerTypePtr();

    int generated_count = 0;

    for ( int a = 0; a < ANGLE_DIVS; ++a )
    {
        const AngleDeg target_angle = ANGLE_STEP * a;

        if ( holder->pos().x < 16.0
             && target_angle.abs() > 100.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": %d: (%.2f %.2f) danger angle(1) %.1f",
                          generated_count,
                          target_angle.degree() );
#endif
            continue;
        }

        if ( holder->pos().x < -36.0
             && holder->pos().absY() < 20.0
             && target_angle.abs() > 45.0 )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": %d: (%.2f %.2f) danger angle(2) %.1f",
                          generated_count,
                          target_angle.degree() );
#endif
            continue;
        }

        const Vector2D unit_vec = Vector2D::from_polar( 1.0, target_angle );
        for ( int d = 1; d <= DIST_DIVS; ++d )
        {
            const double holder_move_dist = DIST_STEP * d;
            const Vector2D target_point
                = holder->pos()
                + unit_vec.setLengthVector( holder_move_dist );

            if ( target_point.absX() > max_x
                 || target_point.absY() > max_y )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN,
                              __FILE__": %d: (%.2f %.2f) out of pitch.",
                              generated_count,
                              target_point.x, target_point.y );
#endif
                continue;
            }

            const int holder_reach_step
                = 1 + 1  // kick + turn
                + ptype->cyclesToReachDistance( holder_move_dist - ptype->kickableArea() * 0.5 );

            //
            // check opponent
            //
            bool exist_opponent = false;
            for ( PlayerCont::const_iterator o = state.opponents().begin();
                  o != state.opponents().end();
                  ++o )
            {
                double opp_move_dist = o->pos().dist( target_point );
                int o_step
                    = 1 // turn step
                    + o->playerTypePtr()->cyclesToReachDistance( opp_move_dist - ptype->kickableArea() );

                if ( o_step - bonus_step <= holder_reach_step )
                {
                    exist_opponent = true;
                    break;
                }
            }

            if ( exist_opponent )
            {
#ifdef DEBUG_PRINT
                dlog.addText( Logger::ACTION_CHAIN,
                              __FILE__": %d: (%.2f %.2f) exist opponent.",
                              generated_count, target_point.x, target_point.y );
#endif
                continue;
            }

            const double ball_speed = SP.firstBallSpeed( state.ball().pos().dist( target_point ),
                                                         holder_reach_step );

            PredictState::ConstPtr result_state( new PredictState( state,
                                                                   holder_reach_step,
                                                                   holder->unum(),
                                                                   target_point ) );
            CooperativeAction::Ptr action( new Dribble( holder->unum(),
                                                        target_point,
                                                        ball_speed,
                                                        1,
                                                        1,
                                                        holder_reach_step - 2,
                                                        "actgenDribble" ) );
            ++s_action_count;
            ++generated_count;
            action->setIndex( s_action_count );
            result->push_back( ActionStatePair( action, result_state ) );
        }
    }


#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": Dribble path=%d, holder=%d generated=%d/%d",
                  path.size(),
                  holder->unum(),
                  generated_count, s_action_count );
#endif
}
