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

#include "actgen_strict_check_pass.h"

#include "strict_check_pass_generator.h"

#include "action_state_pair.h"
#include "predict_state.h"

#include <rcsc/common/logger.h>

//#define DEBUG_PRINT

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
void
ActGen_StrictCheckPass::generate( std::vector< ActionStatePair > * result,
                                  const PredictState & state,
                                  const WorldModel & wm,
                                  const std::vector< ActionStatePair > & path ) const
{
    // generate only first actions
    if ( ! path.empty() )
    {
        return;
    }

    const std::vector< CooperativeAction::Ptr > &
        courses = StrictCheckPassGenerator::instance().courses( wm );

    //
    // add pass course candidates
    //
    const std::vector< CooperativeAction::Ptr >::const_iterator end = courses.end();
    for ( std::vector< CooperativeAction::Ptr >::const_iterator act = courses.begin();
          act != end;
          ++act )
    {
        if ( (*act)->targetPlayerUnum() == Unum_Unknown )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": can't pass from %d,"
                          " target invalid",
                          state.ballHolder()->unum() );
#endif
            continue;
        }

        const AbstractPlayerObject * target_player = state.ourPlayer( (*act)->targetPlayerUnum() );

        if ( ! target_player )
        {
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          __FILE__": can't pass from %d to %d, NULL target player",
                          state.ballHolder()->unum(),
                          (*act)->targetPlayerUnum() );
#endif
            continue;
        }

        result->push_back( ActionStatePair( *act,
                                            new PredictState( state,
                                                              (*act)->durationStep(),
                                                              (*act)->targetPlayerUnum(),
                                                              (*act)->targetPoint() ) ) );
    }
}
