// -*-c++-*-

/*!
  \file keepaway_communication.h
  \brief communication planner for keepaway mode Header File
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

#ifndef KEEPAWAY_COMMUNICATION_H
#define KEEPAWAY_COMMUNICATION_H

#include "communication.h"

#include <rcsc/game_time.h>
#include <rcsc/types.h>

namespace rcsc {
class WorldModel;
}

class KeepawayCommunication
    : public Communication {
public:

    KeepawayCommunication();
    ~KeepawayCommunication();

    virtual
    bool execute( rcsc::PlayerAgent * agent );

private:

    bool sayPlan( rcsc::PlayerAgent * agent );

    void attentiontoSomeone( rcsc::PlayerAgent * agent );
};

#endif
