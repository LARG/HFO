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

#include "neck_turn_to_receiver.h"

#include "action_chain_holder.h"
#include "action_chain_graph.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/neck_turn_to_low_conf_teammate.h>

#include <rcsc/player/player_agent.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
Neck_TurnToReceiver::Neck_TurnToReceiver( const ActionChainGraph & chain_graph )
    : M_chain_graph( chain_graph )
{
}


/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_TurnToReceiver::execute( PlayerAgent * agent )
{
    if ( agent->effector().queuedNextBallKickable() )
    {
        if ( executeImpl( agent ) )
        {

        }
        else if ( agent->world().self().pos().x > 35.0
                  || agent->world().self().pos().absY() > 20.0 )
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": Neck_TurnToReceiver. next kickable."
                          " attack or side area. scan field" );
            Neck_ScanField().execute( agent );
        }
        else
        {
            dlog.addText( Logger::TEAM,
                          __FILE__": Neck_TurnToReceiver. next kickable. look low conf teammate" );
            Neck_TurnToLowConfTeammate().execute( agent );
        }
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_TurnToReceiver. look ball" );
        Neck_TurnToBall().execute( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Neck_TurnToReceiver::executeImpl( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": Neck_TurnToReceiver (executeImpl)" );

    const WorldModel & wm = agent->world();

    const CooperativeAction & pass = M_chain_graph.getFirstAction();
    const AbstractPlayerObject * receiver = wm.ourPlayer( pass.targetPlayerUnum() );

    // if ( receiver->posCount() == 0 )
    // {
    //     dlog.addText( Logger::TEAM,
    //                         __FILE__": Neck_TurnToReceiver. current seen." );
    //     return false;
    // }

    if ( receiver->unum() == wm.self().unum() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": Neck_TurnToReceiver. self pass." );
        return false;
    }

    const Vector2D next_self_pos = agent->effector().queuedNextMyPos();
    //const AngleDeg next_self_body = agent->effector().queuedNextMyBody();
    const double next_view_width = agent->effector().queuedNextViewWidth().width() * 0.5;

    const Vector2D receiver_pos = receiver->pos() + receiver->vel();
    const AngleDeg receiver_angle = ( receiver_pos - next_self_pos ).th();

    Vector2D face_point = ( receiver_pos + pass.targetPoint() ) * 0.5;
    AngleDeg face_angle = ( face_point - next_self_pos ).th();

    double rate = 0.5;
    while ( ( face_angle - receiver_angle ).abs() > next_view_width - 10.0 )
    {
        rate += 0.1;
        face_point
            = receiver_pos * rate
            + pass.targetPoint() * ( 1.0 - rate );
        face_angle = ( face_point - next_self_pos ).th();

        if ( rate > 0.999 )
        {
            break;
        }
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": facePoint=(%.1f %.1f) faceAngle=%.1f",
                  face_point.x, face_point.y,
                  face_angle.degree() );
    agent->debugClient().addMessage( "NeckToReceiver%.0f",
                                     face_angle.degree() );
    Neck_TurnToPoint( face_point ).execute( agent );

    return true;
}
