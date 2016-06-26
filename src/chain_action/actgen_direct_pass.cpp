// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "actgen_direct_pass.h"

#include "pass.h"
#include "simple_pass_checker.h"

#include "predict_state.h"
#include "action_state_pair.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <vector>

// #define DEBUG_PRINT

#ifdef DEBUG_PRINT
#include "action_chain_graph.h"
#endif

using namespace rcsc;

static const double SAME_PASSER_POS_THRESHOLD2 = std::pow( 10.0, 2 );

namespace {

/*-------------------------------------------------------------------*/
/*!

 */
double
s_get_ball_speed_for_pass( const double & distance )
{
    if ( distance >= 20.0 )
    {
        //return 3.0;
        return 2.5;
    }
    else if ( distance >= 8.0 )
    {
        //return 2.5;
        return 2.0;
    }
    else if ( distance >= 5.0 )
    {
        return 1.8;
    }
    else
    {
        return 1.5;
    }
}

}


/*-------------------------------------------------------------------*/
/*!

 */
ActGen_DirectPass::ActGen_DirectPass()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
ActGen_DirectPass::~ActGen_DirectPass()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_DirectPass::generate( std::vector< ActionStatePair > * result,
                             const PredictState & state,
                             const WorldModel & current_wm,
                             const std::vector< ActionStatePair > & path ) const
{
    static const int VALID_PLAYER_THRESHOLD = 10;
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


#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "========== (ActGen_DirectPass::generate)" );
#endif

    //
    // check same situation in history
    //
    bool old_holder_table[11];
    for ( int i = 0; i < 11; ++i )
    {
        old_holder_table[i] = false;
    }

#ifdef DEBUG_PRINT
    ActionChainGraph::writeChainInfoLog( "branch cut checking: history = ",
                                         current_wm, path, 0.0 );
#endif

    for ( int c = static_cast< int >( path.size() ) - 1; c >= 0; --c )
    {
        int ball_holder_unum = Unum_Unknown;

        if ( c == 0 )
        {
            int self_min = current_wm.interceptTable()->selfReachCycle();
            int teammate_min = current_wm.interceptTable()->teammateReachCycle();

            if ( teammate_min < self_min )
            {
                const PlayerObject * teammate = current_wm.interceptTable()->fastestTeammate();
                if ( teammate )
                {
                    ball_holder_unum = teammate->unum();
                }
            }
            else
            {
                ball_holder_unum = current_wm.self().unum();
            }
        }
        else
        {
            ball_holder_unum = path[c - 1].state().ballHolder()->unum();
        }


#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "direct: path=%d, ball_holder=%d",
                      c, ball_holder_unum );
#endif

        if ( ball_holder_unum == Unum_Unknown )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "__ unknown number ball holder" );
#endif
            continue;
        }

        old_holder_table[ ball_holder_unum - 1 ] = true;
    }

#ifdef DEBUG_PRINT
    for ( int unum = 1; unum <= 11; ++unum )
    {
        dlog.addText( Logger::ACTION_CHAIN,
                      "direct: num=%d, ignore? = %s",
                      unum,
                      ( old_holder_table[ unum - 1 ] ? "true" : "false" ) );
    }
#endif



    //
    // add pass candidates
    //
    const rcsc::AbstractPlayerObject * holder = state.ballHolder();

    if ( ! holder )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "direct: invalid holder" );
#endif
        return;
    }


    const SimplePassChecker pass_check;
    int generated_count = 0;

    for ( PredictPlayerPtrCont::const_iterator
              it = state.ourPlayers().begin(),
              end = state.ourPlayers().end();
          it != end;
          ++it )
    {
        if ( ! (*it)->isValid() ) continue;

#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "direct: checking to %d", (*it)->unum() );
#endif

        if ( (*it)->unum() != Unum_Unknown
             && old_holder_table[ (*it)->unum() - 1 ] )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "direct: ignored old holder %d",
                          (*it)->unum() );
#endif
            continue;
        }

        if ( (*it)->unum() == state.ballHolderUnum() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "direct: ignored direct pass to self" );
#endif
            continue;
        }

        if ( (*it)->posCount() > VALID_PLAYER_THRESHOLD
             || (*it)->isGhost()
             || (*it)->unum() == Unum_Unknown
             || (*it)->unumCount() > VALID_PLAYER_THRESHOLD
             || (*it)->isTackling() )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "direct: can't pass from %d to %d(%.1f %.1f),"
                          " target accuracy low",
                          holder->unum(),
                          (*it)->unum(), (*it)->pos().x, (*it)->pos().y );
#endif
            continue;
        }


        //
        // check direct pass
        //
        const double dist = ( (*it)->pos() - holder->pos() ).r();

        const double ball_speed = s_get_ball_speed_for_pass( ( (*it)->pos() - holder->pos() ).r() );

        if ( ! pass_check( state, *holder, **it, (**it).pos(), ball_speed ) )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "direct: can't pass from %d to %d",
                          holder->unum(), (*it)->unum() );
#endif
            continue;
        }

        const unsigned long kick_step = 2;

        const unsigned long spend_time
            = calc_length_geom_series( ball_speed,
                                       dist,
                                       ServerParam::i().ballDecay() )
            + kick_step;

        PredictState::ConstPtr result_state( new PredictState( state,
                                                               spend_time,
                                                               (*it)->unum(),
                                                               (*it)->pos() ) );

        CooperativeAction::Ptr action( new Pass( holder->unum(),
                                                 (*it)->unum(),
                                                 (*it)->pos(),
                                                 ball_speed,
                                                 spend_time,
                                                 kick_step,
                                                 false,
                                                 "actgenDirect" ) );
        ++s_action_count;
        ++generated_count;
        action->setIndex( s_action_count );
        result->push_back( ActionStatePair( action, result_state ) );

    }

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  __FILE__": Direct path=%d, generated=%d total=%d",
                  path.size(),
                  generated_count, s_action_count );
#endif

}
