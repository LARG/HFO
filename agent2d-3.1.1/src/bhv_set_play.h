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

#ifndef BHV_SET_PLAY_H
#define BHV_SET_PLAY_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

namespace rcsc {
class WorldModel;
}

class Bhv_SetPlay
    : public rcsc::SoccerBehavior {
public:
    Bhv_SetPlay()
      { }

    bool execute( rcsc::PlayerAgent * agent );

    static
    rcsc::Vector2D get_avoid_circle_point( const rcsc::WorldModel & world,
                                           const rcsc::Vector2D & target_point );

    static
    double get_set_play_dash_power( const rcsc::PlayerAgent * agent );

    static
    bool is_kicker( const rcsc::PlayerAgent * agent );

    static
    bool is_delaying_tactics_situation( const rcsc::PlayerAgent * agent );

private:
    void doBasicTheirSetPlayMove( rcsc::PlayerAgent * agent );

};

#endif
