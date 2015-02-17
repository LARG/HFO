// -*-c++-*-

/*!
  \file neck_offensive_intercept_neck.h
  \brief default neck action to use with intercept action
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

#ifndef NECK_OFFENSIVE_INTERCEPT_NECK_H
#define NECK_OFFENSIVE_INTERCEPT_NECK_H

#include <rcsc/player/soccer_action.h>

namespace rcsc {
class PlayerAgent;
class ViewAction;
}

/*!
  \class Neck_OffensiveInterceptNeck
  \brief neck action for an offensive intercept situation
*/
class Neck_OffensiveInterceptNeck
    : public rcsc::NeckAction {
public:
    /*!
      \brief constructor
     */
    Neck_OffensiveInterceptNeck()
      { }

    /*!
      \brief default neck action to use with intercept action
      \param agent pointer to the agent itself
      \return true with action, false if not performed
     */
    bool execute( rcsc::PlayerAgent * agent );

    /*!
      \brief create copy of this object
      \return copy of this object
    */
    rcsc::NeckAction * clone() const
      {
          return new Neck_OffensiveInterceptNeck();
      }

private:

    bool doTurnNeckToShootPoint( rcsc::PlayerAgent * agent );
};

#endif
