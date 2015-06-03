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
#include <config.h>
#endif

#include "agent.h"

#include "strategy.h"
#include "field_analyzer.h"

#include "action_chain_holder.h"
#include "sample_field_evaluator.h"

#include "soccer_role.h"

#include "sample_communication.h"
#include "keepaway_communication.h"

#include "bhv_chain_action.h"
#include "bhv_penalty_kick.h"
#include "bhv_set_play.h"
#include "bhv_set_play_kick_in.h"
#include "bhv_set_play_indirect_free_kick.h"

#include "bhv_custom_before_kick_off.h"
#include "bhv_strict_check_shoot.h"
#include "bhv_basic_move.h"

#include "view_tactical.h"

#include "intention_receive.h"
#include "lowlevel_feature_extractor.h"
#include "highlevel_feature_extractor.h"

#include "actgen_cross.h"
#include "actgen_direct_pass.h"
#include "actgen_self_pass.h"
#include "actgen_strict_check_pass.h"
#include "actgen_short_dribble.h"
#include "actgen_simple_dribble.h"
#include "actgen_shoot.h"
#include "actgen_action_chain_length_filter.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/bhv_emergency.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_intercept.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/body_dribble.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/formation/formation.h>
#include <rcsc/action/kick_table.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/audio_sensor.h>
#include <rcsc/player/freeform_parser.h>

#include <rcsc/common/basic_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
// #include <rcsc/common/free_message_parser.h>

#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace rcsc;

// Debugging tools to check for proper feature normalization
float min_feat_val = 1e8;
float max_feat_val = -1e8;

// Socket Error
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

// Minimium and feature values
#define FEAT_MIN -1.
#define FEAT_MAX 1.

#define LOG_FEATURE(val) \
  if (val <= min_feat_val) \
    min_feat_val = val; \
  if (val >= max_feat_val) \
    max_feat_val = val; \
  std::cout << "FEATURE " << val << " [" << min_feat_val << ", " << max_feat_val << "]" << std::endl;

Agent::Agent()
    : PlayerAgent(),
      M_communication(),
      M_field_evaluator(createFieldEvaluator()),
      M_action_generator(createActionGenerator()),
      lastTrainerMessageTime(-1),
      server_port(6008),
      server_running(false),
      num_teammates(-1),
      num_opponents(-1),
      playing_offense(false)
{
    boost::shared_ptr< AudioMemory > audio_memory( new AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    // set communication message parser
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

    // set freeform message parser
    setFreeformParser(FreeformParser::Ptr(new FreeformParser(M_worldmodel)));

    // set action generators
    // M_action_generators.push_back( ActionGenerator::Ptr( new PassGenerator() ) );

    // set communication planner
    M_communication = Communication::Ptr(new SampleCommunication());
}

Agent::~Agent() {
  delete feature_extractor;
  std::cout << "[Agent Server] Closing Server." << std::endl;
  close(newsockfd);
  close(sockfd);
}

bool Agent::initImpl(CmdLineParser & cmd_parser) {
    bool result = PlayerAgent::initImpl(cmd_parser);

    // read additional options
    result &= Strategy::instance().init(cmd_parser);

    rcsc::ParamMap my_params("Additional options");
    my_params.add()
        ("numTeammates", "", &num_teammates)
        ("numOpponents", "", &num_opponents)
        ("playingOffense", "", &playing_offense)
        ("serverPort", "", &server_port);
    cmd_parser.parse(my_params);
    if (cmd_parser.count("help") > 0) {
        my_params.printHelp(std::cout);
        return false;
    }
    if (cmd_parser.failed()) {
        std::cerr << "player: ***WARNING*** detected unsuppprted options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

#ifdef ELOG
#else
    const std::list<std::string>& args = cmd_parser.args();
    if (std::find(args.begin(), args.end(), "--record") != args.end()) {
      std::cerr
          << "[Agent Client] ERROR: Action recording requested but no supported."
          << " To enable action recording, install https://github.com/mhauskn/librcsc"
          << " and recompile with -DELOG. See CMakeLists.txt"
          << std::endl;
      return false;
    }
#endif

    if (!result) {
        return false;
    }

    if (!Strategy::instance().read(config().configDir())) {
        std::cerr << "***ERROR*** Failed to read team strategy." << std::endl;
        return false;
    }

    if (KickTable::instance().read(config().configDir() + "/kick-table")) {
        std::cerr << "Loaded the kick table: ["
                  << config().configDir() << "/kick-table]"
                  << std::endl;
    }

    assert(num_teammates >= 0);
    assert(num_opponents >= 0);
    return true;
}

void Agent::startServer(int server_port) {
  std::cout << "Starting Server on Port " << server_port << std::endl;
  struct sockaddr_in serv_addr, cli_addr;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("[Agent Server] ERROR opening socket");
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(server_port);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    error("[Agent Server] ERROR on binding");
  }
  listen(sockfd, 5);
  socklen_t clilen = sizeof(cli_addr);
  std::cout << "[Agent Server] Waiting for client to connect... " << std::endl;
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0) {
    error("[Agent Server] ERROR on accept");
  }
  std::cout << "[Agent Server] Connected" << std::endl;
  server_running = true;
}

void Agent::clientHandshake() {
  // Send float 123.2345
  float f = 123.2345;
  if (send(newsockfd, &f, sizeof(float), 0) < 0) {
    error("[Agent Server] ERROR sending from socket");
  }
  // Recieve float 5432.321
  if (recv(newsockfd, &f, sizeof(float), 0) < 0) {
    error("[Agent Server] ERROR recv from socket");
  }
  // Check that error is within bounds
  if (abs(f - 5432.321) > 1e-4) {
    error("[Agent Server] Handshake failed. Improper float recieved.");
  }
  // Recieve the feature set to use
  feature_set_t feature_set;
  if (recv(newsockfd, &feature_set, sizeof(int), 0) < 0) {
    error("[Agent Server] ERROR recv from socket");
  }
  // Create the corresponding FeatureExtractor
  feature_extractor = getFeatureExtractor(feature_set);
  // Send the number of features
  int numFeatures = feature_extractor->getNumFeatures();
  assert(numFeatures > 0);
  if (send(newsockfd, &numFeatures, sizeof(int), 0) < 0) {
    error("[Agent Server] ERROR sending from socket");
  }
  // Check that client has recieved correctly
  int client_response = -1;
  if (recv(newsockfd, &client_response, sizeof(int), 0) < 0) {
    error("[Agent Server] ERROR recv from socket");
  }
  if (client_response != numFeatures) {
    error("[Agent Server] Client incorrectly parsed the number of features.");
  }
  std::cout << "[Agent Server] Handshake complete" << std::endl;
}

FeatureExtractor* Agent::getFeatureExtractor(feature_set_t feature_set_indx) {
  if (feature_extractor != NULL) {
    delete feature_extractor;
  }

  switch (feature_set_indx) {
    case LOW_LEVEL_FEATURE_SET:
      return new LowLevelFeatureExtractor(num_teammates, num_opponents,
                                          playing_offense);
      break;
    case HIGH_LEVEL_FEATURE_SET:
      return new HighLevelFeatureExtractor(num_teammates, num_opponents,
                                           playing_offense);
      break;
    default:
      std::cerr << "[Feature Extractor] ERROR Unrecognized Feature set index: "
                << feature_set_indx << std::endl;
      exit(1);
  }
}

hfo_status_t Agent::getGameStatus() {
  hfo_status_t game_status = IN_GAME;
  if (audioSensor().trainerMessageTime().cycle() > lastTrainerMessageTime) {
    lastTrainerMessageTime = audioSensor().trainerMessageTime().cycle();
    const std::string& message = audioSensor().trainerMessage();
    if (message.compare("GOAL") == 0) {
      game_status = GOAL;
    } else if (message.compare("CAPTURED_BY_DEFENSE") == 0) {
      game_status = CAPTURED_BY_DEFENSE;
    } else if (message.compare("OUT_OF_BOUNDS") == 0) {
      game_status = OUT_OF_BOUNDS;
    } else if (message.compare("OUT_OF_TIME") == 0) {
      game_status = OUT_OF_TIME;
    } else {
      std::cout << "[Agent Server] Unrecognized Trainer Message: " << message
                << std::endl;
    }
  }
  return game_status;
}

/*!
  main decision
  virtual method in super class
*/
void Agent::actionImpl() {
  if (!server_running) {
    startServer(server_port);
    clientHandshake();
  }

  // Update and send the game status
  hfo_status_t game_status = getGameStatus();
  if (send(newsockfd, &game_status, sizeof(int), 0) < 0) {
    error("[Agent Server] ERROR sending from socket");
  }

  // Update and send the state features
  const std::vector<float>& features =
      feature_extractor->ExtractFeatures(this->world());

#ifdef ELOG
  if (config().record()) {
    elog.addText(Logger::WORLD, "GameStatus %d", game_status);
    elog.flush();
    feature_extractor->LogFeatures();
  }
#endif

  if (send(newsockfd, &(features.front()),
           features.size() * sizeof(float), 0) < 0) {
    error("[Agent Server] ERROR sending state features from socket");
  }

  // Get the action
  Action action;
  if (recv(newsockfd, &action, sizeof(Action), 0) < 0) {
    error("[Agent Server] ERROR recv from socket");
  }
  switch(action.action) {
    case DASH:
      this->doDash(action.arg1, action.arg2);
      break;
    case TURN:
      this->doTurn(action.arg1);
      break;
    case TACKLE:
      this->doTackle(action.arg1, false);
      break;
    case KICK:
      this->doKick(action.arg1, action.arg2);
      break;
    case MOVE:
      this->doMove();
      break;
    case SHOOT:
      this->doShoot();
      break;
    case PASS:
      this->doPass();
      break;
    case DRIBBLE:
      this->doDribble();
      break;
    case QUIT:
      std::cout << "[Agent Server] Got quit from agent." << std::endl;
      exit(0);
    default:
      std::cerr << "[Agent Server] ERROR Unsupported Action: "
                << action.action << std::endl;
      exit(1);
  }

  // For now let's not worry about turning the neck or setting the vision.
  this->setViewAction(new View_Tactical());
  this->setNeckAction(new Neck_TurnToBallOrScan());
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Agent::handleActionStart()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
Agent::handleActionEnd()
{
    if ( world().self().posValid() )
    {
#if 0
        const ServerParam & SP = ServerParam::i();
        //
        // inside of pitch
        //

        // top,lower
        debugClient().addLine( Vector2D( world().ourOffenseLineX(),
                                         -SP.pitchHalfWidth() ),
                               Vector2D( world().ourOffenseLineX(),
                                         -SP.pitchHalfWidth() + 3.0 ) );
        // top,lower
        debugClient().addLine( Vector2D( world().ourDefenseLineX(),
                                         -SP.pitchHalfWidth() ),
                               Vector2D( world().ourDefenseLineX(),
                                         -SP.pitchHalfWidth() + 3.0 ) );

        // bottom,upper
        debugClient().addLine( Vector2D( world().theirOffenseLineX(),
                                         +SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().theirOffenseLineX(),
                                         +SP.pitchHalfWidth() ) );
        //
        debugClient().addLine( Vector2D( world().offsideLineX(),
                                         world().self().pos().y - 15.0 ),
                               Vector2D( world().offsideLineX(),
                                         world().self().pos().y + 15.0 ) );

        // outside of pitch

        // top,upper
        debugClient().addLine( Vector2D( world().ourOffensePlayerLineX(),
                                         -SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().ourOffensePlayerLineX(),
                                         -SP.pitchHalfWidth() ) );
        // top,upper
        debugClient().addLine( Vector2D( world().ourDefensePlayerLineX(),
                                         -SP.pitchHalfWidth() - 3.0 ),
                               Vector2D( world().ourDefensePlayerLineX(),
                                         -SP.pitchHalfWidth() ) );
        // bottom,lower
        debugClient().addLine( Vector2D( world().theirOffensePlayerLineX(),
                                         +SP.pitchHalfWidth() ),
                               Vector2D( world().theirOffensePlayerLineX(),
                                         +SP.pitchHalfWidth() + 3.0 ) );
        // bottom,lower
        debugClient().addLine( Vector2D( world().theirDefensePlayerLineX(),
                                         +SP.pitchHalfWidth() ),
                               Vector2D( world().theirDefensePlayerLineX(),
                                         +SP.pitchHalfWidth() + 3.0 ) );
#else
        // top,lower
        debugClient().addLine( Vector2D( world().ourDefenseLineX(),
                                         world().self().pos().y - 2.0 ),
                               Vector2D( world().ourDefenseLineX(),
                                         world().self().pos().y + 2.0 ) );

        //
        debugClient().addLine( Vector2D( world().offsideLineX(),
                                         world().self().pos().y - 15.0 ),
                               Vector2D( world().offsideLineX(),
                                         world().self().pos().y + 15.0 ) );
#endif
    }

    //
    // ball position & velocity
    //
    dlog.addText( Logger::WORLD,
                  "WM: BALL pos=(%lf, %lf), vel=(%lf, %lf, r=%lf, ang=%lf)",
                  world().ball().pos().x,
                  world().ball().pos().y,
                  world().ball().vel().x,
                  world().ball().vel().y,
                  world().ball().vel().r(),
                  world().ball().vel().th().degree() );


    dlog.addText( Logger::WORLD,
                  "WM: SELF move=(%lf, %lf, r=%lf, th=%lf)",
                  world().self().lastMove().x,
                  world().self().lastMove().y,
                  world().self().lastMove().r(),
                  world().self().lastMove().th().degree() );

    Vector2D diff = world().ball().rpos() - world().ball().rposPrev();
    dlog.addText( Logger::WORLD,
                  "WM: BALL rpos=(%lf %lf) prev_rpos=(%lf %lf) diff=(%lf %lf)",
                  world().ball().rpos().x,
                  world().ball().rpos().y,
                  world().ball().rposPrev().x,
                  world().ball().rposPrev().y,
                  diff.x,
                  diff.y );

    Vector2D ball_move = diff + world().self().lastMove();
    Vector2D diff_vel = ball_move * ServerParam::i().ballDecay();
    dlog.addText( Logger::WORLD,
                  "---> ball_move=(%lf %lf) vel=(%lf, %lf, r=%lf, th=%lf)",
                  ball_move.x,
                  ball_move.y,
                  diff_vel.x,
                  diff_vel.y,
                  diff_vel.r(),
                  diff_vel.th().degree() );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Agent::handleServerParam()
{
    if ( KickTable::instance().createTables() )
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << " KickTable created."
                  << std::endl;
    }
    else
    {
        std::cerr << world().teamName() << ' '
                  << world().self().unum() << ": "
                  << " KickTable failed..."
                  << std::endl;
        M_client->setServerAlive( false );
    }


    if ( ServerParam::i().keepawayMode() )
    {
        std::cerr << "set Keepaway mode communication." << std::endl;
        M_communication = Communication::Ptr( new KeepawayCommunication() );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Agent::handlePlayerParam()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
void
Agent::handlePlayerType()
{

}

/*-------------------------------------------------------------------*/
/*!
  communication decision.
  virtual method in super class
*/
void
Agent::communicationImpl()
{
    if ( M_communication )
    {
        M_communication->execute( this );
    }
}

/*-------------------------------------------------------------------*/
/*!
*/
bool
Agent::doPreprocess()
{
    // check tackle expires
    // check self position accuracy
    // ball search
    // check queued intention
    // check simultaneous kick

    const WorldModel & wm = this->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPreProcess)" );

    //
    // freezed by tackle effect
    //
    if ( wm.self().isFrozen() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle wait. expires= %d",
                      wm.self().tackleExpires() );
        // face neck to ball
        this->setViewAction( new View_Tactical() );
        this->setNeckAction( new Neck_TurnToBallOrScan() );
        return true;
    }

    //
    // BeforeKickOff or AfterGoal. jump to the initial position
    //
    if ( wm.gameMode().type() == GameMode::BeforeKickOff
         || wm.gameMode().type() == GameMode::AfterGoal_ )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": before_kick_off" );
        Vector2D move_point =  Strategy::i().getPosition( wm.self().unum() );
        Bhv_CustomBeforeKickOff( move_point ).execute( this );
        this->setViewAction( new View_Tactical() );
        return true;
    }

    //
    // self localization error
    //
    if ( ! wm.self().posValid() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": invalid my pos" );
        Bhv_Emergency().execute( this ); // includes change view
        return true;
    }

    //
    // ball localization error
    //
    const int count_thr = ( wm.self().goalie()
                            ? 10
                            : 5 );
    if ( wm.ball().posCount() > count_thr
         || ( wm.gameMode().type() != GameMode::PlayOn
              && wm.ball().seenPosCount() > count_thr + 10 ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": search ball" );
        this->setViewAction( new View_Tactical() );
        Bhv_NeckBodyToBall().execute( this );
        return true;
    }

    //
    // set default change view
    //

    this->setViewAction( new View_Tactical() );

    //
    // check shoot chance
    //
    if ( doShoot() )
    {
        return true;
    }

    //
    // check queued action
    //
    if ( this->doIntention() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": do queued intention" );
        return true;
    }

    //
    // check simultaneous kick
    //
    if ( doForceKick() )
    {
        return true;
    }

    //
    // check pass message
    //
    if ( doHeardPassReceive() )
    {
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doShoot()
{
    const WorldModel & wm = this->world();

    if ( wm.gameMode().type() != GameMode::IndFreeKick_
         && wm.time().stopped() == 0
         && wm.self().isKickable()
         && Bhv_StrictCheckShoot().execute( this ) )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": shooted" );

        // reset intention
        this->setIntention( static_cast< SoccerIntention * >( 0 ) );
        return true;
    }

    return false;
}


/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doPass()
{
    rcsc::Body_Pass pass;
    pass.get_best_pass(this->world(), NULL, NULL, NULL);
    pass.execute(this);
    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doDribble()
{
  Strategy::instance().update( world() );
  M_field_evaluator = createFieldEvaluator();
  CompositeActionGenerator * g = new CompositeActionGenerator();
  g->addGenerator(new ActGen_MaxActionChainLengthFilter(new ActGen_ShortDribble(), 1));
  M_action_generator = ActionGenerator::ConstPtr(g);
  ActionChainHolder::instance().setFieldEvaluator( M_field_evaluator );
  ActionChainHolder::instance().setActionGenerator( M_action_generator );
  doPreprocess();
  ActionChainHolder::instance().update( world() );
  Bhv_ChainAction(ActionChainHolder::instance().graph()).execute(this);
  return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doMove()
{
  Strategy::instance().update( world() );
  int role_num = Strategy::i().roleNumber(world().self().unum());
  Bhv_BasicMove().execute(this);
  return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doForceKick()
{
    const WorldModel & wm = this->world();

    if ( wm.gameMode().type() == GameMode::PlayOn
         && ! wm.self().goalie()
         && wm.self().isKickable()
         && wm.existKickableOpponent() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": simultaneous kick" );
        this->debugClient().addMessage( "SimultaneousKick" );
        Vector2D goal_pos( ServerParam::i().pitchHalfLength(), 0.0 );

        if ( wm.self().pos().x > 36.0
             && wm.self().pos().absY() > 10.0 )
        {
            goal_pos.x = 45.0;
            dlog.addText( Logger::TEAM,
                          __FILE__": simultaneous kick cross type" );
        }
        Body_KickOneStep( goal_pos,
                          ServerParam::i().ballSpeedMax()
                          ).execute( this );
        this->setNeckAction( new Neck_ScanField() );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doHeardPassReceive()
{
    const WorldModel & wm = this->world();

    if ( wm.audioMemory().passTime() != wm.time()
         || wm.audioMemory().pass().empty()
         || wm.audioMemory().pass().front().receiver_ != wm.self().unum() )
    {

        return false;
    }

    int self_min = wm.interceptTable()->selfReachCycle();
    Vector2D intercept_pos = wm.ball().inertiaPoint( self_min );
    Vector2D heard_pos = wm.audioMemory().pass().front().receive_pos_;

    dlog.addText( Logger::TEAM,
                  __FILE__":  (doHeardPassReceive) heard_pos(%.2f %.2f) intercept_pos(%.2f %.2f)",
                  heard_pos.x, heard_pos.y,
                  intercept_pos.x, intercept_pos.y );

    if ( ! wm.existKickableTeammate()
         && wm.ball().posCount() <= 1
         && wm.ball().velCount() <= 1
         && self_min < 20
         //&& intercept_pos.dist( heard_pos ) < 3.0 ) //5.0 )
         )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHeardPassReceive) intercept cycle=%d. intercept",
                      self_min );
        this->debugClient().addMessage( "Comm:Receive:Intercept" );
        Body_Intercept().execute( this );
        this->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doHeardPassReceive) intercept cycle=%d. go to receive point",
                      self_min );
        this->debugClient().setTarget( heard_pos );
        this->debugClient().addMessage( "Comm:Receive:GoTo" );
        Body_GoToPoint( heard_pos,
                    0.5,
                        ServerParam::i().maxDashPower()
                        ).execute( this );
        this->setNeckAction( new Neck_TurnToBall() );
    }

    this->setIntention( new IntentionReceive( heard_pos,
                                              ServerParam::i().maxDashPower(),
                                              0.9,
                                              5,
                                              wm.time() ) );

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
FieldEvaluator::ConstPtr
Agent::getFieldEvaluator() const
{
    return M_field_evaluator;
}

/*-------------------------------------------------------------------*/
/*!

*/
FieldEvaluator::ConstPtr
Agent::createFieldEvaluator() const
{
    return FieldEvaluator::ConstPtr( new SampleFieldEvaluator );
}


/*-------------------------------------------------------------------*/
/*!
*/
ActionGenerator::ConstPtr
Agent::createActionGenerator() const
{
    CompositeActionGenerator * g = new CompositeActionGenerator();

    //
    // shoot
    //
    g->addGenerator( new ActGen_RangeActionChainLengthFilter
                     ( new ActGen_Shoot(),
                       2, ActGen_RangeActionChainLengthFilter::MAX ) );

    //
    // strict check pass
    //
    g->addGenerator( new ActGen_MaxActionChainLengthFilter
                     ( new ActGen_StrictCheckPass(), 1 ) );

    //
    // cross
    //
    g->addGenerator( new ActGen_MaxActionChainLengthFilter
                     ( new ActGen_Cross(), 1 ) );

    //
    // direct pass
    //
    // g->addGenerator( new ActGen_RangeActionChainLengthFilter
    //                  ( new ActGen_DirectPass(),
    //                    2, ActGen_RangeActionChainLengthFilter::MAX ) );

    //
    // short dribble
    //
    g->addGenerator( new ActGen_MaxActionChainLengthFilter
                     ( new ActGen_ShortDribble(), 1 ) );

    //
    // self pass (long dribble)
    //
    g->addGenerator( new ActGen_MaxActionChainLengthFilter
                     ( new ActGen_SelfPass(), 1 ) );

    //
    // simple dribble
    //
    // g->addGenerator( new ActGen_RangeActionChainLengthFilter
    //                  ( new ActGen_SimpleDribble(),
    //                    2, ActGen_RangeActionChainLengthFilter::MAX ) );

    return ActionGenerator::ConstPtr( g );
}
