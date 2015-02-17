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

#ifndef SAMPLE_COACH_H
#define SAMPLE_COACH_H

#include <rcsc/coach/coach_agent.h>
#include <rcsc/types.h>

#include <vector>


namespace rcsc {
class PlayerType;
}


class SampleCoach
    : public rcsc::CoachAgent {
private:
    typedef std::vector< const rcsc::PlayerType * > PlayerTypePtrCont;


    int M_opponent_player_types[11];

    rcsc::TeamGraphic M_team_graphic;

public:

    SampleCoach();

    virtual
    ~SampleCoach();


protected:

    /*!
      You can override this method.
      But, CoachAgent::initImpl() must be called in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision making
    virtual
    void actionImpl();

    /*!
      this method is automatically called just after receiving server_param message.
     */
    virtual
    void handleServerParam();

    /*!
      this method is automatically called just after receiving player_param message.
     */
    virtual
    void handlePlayerParam();

    /*!
      this method is automatically called just after receiving player_type message.
     */
    virtual
    void handlePlayerType();

private:

    void doSubstitute();

    void doFirstSubstitute();
    void doSubstituteTiredPlayers();

    void substituteTo( const int unum,
                       const int type );

    int getFastestType( PlayerTypePtrCont & candidates );

    void sayPlayerTypes();

    void sendTeamGraphic();

};

#endif
