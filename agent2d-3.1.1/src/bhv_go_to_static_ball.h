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

#ifndef AGENT2D_BHV_GO_TO_STATIC_BALL_H
#define AGENT2D_BHV_GO_TO_STATIC_BALL_H

#include <rcsc/geom/angle_deg.h>
#include <rcsc/player/soccer_action.h>

class Bhv_GoToStaticBall
    : public rcsc::SoccerBehavior {
private:
    const rcsc::AngleDeg M_ball_place_angle; // global angle from self final kick position

public:
    Bhv_GoToStaticBall( const rcsc::AngleDeg & ball_place_angle )
        : M_ball_place_angle( ball_place_angle )
      { }

    bool execute( rcsc::PlayerAgent * agent );
};

#endif
