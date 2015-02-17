// -*-c++-*-

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

#ifndef BHV_CHAIN_ACTION_H
#define BHV_CHAIN_ACTION_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

#include <vector>

namespace rcsc {
class WorldModel;
}

class ActionStatePair;
class ActionChainGraph;

class Bhv_ChainAction
    : public rcsc::SoccerBehavior {

private:
    const ActionChainGraph & M_chain_graph;

public:
    Bhv_ChainAction( const ActionChainGraph & chain_graph );
    Bhv_ChainAction();

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doTurnToForward( rcsc::PlayerAgent * agent );

    rcsc::Vector2D getKeepBallVel( const rcsc::WorldModel & wm );

};

#endif
