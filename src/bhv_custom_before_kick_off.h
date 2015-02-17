// -*-c++-*-

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

#ifndef BHV_CUSTOM_BEFORE_KICK_OFF_H
#define BHV_CUSTOM_BEFORE_KICK_OFF_H

#include <rcsc/player/soccer_action.h>
#include <rcsc/geom/vector_2d.h>

/*!
  \class Bhv_CustomBeforeKickOff
  \brief custom action for before_kick_off
 */
class Bhv_CustomBeforeKickOff
    : public rcsc::SoccerBehavior {
private:
    //! target position for move command
    rcsc::Vector2D M_move_point;
public:
    /*!
      \brief construct with target position
      \param point target position for move command
     */
    explicit
    Bhv_CustomBeforeKickOff( const rcsc::Vector2D & point )
        : M_move_point( point )
      { }

    /*!
      \brief execute action
      \param agent pointer to the agent itself
      \return true if action is performed
     */
    bool execute( rcsc::PlayerAgent * agent );
};

#endif
