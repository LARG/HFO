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

#include "role_keepaway_taker.h"


#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_clear_ball.h>

#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>

using namespace rcsc;

const std::string RoleKeepawayTaker::NAME( "KeepawayTaker" );

/*-------------------------------------------------------------------*/
/*!

 */
namespace {
rcss::RegHolder role = SoccerRole::creators().autoReg( &RoleKeepawayTaker::create,
                                                       RoleKeepawayTaker::NAME );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
RoleKeepawayTaker::execute( PlayerAgent * agent )
{

    //////////////////////////////////////////////////////////////
    // play_on play
    if ( agent->world().self().isKickable() )
    {
        // kick
        doKick( agent );
    }
    else
    {
        // positioning
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayTaker::doKick( PlayerAgent * agent )
{
    Body_ClearBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
RoleKeepawayTaker::doMove( PlayerAgent * agent )
{
    Body_Intercept().execute( agent );
    agent->setNeckAction( new Neck_TurnToBall() );
}
