// -*-c++-*-

/*!
  \file keepaway_communication.cpp
  \brief communication planner for keepaway mode Source File
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

#include "keepaway_communication.h"

#include "strategy.h"

#include <rcsc/formation/formation.h>
#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/freeform_parser.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
KeepawayCommunication::KeepawayCommunication()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
KeepawayCommunication::~KeepawayCommunication()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
KeepawayCommunication::execute( PlayerAgent * agent )
{
    if ( ! agent->config().useCommunication()
         || agent->world().gameMode().type() != GameMode::PlayOn )
    {
        return false;
    }

    sayPlan( agent );

    attentiontoSomeone( agent );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
KeepawayCommunication::sayPlan( PlayerAgent * )
{
    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
KeepawayCommunication::attentiontoSomeone( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    const PlayerObject * fastest_teammate = wm.interceptTable()->fastestTeammate();
    const int self_min = wm.interceptTable()->selfReachCycle();
    const int mate_min = wm.interceptTable()->teammateReachCycle();
    const int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( fastest_teammate
         && fastest_teammate->unum() != Unum_Unknown )
    {
        if ( mate_min < self_min
             && mate_min <= opp_min + 1
             && mate_min <= 5 + std::min( 4, fastest_teammate->posCount() )
             && wm.ball().inertiaPoint( mate_min ).dist2( agent->effector().queuedNextSelfPos() )
             < std::pow( 35.0, 2 ) )
        {
            // set attention to ball nearest teammate
            agent->debugClient().addMessage( "AttBallOwner%d", fastest_teammate->unum() );
            agent->doAttentionto( wm.ourSide(), fastest_teammate->unum() );
            return;
        }
    }

    const PlayerObject * nearest_teammate = wm.getTeammateNearestToBall( 5 );

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && opp_min <= 3
         && opp_min <= mate_min
         && opp_min <= self_min
         && nearest_teammate->distFromSelf() < 45.0
         && nearest_teammate->distFromBall() < 20.0 )
    {
        agent->debugClient().addMessage( "AttBallNearest(1)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && wm.ball().posCount() >= 3
         && nearest_teammate->distFromBall() < 20.0 )
    {
        agent->debugClient().addMessage( "AttBallNearest(2)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }

    if ( nearest_teammate
         && nearest_teammate->unum() != Unum_Unknown
         && nearest_teammate->distFromSelf() < 45.0
         && nearest_teammate->distFromBall() < 2.0 )
    {
        agent->debugClient().addMessage( "AttBallNearest(3)%d", nearest_teammate->unum() );
        agent->doAttentionto( wm.ourSide(), nearest_teammate->unum() );
        return;
    }

    agent->debugClient().addMessage( "AttOff" );
    agent->doAttentiontoOff();
}
