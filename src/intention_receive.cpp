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

#include "intention_receive.h"

#include <rcsc/action/body_intercept.h>

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/debug_client.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "neck_default_intercept_neck.h"
#include "neck_offensive_intercept_neck.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
IntentionReceive::IntentionReceive( const Vector2D & target_point,
                                    const double & dash_power,
                                    const double & buf,
                                    const int max_step,
                                    const GameTime & start_time )
    : M_target_point( target_point )
    , M_dash_power( dash_power )
    , M_buffer( buf )
    , M_step( max_step )
    , M_last_execute_time( start_time )
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionReceive::finished( const PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. time 0" );
        return true;
    }

    if ( agent->world().self().isKickable() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": already kickable" );
        return true;
    }

    if ( agent->world().existKickableTeammate() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": exist kickable teammate" );
        return true;
    }

    if ( agent->world().ball().distFromSelf() < 3.0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. ball very near" );
        return true;
    }

    if ( M_last_execute_time.cycle() < agent->world().time().cycle() - 1 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. strange time." );
        return true;
    }

    if ( agent->world().self().pos().dist( M_target_point ) < M_buffer )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": finished. already there." );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
IntentionReceive::execute( PlayerAgent * agent )
{
    if ( M_step <= 0 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. empty intention." );
        return false;
    }

    const WorldModel & wm = agent->world();

    M_step -= 1;
    M_last_execute_time = wm.time();

    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addMessage( "IntentionRecv" );

    dlog.addText( Logger::TEAM,
                  __FILE__": execute. try to receive" );

    int self_min = wm.interceptTable()->selfReachCycle();
    int opp_min = wm.interceptTable()->opponentReachCycle();

    if ( self_min < 6 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. point very near. intercept" );
        agent->debugClient().addMessage( "IntentionRecv:Intercept" );
        Body_Intercept().execute( agent );

        if ( opp_min >= self_min + 3 )
        {
            agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        }
        else
        {
            agent->setNeckAction( new Neck_DefaultInterceptNeck
                                  ( new Neck_TurnToBallOrScan() ) );
        }

        return true;
    }

    if ( Body_Intercept().execute( agent ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": execute. intercept cycle=%d",
                      self_min );
        agent->debugClient().addMessage( "IntentionRecv%d:Intercept", M_step );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );
        return true;
    }


    dlog.addText( Logger::TEAM,
                  __FILE__": execute. intercept cycle=%d. go to receive point",
                  self_min );
    agent->debugClient().addMessage( "IntentionRecv%d:GoTo", M_step );
    agent->debugClient().setTarget( M_target_point );
    agent->debugClient().addCircle( M_target_point, M_buffer );

    Body_GoToPoint( M_target_point,
                    M_buffer,
                    M_dash_power
                    ).execute( agent );
    agent->setNeckAction( new Neck_TurnToBallOrScan() );

    return true;
}
