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

#include "intention_wait_after_set_play_kick.h"


#include <rcsc/action/bhv_neck_body_to_ball.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
IntentionWaitAfterSetPlayKick::IntentionWaitAfterSetPlayKick()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionWaitAfterSetPlayKick::finished( const PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    if ( wm.existKickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. exist kickable opponent" );
        return true;
    }

    if ( ! wm.self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. no kickable" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionWaitAfterSetPlayKick::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": (execute) wait after set play kick" );

    agent->debugClient().addMessage( "Intention:Wait" );

    Bhv_NeckBodyToBall().execute( agent );

    return true;
}
