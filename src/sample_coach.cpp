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

#include "sample_coach.h"

#include <rcsc/coach/coach_command.h>
#include <rcsc/coach/coach_config.h>
#include <rcsc/coach/coach_debug_client.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <rcsc/coach/global_world_model.h>

#include <cstdio>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <functional>

#include "team_logo.xpm"

using namespace rcsc;


struct RealSpeedMaxCmp
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          if ( std::fabs( lhs->realSpeedMax() - rhs->realSpeedMax() ) < 0.005 )
          {
              return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
          }

          return lhs->realSpeedMax() > rhs->realSpeedMax();
      }

};

/*-------------------------------------------------------------------*/
/*!

*/
SampleCoach::SampleCoach()
    : CoachAgent()
{
    //
    // register audio memory & say message parsers
    //

    boost::shared_ptr< AudioMemory > audio_memory( new AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    addSayMessageParser( SayMessageParser::Ptr( new BallMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new PassMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new InterceptMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new GoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new GoalieAndPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OffsideLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new DefenseLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new WaitRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new PassRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new DribbleMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new BallGoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OnePlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new TwoPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new ThreePlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new SelfMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new TeammateMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OpponentMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new BallPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new StaminaMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new RecoveryMessageParser( audio_memory ) ) );

    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 9 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 8 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 7 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 6 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 5 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 4 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 3 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 2 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 1 >( audio_memory ) ) );

    //
    //
    //

    for ( int i = 0; i < 11; ++i )
    {
        M_opponent_player_types[i] = Hetero_Default;
    }

}

/*-------------------------------------------------------------------*/
/*!

*/
SampleCoach::~SampleCoach()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCoach::initImpl( CmdLineParser & cmd_parser )
{
    bool result =CoachAgent::initImpl( cmd_parser );

#if 0
    ParamMap my_params;
    if ( cmd_parser.count( "help" ) )
    {
       my_params.printHelp( std::cout );
       return false;
    }
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

    if ( config().useTeamGraphic() )
    {
        if ( config().teamGraphicFile().empty() )
        {
            M_team_graphic.createXpmTiles( team_logo_xpm );
        }
        else
        {
            M_team_graphic.readXpmFile( config().teamGraphicFile().c_str() );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::actionImpl()
{
    debugClient().addMessage( "Cycle=%ld", world().time().cycle() );

    if ( world().time().cycle() == 0
         && config().useTeamGraphic()
         && M_team_graphic.tiles().size() != teamGraphicOKSet().size() )
    {
        sendTeamGraphic();
    }

    doSubstitute();
    sayPlayerTypes();
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::handleServerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::handlePlayerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::handlePlayerType()
{

}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doSubstitute()
{
#ifdef __APPLE__
    static bool S_first_substituted = false;
#else
    static thread_local bool S_first_substituted = false;
#endif

    if ( ! S_first_substituted
         && world().time().cycle() == 0
         && world().time().stopped() > 10 )
    {
        doFirstSubstitute();
        S_first_substituted = true;

        return;
    }

    if ( world().time().cycle() > 0
         && world().gameMode().type() != GameMode::PlayOn
         && ! world().gameMode().isPenaltyKickMode() )
    {
        doSubstituteTiredPlayers();

        return;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doFirstSubstitute()
{
    PlayerTypePtrCont candidates;

    std::fprintf( stderr,
                  "id speed step inc  power  stam"
                  //" decay"
                  //" moment"
                  //" dprate"
                  "  karea"
                  //"  krand"
                  //" effmax effmin"
                  "\n" );

    for ( int id = 0; id < PlayerParam::i().playerTypes(); ++id )
    {
        const PlayerType * param = PlayerTypeSet::i().get( id );

        if ( ! param )
        {
            std::cerr << config().teamName() << " coach: "
                      << " could not get the player type " << id << std::endl;
            continue;
        }

        if ( id == Hetero_Default
             && PlayerParam::i().allowMultDefaultType() )
        {
            for ( int i = 0; i <= MAX_PLAYER; ++i )
            {
                candidates.push_back( param );
            }
        }

        for ( int i = 0; i < PlayerParam::i().ptMax(); ++i )
        {
            candidates.push_back( param );
        }

        std::fprintf( stderr,
                      " %d %.3f  %2d  %.1f %5.1f %5.1f"
                      //" %.3f"
                      //"  %4.1f"
                      //"  %.5f"
                      "  %.3f"
                      //"  %.2f"
                      //"  %.3f  %.3f"
                      "\n",
                      id,
                      param->realSpeedMax(),
                      param->cyclesToReachMaxSpeed(),
                      param->staminaIncMax(),
                      param->getDashPowerToKeepMaxSpeed(),
                      param->getOneStepStaminaComsumption(),
                      //param->playerDecay(),
                      //param->inertiaMoment(),
                      //param->dashPowerRate(),
                      param->kickableArea()
                      //param->kickRand(),
                      //param->effortMax(), param->effortMin()
                      );
    }

    std::vector< int > ordered_unum;
    ordered_unum.reserve( 11 );

#if 0
    // side back has priority
    ordered_unum.push_back( 11 ); // center forward
    ordered_unum.push_back( 2 );  // center back
    ordered_unum.push_back( 3 );  // center back
    ordered_unum.push_back( 4 );  // side back
    ordered_unum.push_back( 5 );  // side back
    ordered_unum.push_back( 10 ); // side half
    ordered_unum.push_back( 9 );  // side half
    ordered_unum.push_back( 6 );  // center half
    ordered_unum.push_back( 7 );  // defensive half
    ordered_unum.push_back( 8 );  // defensive half
#else
    // wing player has priority
    ordered_unum.push_back( 11 ); // center forward
    ordered_unum.push_back( 2 );  // center back
    ordered_unum.push_back( 3 );  // center back
    ordered_unum.push_back( 10 ); // side half
    ordered_unum.push_back( 9 );  // side half
    ordered_unum.push_back( 6 );  // center half
    ordered_unum.push_back( 4 );  // side back
    ordered_unum.push_back( 5 );  // side back
    ordered_unum.push_back( 7 );  // defensive half
    ordered_unum.push_back( 8 );  // defensive half
#endif


    //
    // goalie:
    // goalie is always assigned to the default type so far.
    //

    if ( config().version() >= 14.0 )
    {
        substituteTo( 1, Hetero_Default ); // goalie
    }
    {
        PlayerTypePtrCont::iterator it = candidates.begin();
        for ( ; it != candidates.end(); ++it )
        {
            if ( (*it)->id() == Hetero_Default )
            {
                break;
            }
        }

        if ( it != candidates.end() )
        {
            candidates.erase( it );
        }
    }

    //
    // change field players
    //

    for ( std::vector< int >::iterator unum = ordered_unum.begin();
          unum != ordered_unum.end();
          ++unum )
    {
        const GlobalPlayerObject * p = world().teammate( *unum );
        if ( ! p )
        {
            std::cerr << config().teamName() << " coach: "
                      << " teammate " << *unum << " does not exist."
                      << " skip first substitution." << std::endl;
            dlog.addText( Logger::TEAM,
                          __FILE__": teammate %d does not exist. skip first substitution.",
                          *unum );
            continue;
        }

        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            substituteTo( *unum, type );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doSubstituteTiredPlayers()
{
    int substitute_count = world().ourSubstituteCount();

    if ( substitute_count >= PlayerParam::i().subsMax() )
    {
        // over the maximum substitution
        return;
    }

    const ServerParam & SP = ServerParam::i();

    //
    // check game time
    //
    const int half_time = SP.actualHalfTime();
    const int normal_time = half_time * SP.nrNormalHalfs();

    if ( world().time().cycle() < normal_time - 500
         //|| world().time().cycle() <= half_time + 1
         //|| world().gameMode().type() == GameMode::KickOff_
         )
    {
        return;
    }

    dlog.addText( Logger::TEAM,
                  __FILE__": consider to substitute tired teammates." );

    //
    // create candidate teamamte
    //
    std::vector< int > tired_teammate_unum;

    for ( std::vector< const GlobalPlayerObject * >::const_iterator
              t = world().teammates().begin(),
              end = world().teammates().end();
          t != end;
          ++t )
    {
        if ( (*t)->recovery() < ServerParam::i().recoverInit() - 0.002 )
        {
            tired_teammate_unum.push_back( (*t)->unum() );
        }
    }

    if ( tired_teammate_unum.empty() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": no tired teammates." );
        return;
    }

    //
    // create candidate player type
    //
    PlayerTypePtrCont candidates;

    for ( std::vector< int >::const_iterator
              id = world().availablePlayerTypeId().begin(),
              end = world().availablePlayerTypeId().end();
          id != end;
          ++id )
    {
        const PlayerType * param = PlayerTypeSet::i().get( *id );
        if ( ! param )
        {
            std::cerr << config().teamName() << " coach: "
                      << world().time()
                      << " : Could not get player type. id=" << *id << std::endl;
            continue;
        }

        candidates.push_back( param );
    }

    //
    // try substitution
    //


    for ( std::vector< int >::iterator unum = tired_teammate_unum.begin();
          unum != tired_teammate_unum.end();
          ++unum )
    {
        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            substituteTo( *unum, type );
            if ( ++substitute_count >= PlayerParam::i().subsMax() )
            {
                // over the maximum substitution
                break;
            }
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleCoach::substituteTo( const int unum,
                           const int type )
{
    if ( world().time().cycle() > 0
         && world().ourSubstituteCount() >= PlayerParam::i().subsMax() )
    {
        std::cerr << "***Warning*** "
                  << config().teamName() << " coach: over the substitution max."
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    std::vector< int >::const_iterator
        it = std::find( world().availablePlayerTypeId().begin(),
                        world().availablePlayerTypeId().end(),
                        type );
    if ( it == world().availablePlayerTypeId().end() )
    {
        std::cerr << "***ERROR*** "
                  << config().teamName() << " coach: "
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    doChangePlayerType( unum, type );

    std::cout << config().teamName() << " coach: "
              << "change player " << unum
              << " to type " << type
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
SampleCoach::getFastestType( PlayerTypePtrCont & candidates )
{
    if ( candidates.empty() )
    {
        return Hetero_Unknown;
    }

    // sort by max speed
    std::sort( candidates.begin(),
               candidates.end(),
               RealSpeedMaxCmp() );

//     std::cerr << "getFastestType candidate = ";
//     for ( PlayerTypePtrCont::iterator it = candidates.begin();
//           it != candidates.end();
//           ++it )
//     {
//         std::cerr << (*it)->id() << ' ';
//     }
//     std::cerr << std::endl;

    PlayerTypePtrCont::iterator best_type = candidates.end();
    double max_speed = 0.0;
    int min_cycle = 100;
    for ( PlayerTypePtrCont::iterator it = candidates.begin();
          it != candidates.end();
          ++it )
    {
        if ( (*it)->realSpeedMax() < max_speed - 0.01 )
        {
            break;
        }

        if ( (*it)->cyclesToReachMaxSpeed() < min_cycle )
        {
            best_type = it;
            max_speed = (*best_type)->realSpeedMax();
            min_cycle = (*best_type)->cyclesToReachMaxSpeed();
            continue;
        }

        if ( (*it)->cyclesToReachMaxSpeed() == min_cycle )
        {
            if ( (*it)->getOneStepStaminaComsumption()
                 < (*best_type)->getOneStepStaminaComsumption() )
            {
                best_type = it;
                max_speed = (*best_type)->realSpeedMax();
            }
        }
    }

    if ( best_type != candidates.end() )
    {
        int id = (*best_type)->id();
        candidates.erase( best_type );
        return id;
    }

    return Hetero_Unknown;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::sayPlayerTypes()
{
    /*
      format:
      "(player_types (1 0) (2 1) (3 2) (4 3) (5 4) (6 5) (7 6) (8 -1) (9 0) (10 1) (11 2))"
      ->
      (say (freeform "(player_type ...)"))
    */

#ifdef __APPLE__
    static GameTime s_last_send_time( 0, 0 );
#else
    static thread_local GameTime s_last_send_time( 0, 0 );
#endif

    if ( ! config().useFreeform() )
    {
        return;
    }

    if ( ! world().canSendFreeform() )
    {
        return;
    }

    int analyzed_count = 0;

    for ( int unum = 1; unum <= 11; ++unum )
    {
        const int id = world().theirPlayerTypeId( unum );

        if ( id != M_opponent_player_types[unum - 1] )
        {
            M_opponent_player_types[unum - 1] = id;

            if ( id != Hetero_Unknown )
            {
                ++analyzed_count;
            }
        }
    }

    if ( analyzed_count == 0 )
    {
        return;
    }

    std::string msg;
    msg.reserve( 128 );

    msg = "(player_types ";

    for ( int unum = 1; unum <= 11; ++unum )
    {
        char buf[8];
        snprintf( buf, 8, "(%d %d)",
                  unum, M_opponent_player_types[unum - 1] );
        msg += buf;
    }

    msg += ")";

    doSayFreeform( msg );

    s_last_send_time = world().time();

    std::cout << config().teamName()
              << " coach: "
              << world().time()
              << " sent freeform " << msg
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::sendTeamGraphic()
{
    int count = 0;
    for ( TeamGraphic::Map::const_reverse_iterator tile = M_team_graphic.tiles().rbegin();
          tile != M_team_graphic.tiles().rend();
          ++tile )
    {
        if ( teamGraphicOKSet().find( tile->first ) == teamGraphicOKSet().end() )
        {
            if ( ! doTeamGraphic( tile->first.first,
                                  tile->first.second,
                                  M_team_graphic ) )
            {
                break;
            }
            ++count;
        }
    }

    if ( count > 0 )
    {
        std::cout << config().teamName()
                  << " coach: "
                  << world().time()
                  << " send team_graphic " << count << " tiles"
                  << std::endl;
    }
}
