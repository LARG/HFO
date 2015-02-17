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

#ifndef VIEW_TACTICAL_H
#define VIEW_TACTICAL_H

#include <rcsc/player/soccer_action.h>

class View_Tactical
    : public rcsc::ViewAction {
private:

public:

    bool execute( rcsc::PlayerAgent * agent );


    ViewAction * clone() const
      {
          return new View_Tactical;
      }

private:

    bool doDefault( rcsc::PlayerAgent * agent );
    bool doOurGoalieFreeKick( rcsc::PlayerAgent * agent );

};

#endif
