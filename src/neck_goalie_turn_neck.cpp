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

#include "neck_goalie_turn_neck.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_GoalieTurnNeck::execute( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Neck_GoalieTurnNeck" );

    if ( agent->world().ball().pos().x > 0.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": ball is their side" );
        return Neck_TurnToPoint( Vector2D(0.0, 0.0)
                                 ).execute( agent );
    }

    if ( agent->world().ball().posCount() == 0
         && agent->world().ball().pos().absY() > 15.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": field scan" );
        return Neck_ScanField().execute( agent );
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": neck to ball" );
    return Neck_TurnToBall().execute( agent );
}
