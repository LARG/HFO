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

#ifndef AGENT2D_BHV_GOALIE_BASIC_MOVE_H
#define AGENT2D_BHV_GOALIE_BASIC_MOVE_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

#include <boost/shared_ptr.hpp>

namespace rcsc {
class Formation;
}

class Bhv_GoalieBasicMove
    : public rcsc::SoccerBehavior {
public:

    Bhv_GoalieBasicMove()
      { }

    bool execute( rcsc::PlayerAgent * agent );

private:

    rcsc::Vector2D getTargetPoint( rcsc::PlayerAgent * agent );
    double getBasicDashPower( rcsc::PlayerAgent * agent,
                              const rcsc::Vector2D & move_point );

    bool doPrepareDeepCross( rcsc::PlayerAgent * agent,
                             const rcsc::Vector2D & move_point );
    bool doStopAtMovePoint( rcsc::PlayerAgent * agent,
                            const rcsc::Vector2D & move_point );
    bool doMoveForDangerousState( rcsc::PlayerAgent * agent,
                                  const rcsc::Vector2D & move_point );
    bool doCorrectX( rcsc::PlayerAgent * agent,
                     const rcsc::Vector2D & move_point );
    bool doCorrectBodyDir( rcsc::PlayerAgent * agent,
                           const rcsc::Vector2D & move_point,
                           const bool consider_opp );
    bool doGoToMovePoint( rcsc::PlayerAgent * agent,
                          const rcsc::Vector2D & move_point );


    void doGoToPointLookBall( rcsc::PlayerAgent * agent,
                              const rcsc::Vector2D & target_point,
                              const rcsc::AngleDeg & body_angle,
                              const double & dist_thr,
                              const double & dash_power,
                              const double & back_power_rate = 0.7 );

};

#endif
