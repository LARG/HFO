// -*-c++-*-

/*!
  \file soccer_role.cpp
  \brief abstract player role class Header File
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "soccer_role.h"

// #include "role_sample.h"
// #include "role_savior.h"
// #include "role_goalie.h"
// #include "role_center_back.h"
// #include "role_center_forward.h"
// #include "role_defensive_half.h"
// #include "role_offensive_half.h"
// #include "role_side_back.h"
// #include "role_side_forward.h"
// #include "role_sweeper.h"

#include <rcsc/player/world_model.h>
#include <rcsc/game_mode.h>

/*-------------------------------------------------------------------*/
/*!

*/
SoccerRole::Creators &
SoccerRole::creators()
{
    static Creators s_instance;
    return s_instance;
}

/*-------------------------------------------------------------------*/
/*!

*/
SoccerRole::Ptr
SoccerRole::create( const std::string & name )
{
    SoccerRole::Ptr ptr( static_cast< SoccerRole * >( 0 ) );

    Creator creator;
    if ( SoccerRole::creators().getCreator( creator, name ) )
    {
        ptr = creator();
    }
    // else if ( RoleSample::NAME == name ) ptr = RoleSample::create();
    // else if ( RoleGoalie::NAME == name ) ptr = RoleGoalie::create();
    // else if ( RoleSavior::NAME == name ) ptr = RoleSavior::create();
    // else if ( RoleCenterBack::NAME == name ) ptr = RoleCenterBack::create();
    // else if ( RoleCenterForward::NAME == name ) ptr = RoleCenterForward::create();
    // else if ( RoleDefensiveHalf::NAME == name ) ptr = RoleDefensiveHalf::create();
    // else if ( RoleOffensiveHalf::NAME == name ) ptr = RoleOffensiveHalf::create();
    // else if ( RoleSideBack::NAME == name ) ptr = RoleSideBack::create();
    // else if ( RoleSideForward::NAME == name ) ptr = RoleSideForward::create();
    // else if ( RoleSweeper::NAME == name ) ptr = RoleSideSweeper::create();

    return ptr;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
SoccerRole::acceptExecution( const rcsc::WorldModel & wm )
{
    if ( wm.gameMode().type() == rcsc::GameMode::PlayOn )
    {
        return true;
    }

    return false;
}
