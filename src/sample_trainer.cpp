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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sample_trainer.h"

#include <rcsc/trainer/trainer_command.h>
#include <rcsc/trainer/trainer_config.h>
#include <rcsc/coach/global_world_model.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/server_param.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>
#include <rcsc/random.h>

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
SampleTrainer::SampleTrainer()
    : TrainerAgent()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
SampleTrainer::~SampleTrainer()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleTrainer::initImpl( CmdLineParser & cmd_parser )
{
    bool result = TrainerAgent::initImpl( cmd_parser );

#if 0
    ParamMap my_params;

    std::string formation_conf;
    my_map.add()
        ( &conf_path, "fconf" )
        ;

    cmd_parser.parse( my_params );
#endif

    if ( cmd_parser.failed() )
    {
        std::cerr << "coach: ***WARNING*** detected unsupported options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

    if ( ! result )
    {
        return false;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.
    //////////////////////////////////////////////////////////////////

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleTrainer::actionImpl()
{
    if ( world().teamNameLeft().empty() )
    {
        doTeamNames();
        return;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.

    //sampleAction();
    //recoverForever();
    //doSubstitute();
    doKeepaway();
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleTrainer::sampleAction()
{
    // sample training to test a ball interception.

#ifdef __APPLE__
    static int s_state = 0;
    static int s_wait_counter = 0;
    static Vector2D s_last_player_move_pos;
#else
    static thread_local int s_state = 0;
    static thread_local int s_wait_counter = 0;
    static thread_local Vector2D s_last_player_move_pos;
#endif

    if ( world().existKickablePlayer() )
    {
        s_state = 1;
    }

    switch ( s_state ) {
    case 0:
        // nothing to do
        break;
    case 1:
        // exist kickable left player

        // recover stamina
        doRecover();
        // move ball to center
        doMoveBall( Vector2D( 0.0, 0.0 ),
                    Vector2D( 0.0, 0.0 ) );
        // change playmode to play_on
        doChangeMode( PM_PlayOn );
        {
            // move player to random point
            UniformReal uni01( 0.0, 1.0 );
            Vector2D move_pos
                = Vector2D::polar2vector( 15.0, //20.0,
                                          AngleDeg( 360.0 * uni01() ) );
            s_last_player_move_pos = move_pos;

            doMovePlayer( config().teamName(),
                          1, // uniform number
                          move_pos,
                          move_pos.th() - 180.0 );
        }
        // change player type
        {
#ifdef __APPLE__
            static int type = 0;
#else
            static thread_local int type = 0;
#endif
            doChangePlayerType( world().teamNameLeft(), 1, type );
            type = ( type + 1 ) % PlayerParam::i().playerTypes();
        }

        doSay( "move player" );
        s_state = 2;
        std::cout << "trainer: actionImpl init episode." << std::endl;
        break;
    case 2:
        ++s_wait_counter;
        if ( s_wait_counter > 3
             && ! world().playersLeft().empty() )
        {
            // add velocity to the ball
            //UniformReal uni_spd( 2.7, 3.0 );
            //UniformReal uni_spd( 2.5, 3.0 );
            UniformReal uni_spd( 2.3, 2.7 );
            //UniformReal uni_ang( -50.0, 50.0 );
            UniformReal uni_ang( -10.0, 10.0 );
            Vector2D velocity
                = Vector2D::polar2vector( uni_spd(),
                                          s_last_player_move_pos.th()
                                          + uni_ang() );
            doMoveBall( Vector2D( 0.0, 0.0 ),
                        velocity );
            s_state = 0;
            s_wait_counter = 0;
            std::cout << "trainer: actionImpl start ball" << std::endl;
        }
        break;

    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleTrainer::recoverForever()
{
    if ( world().playersLeft().empty() )
    {
        return;
    }

    if ( world().time().stopped() == 0
         && world().time().cycle() % 50 == 0 )
    {
        // recover stamina
        doRecover();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleTrainer::doSubstitute()
{
#ifdef __APPLE__
    static bool s_substitute = false;
#else
    static thread_local bool s_substitute = false;
#endif
    if ( ! s_substitute
         && world().time().cycle() == 0
         && world().time().stopped() >= 10 )
    {
        std::cerr << "trainer " << world().time() << " team name = "
                  << world().teamNameLeft()
                  << std::endl;

        if ( ! world().teamNameLeft().empty() )
        {
            UniformSmallInt uni( 0, PlayerParam::i().ptMax() );
            doChangePlayerType( world().teamNameLeft(),
                                1,
                                uni() );

            s_substitute = true;
        }
    }

    if ( world().time().stopped() == 0
         && world().time().cycle() % 100 == 1
         && ! world().teamNameLeft().empty() )
    {
#ifdef __APPLE__
        static int type = 0;
#else
        static thread_local int type = 0;
#endif
        doChangePlayerType( world().teamNameLeft(), 1, type );
        type = ( type + 1 ) % PlayerParam::i().playerTypes();
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleTrainer::doKeepaway()
{
    if ( world().trainingTime() == world().time() )
    {
        std::cerr << "trainer: "
                  << world().time()
                  << " keepaway training time." << std::endl;
    }

}
