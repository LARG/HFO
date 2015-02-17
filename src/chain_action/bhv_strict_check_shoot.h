// -*-c++-*-

/*!
  \file bhv_strict_check_shoot.h
  \brief strict checked shoot action using ShootGenerator.
*/

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef BHV_STRICT_CHECK_SHOOT_H
#define BHV_STRICT_CHECK_SHOOT_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

/*!
  \class Bhv_StrictCheckShoot
  \brief strict checked shoot action
 */
class Bhv_StrictCheckShoot
    : public rcsc::SoccerBehavior {
private:

public:
    /*!
      \brief accessible from global.
     */
    Bhv_StrictCheckShoot()
      { }

    /*!
      \brief execute action
      \param agent pointer to the agent itself
      \return true if action is performed
     */
    bool execute( rcsc::PlayerAgent * agent );

private:

    bool doTurnNeckToShootPoint( rcsc::PlayerAgent * agent,
                                 const rcsc::Vector2D & shoot_point );
};

#endif
