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

#include "actgen_shoot.h"

#include "field_analyzer.h"

#include "shoot.h"
#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

//#define DEBUG_PRINT

using namespace rcsc;

static const int VALID_PLAYER_THRESHOLD = 10;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_Shoot::generate( std::vector< ActionStatePair > * result,
                        const PredictState & state,
                        const WorldModel & wm,
                        const std::vector< ActionStatePair > & ) const
{
    const AbstractPlayerObject * holder = state.ballHolder();

    if ( holder->posCount() > VALID_PLAYER_THRESHOLD
         || holder->isGhost() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::RECURSIVE,
                      "shoot: unum %d can't shoot, pos accuracy low",
                      holder->unum() );
#endif
        return;
    }


    AbstractPlayerCont opponents
        = state.getPlayerCont( new OpponentOrUnknownPlayerPredicate( wm ) );

    if ( ! FieldAnalyzer::can_shoot_from( holder->unum() == wm.self().unum(),
                                          holder->pos(),
                                          opponents,
                                          VALID_PLAYER_THRESHOLD ) )
    {
        //
        // failure
        //
#ifdef DEBUG_PRINT
        dlog.addText( Logger::RECURSIVE,
                      "shoot: unum %d can't shoot",
                      holder->unum() );
#endif
        return;
    }


    //
    // success
    //
#ifdef DEBUG_PRINT
    dlog.addText( Logger::RECURSIVE,
                  "shoot: unum %d OK",
                  holder->unum() );
#endif

    const long shoot_spend_time
        = ( holder->pos().dist( ServerParam::i().theirTeamGoalPos() ) / 1.5 );

    PredictState::ConstPtr result_state( new PredictState( state,
                                                           shoot_spend_time,
                                                           ServerParam::i().theirTeamGoalPos() ) );

    CooperativeAction::Ptr action( new Shoot( holder->unum(),
                                              ServerParam::i().theirTeamGoalPos(),
                                              ServerParam::i().ballSpeedMax(),
                                              shoot_spend_time,
                                              1,
                                              "shoot" ) );

    result->push_back( ActionStatePair( action, result_state ) );
}
