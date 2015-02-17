// -*-c++-*-

/*!
  \file hold_ball.cpp
  \brief hold ball object Source File.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hold_ball.h"

namespace rcsc {

/*-------------------------------------------------------------------*/
/*!

 */
HoldBall::HoldBall( const int player,
                    const Vector2D & target_point,
                    const int duration_step,
                    const char * description )
    : CooperativeAction( CooperativeAction::Hold,
                         player,
                         target_point,
                         duration_step,
                         description )
{
    setTargetPlayerUnum( player );
    setFirstBallSpeed( 0.0 );
    setKickCount( 1 );
    setTurnCount( 0 );
    setDashCount( 0 );
}

}
