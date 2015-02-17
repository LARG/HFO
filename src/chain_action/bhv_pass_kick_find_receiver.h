// -*-c++-*-

/*!
  \file bhv_pass_kick_find_receiver.h
  \brief search the pass receiver player and perform pass kick if possible
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

#ifndef BHV_PASS_KICK_FIND_RECEIVER_H
#define BHV_PASS_KICK_FIND_RECEIVER_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

namespace rcsc {
class WorldModel;
}

class ActionChainGraph;
class CooperativeAction;

/*!
  \class Bhv_PassKickFindReceiver
  \brief search specified player and perform pass kick if possible
*/
class Bhv_PassKickFindReceiver
    : public rcsc::SoccerBehavior {

private:
    const ActionChainGraph & M_chain_graph;

public:

    Bhv_PassKickFindReceiver( const ActionChainGraph & chain_graph );

    bool execute( rcsc::PlayerAgent * agent );

private:

    int kickStep( const rcsc::WorldModel & wm,
                  const CooperativeAction & pass );

    bool doPassKick( rcsc::PlayerAgent * agent,
                     const CooperativeAction & pass );

    bool doCheckReceiver( rcsc::PlayerAgent * agent,
                          const CooperativeAction & pass );

    bool doKeepBall( rcsc::PlayerAgent * agent,
                     const CooperativeAction & pass );

    rcsc::Vector2D getKeepBallVel( const rcsc::WorldModel & wm );

    bool doTurnBodyNeckToReceiver( rcsc::PlayerAgent * agent,
                                   const CooperativeAction & pass );

    void doSayPass( rcsc::PlayerAgent * agent,
                    const CooperativeAction & pass );

};

#endif
