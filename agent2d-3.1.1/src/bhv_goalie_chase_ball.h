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

#ifndef AGENT2D_BHV_GOALIE_CHASE_BALL_H
#define AGENT2D_BHV_GOALIE_CHASE_BALL_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

class Bhv_GoalieChaseBall
    : public rcsc::SoccerBehavior {
private:


public:

    bool execute( rcsc::PlayerAgent * agent );

    static
    bool is_ball_chase_situation( const rcsc::PlayerAgent * agent );
    static
    bool is_ball_shoot_moving( const rcsc::PlayerAgent * agent );

private:
    void doGoToCatchPoint( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & target_point );

};

#endif
