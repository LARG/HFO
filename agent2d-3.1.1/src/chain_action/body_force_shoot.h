// -*-c++-*-

/*!
  \file body_force_shoot.h
  \brief kick ball to goal by force
*/

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

#ifndef BODY_FORCE_SHOOT_H
#define BODY_FORCE_SHOOT_H

#include <rcsc/player/soccer_action.h>

/*!
  \class Body_ForceShoot
  \brief kick ball to goal by force
*/
class Body_ForceShoot
    : public rcsc::BodyAction {

public:
    /*!
      \brief constructor
     */
    Body_ForceShoot();

    /*!
      \brief do shoot
      \param agent agent pointer to the agent itself
      \return always true
     */
    bool execute( rcsc::PlayerAgent * agent );
};

#endif
