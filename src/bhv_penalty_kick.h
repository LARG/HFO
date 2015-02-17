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

#ifndef TOKYOTECH_BHV_PENALTY_KICK_H
#define TOKYOTECH_BHV_PENALTY_KICK_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

class Bhv_PenaltyKick
    : public rcsc::SoccerBehavior {
private:

public:

    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doKickerWait( rcsc::PlayerAgent * agent );
    bool doKickerSetup( rcsc::PlayerAgent * agent );
    bool doKickerReady( rcsc::PlayerAgent * agent );
    bool doKicker( rcsc::PlayerAgent * agent );

    // used only for one kick PK
    bool doOneKickShoot( rcsc::PlayerAgent * agent );
    // used only for multi kick PK
    bool doShoot( rcsc::PlayerAgent * agent );
    bool getShootTarget( const rcsc::PlayerAgent * agent,
                         rcsc::Vector2D * point,
                         double * first_speed );
    bool doDribble( rcsc::PlayerAgent * agent );


    bool doGoalieWait( rcsc::PlayerAgent * agent );
    bool doGoalieSetup( rcsc::PlayerAgent * agent );
    bool doGoalie( rcsc::PlayerAgent * agent );

    bool doGoalieBasicMove( rcsc::PlayerAgent * agent );
    rcsc::Vector2D getGoalieMovePos( const rcsc::Vector2D & ball_pos,
                                     const rcsc::Vector2D & my_pos );
    bool isGoalieBallChaseSituation();
    bool doGoalieSlideChase( rcsc::PlayerAgent * agent );
};

#endif
