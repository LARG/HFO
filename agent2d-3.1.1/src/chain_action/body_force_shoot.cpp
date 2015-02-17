// -*-c++-*-

/*!
  \file body_force_shoot.cpp
  \brief kick ball to goal by force
*/

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

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "body_force_shoot.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>

#include <rcsc/action/bhv_shoot.h>
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/body_kick_one_step.h>

/*-------------------------------------------------------------------*/
/*!

*/
Body_ForceShoot::Body_ForceShoot()
{
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Body_ForceShoot::execute( rcsc::PlayerAgent * agent )
{
    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": execute()" );
    agent->debugClient().addMessage( "ForceShoot" );

    if ( rcsc::Bhv_Shoot().execute( agent ) )
    {
        rcsc::dlog.addText( rcsc::Logger::ACTION,
                            __FILE__": ForceShoot -> Body_Shoot2008" );

        return true;
    }

    const rcsc::ServerParam & param = rcsc::ServerParam::i();

    const rcsc::Vector2D target = param.theirTeamGoalPos();

    rcsc::dlog.addText( rcsc::Logger::ACTION,
                        __FILE__": no course, force kick" );
    agent->debugClient().setTarget( target );

    if ( agent->world().existKickableOpponent() )
    {
        if ( rcsc::Body_KickOneStep( target, param.ballSpeedMax() )
             .execute( agent ) )
        {
            return true;
        }
    }

    if ( rcsc::Body_SmartKick( target,
                               param.ballSpeedMax(),
                               param.ballSpeedMax() * 0.96,
                               3 ).execute( agent ) )
    {
        return true;
    }

    if ( rcsc::Body_KickOneStep( target, param.ballSpeedMax() )
         .execute( agent ) )
    {
        return true;
    }

    return true;
}
