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

#include "bhv_prepare_set_play_kick.h"

#include "bhv_go_to_static_ball.h"
#include "bhv_set_play.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/body_go_to_point.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_PrepareSetPlayKick::execute( PlayerAgent * agent )
{
#ifdef __APPLE__
    static int s_rest_wait_cycle = -1;
#else
    static thread_local int s_rest_wait_cycle = -1;
#endif

    // not reach the ball side
    if ( Bhv_GoToStaticBall( M_ball_place_angle ).execute( agent ) )
    {
        return true;
    }

    // reach to ball side

    if ( s_rest_wait_cycle < 0 )
    {
        s_rest_wait_cycle = M_wait_cycle;
    }

    if ( s_rest_wait_cycle == 0 )
    {
        if ( agent->world().self().stamina() < ServerParam::i().staminaMax() * 0.9
             || agent->world().seeTime() != agent->world().time() )
        {
            s_rest_wait_cycle = 1;
        }
    }

    if ( s_rest_wait_cycle > 0 )
    {
        if ( agent->world().gameMode().type() == GameMode::KickOff_ )
        {
            AngleDeg moment( ServerParam::i().visibleAngle() );
            agent->doTurn( moment );
        }
        else
        {
            Body_TurnToBall().execute( agent );
        }

        agent->setNeckAction( new Neck_ScanField() );
        s_rest_wait_cycle--;

        dlog.addText( Logger::TEAM,
                      __FILE__": wait. rest cycles=%d",
                      s_rest_wait_cycle );
        return true;
    }

    s_rest_wait_cycle = -1;

    return false;
}
