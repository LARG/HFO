#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "agent.h"
#include "custom_message.h"

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
#include "shoot_generator.h"
#include "bhv_force_pass.h"

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
#include <rcsc/action/body_smart_kick.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>
#include <rcsc/action/view_synch.h>

#include <rcsc/formation/formation.h>
#include <rcsc/action/kick_table.h>
#include <rcsc/player/intercept_table.h>
#include <rcsc/player/say_message_builder.h>
#include <rcsc/player/audio_sensor.h>
#include <rcsc/player/freeform_parser.h>
#include <rcsc/player/free_message.h>

#include <rcsc/common/basic_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
// #include <rcsc/common/free_message_parser.h>

#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <algorithm>    // std::sort
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <poll.h>

using namespace rcsc;
using namespace hfo;

Agent::Agent()
    : PlayerAgent(),
      M_communication(),
      M_field_evaluator(createFieldEvaluator()),
      M_action_generator(createActionGenerator()),
      feature_set(LOW_LEVEL_FEATURE_SET),
      feature_extractor(NULL),
      lastTrainerMessageTime(-1),
      lastTeammateMessageTime(-1),
      lastStatusUpdateTime(-1),
      game_status(IN_GAME),
      requested_action(NOOP)
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

    // setup last_action variable
    last_action_status = false;
}

Agent::~Agent() {
  if (feature_extractor != NULL) {
    delete feature_extractor;
  }
}

int Agent::getUnum() {
  return world().self().unum();
}

bool Agent::initImpl(CmdLineParser & cmd_parser) {
    bool result = PlayerAgent::initImpl(cmd_parser);

    // read additional options
    result &= Strategy::instance().init(cmd_parser);

    rcsc::ParamMap my_params("Additional options");
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
          << "ERROR: Action recording requested but no supported."
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

    return true;
}

FeatureExtractor* Agent::getFeatureExtractor(feature_set_t feature_set_indx,
                                             int numTeammates,
                                             int numOpponents,
                                             bool playing_offense) {
  switch (feature_set_indx) {
    case LOW_LEVEL_FEATURE_SET:
      return new LowLevelFeatureExtractor(numTeammates, numOpponents,
                                          playing_offense);
      break;
    case HIGH_LEVEL_FEATURE_SET:
      return new HighLevelFeatureExtractor(numTeammates, numOpponents,
                                           playing_offense);
      break;
    default:
      std::cerr << "ERROR: Unrecognized Feature set index: "
                << feature_set_indx << std::endl;
      exit(1);
  }
}

/*!
  main decision
  virtual method in super class
*/
void Agent::actionImpl() {
  if (requested_action < 0) {
    std::cerr << "ERROR: No action. Did you forget to call act()?" << std::endl;
    exit(1);
  }
  if (hfo::NumParams(requested_action) > params.size()) {
    std::cerr << "ERROR: Not enough params for requested action! Action "
              << ActionToString(requested_action) << " requires "
              << hfo::NumParams(requested_action)
              << " parameters, given " << params.size() << std::endl;
    exit(1);
  }

  // For now let's not worry about turning the neck or setting the vision.
  // But do the settings now, so that doesn't override any set by the actions below.
  // TODO: Add setViewActionDefault, setNeckActionDefault to librcsc that only set if not already set.

  const WorldModel & wm = this->world();

  this->setViewAction(new View_Tactical());

  if (wm.ball().posValid()) {
    this->setNeckAction(new Neck_TurnToBallOrScan()); // if not ball().posValid(), requests possibly-invalid queuedNextBallPos()
  } else {
    this->setNeckAction(new Neck_ScanField()); // equivalent to Neck_TurnToBall()
  }



  switch(requested_action) {
    case DASH:
      last_action_status = this->doDash(params[0], params[1]);
      break;
    case TURN:
      last_action_status = this->doTurn(params[0]);
      break;
    case TACKLE:
      last_action_status = this->doTackle(params[0], false);
      break;
    case KICK:
      last_action_status = this->doKick(params[0], params[1]);
      break;
    case KICK_TO:
      if (feature_extractor != NULL) {
        last_action_status = Body_SmartKick(Vector2D(feature_extractor->absoluteXPos(params[0]),
						     feature_extractor->absoluteYPos(params[1])),
					    params[2], params[2] * 0.99, 3).execute(this);
      }
      break;
    case MOVE_TO:
      if (feature_extractor != NULL) {
	last_action_status = Body_GoToPoint(Vector2D(feature_extractor->absoluteXPos(params[0]),
						     feature_extractor->absoluteYPos(params[1])), 0.25,
					    ServerParam::i().maxDashPower()).execute(this);
	last_action_status |= wm.self().collidesWithPost(); // can get out of collision w/post
      }
      break;
    case DRIBBLE_TO:
      if (feature_extractor != NULL) {
        last_action_status = Body_Dribble(Vector2D(feature_extractor->absoluteXPos(params[0]),
						   feature_extractor->absoluteYPos(params[1])), 1.0,
					  ServerParam::i().maxDashPower(), 2).execute(this);
	last_action_status |= wm.self().collidesWithPost(); // ditto
      }
      break;
    case INTERCEPT:
      last_action_status = Body_Intercept().execute(this);
      last_action_status |= wm.self().collidesWithPost(); // ditto
      break;
    case MOVE:
      last_action_status = this->doMove();
      break;
    case SHOOT:
      last_action_status = this->doSmartKick();
      break;
    case PASS:
      last_action_status = this->doPassTo(int(params[0]));
      break;
    case DRIBBLE:
      last_action_status = this->doDribble();
      break;
    case CATCH:
      last_action_status = this->doCatch();
      break;
    case NOOP:
      last_action_status = false;
      break;
    case QUIT:
      std::cout << "Got quit from agent." << std::endl;
      handleExit();
      return;
    case REDUCE_ANGLE_TO_GOAL:
      last_action_status = this->doReduceAngleToGoal();
      break;
    case MARK_PLAYER:
      last_action_status = this->doMarkPlayer(int(params[0]));
      break;
    case DEFEND_GOAL:
      last_action_status = this->doDefendGoal();
      break;
    case GO_TO_BALL:
      last_action_status = this->doGoToBall();
      break;
    case REORIENT:
      last_action_status = this->doReorient();
      break;
    default:
      std::cerr << "ERROR: Unsupported Action: "
                << requested_action << std::endl;
      exit(1);
  }

}

void
Agent::ProcessTrainerMessages()
{
  if (audioSensor().trainerMessageTime().cycle() > lastTrainerMessageTime) {
    const std::string& message = audioSensor().trainerMessage();
    if (feature_extractor == NULL) {
      hfo::Config hfo_config;
      if (hfo::ParseConfig(message, hfo_config)) {
        bool playing_offense = world().ourSide() == rcsc::LEFT;
        num_teammates = playing_offense ?
	  hfo_config.num_offense - 1 : hfo_config.num_defense - 1;
        num_opponents = playing_offense ?
	  hfo_config.num_defense : hfo_config.num_offense;
        feature_extractor = getFeatureExtractor(
            feature_set, num_teammates, num_opponents, playing_offense);
      }
    }
    if (hfo::ParseGameStatus(message, game_status)) {
      lastStatusUpdateTime = audioSensor().trainerMessageTime().cycle();
    }
    hfo::ParsePlayerOnBall(message, player_on_ball);
    lastTrainerMessageTime = audioSensor().trainerMessageTime().cycle();
  }
}

void
Agent::ProcessTeammateMessages()
{
  hear_msg.clear();
  if (audioSensor().teammateMessageTime().cycle() > lastTeammateMessageTime) {
    const std::list<HearMessage> teammateMessages = audioSensor().teammateMessages();
    for (std::list<HearMessage>::const_iterator msgIter = teammateMessages.begin();
         msgIter != teammateMessages.end(); msgIter++) {
      if ((*msgIter).unum_ != world().self().unum()) {
        hear_msg = (*msgIter).str_;
        break;  // For now we just take one.
      }
    }
    lastTeammateMessageTime = audioSensor().teammateMessageTime().cycle();
  }
}

void
Agent::UpdateFeatures()
{
  if (feature_extractor != NULL) {
    state = feature_extractor->ExtractFeatures(this->world(),
					       getLastActionStatus());
  }
}

void
Agent::handleActionStart()
{
  ProcessTrainerMessages();
  ProcessTeammateMessages();
  UpdateFeatures();

  // Optionally write to logfile
#ifdef ELOG
  if (config().record() && feature_extractor != NULL) {
    elog.addText(Logger::WORLD, "GameStatus %d", game_status);
    elog.flush();
    feature_extractor->LogFeatures();
  }
#endif
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
  // Say the outgoing message
  if (!say_msg.empty()) {
    addSayMessage(new CustomMessage(say_msg));
    say_msg.clear();
  }
  // Disabled since it adds default communication messages which
  // can conflict with our comm messages.
  // if ( M_communication )
  // {
  //   M_communication->execute( this );
  // }
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
    // frozen by tackle effect
    //
    if ( wm.self().isFrozen() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle wait. expires= %d",
                      wm.self().tackleExpires() );
        // face neck to ball
        this->setViewAction( new View_Tactical() );
	if (wm.ball().posValid()) {
	  this->setNeckAction( new Neck_TurnToBallOrScan() );
	} else{
	  this->setNeckAction( new Neck_TurnToBall() );
	}
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
        return Bhv_Emergency().execute( this ); // includes change view
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
        return Bhv_NeckBodyToBall().execute( this );
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
  Alternative high-level action to always doing "Move"; usable by either side, although
  probably more useful for offense. Variant of doPreprocess (above), which is called by doDribble.
*/
bool
Agent::doReorient()
{
    // check tackle expires
    // check self position accuracy
    // ball search
    // check queued intention

    const WorldModel & wm = this->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doPreProcessAsAction)" );

    //
    // frozen by tackle effect
    //
    if ( wm.self().isFrozen() )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": tackle wait. expires= %d",
                      wm.self().tackleExpires() );

	return Bhv_Emergency().execute( this ); // includes change view
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
    if ( ! ( wm.self().posValid() && wm.self().velValid() ) )
    {
      if (! wm.self().posValid() ) {
        dlog.addText( Logger::TEAM,
                      __FILE__": invalid my pos" );
      } else {
	dlog.addText( Logger::TEAM,
                      __FILE__": invalid my vel" );
      }
      return Bhv_Emergency().execute( this ); // includes change view
    }

    //
    // set default change view
    //

    this->setViewAction( new View_Tactical() );

    //
    // ball localization error
    //

    const BallObject& ball = wm.ball();
    if (! ( ball.posValid() && ball.velValid() )) {
      dlog.addText( Logger::TEAM,
		    __FILE__": search ball" );

      return Bhv_NeckBodyToBall().execute( this );
    }

    //
    // check pass message
    //
    if ( doHeardPassReceive() )
    {
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

        return Bhv_NeckBodyToBall().execute( this );
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

bool
Agent::doSmartKick()
{
    const ShootGenerator::Container & cont =
        ShootGenerator::instance().courses(this->world(), false);
    ShootGenerator::Container::const_iterator best_shoot
        = std::min_element(cont.begin(), cont.end(), ShootGenerator::ScoreCmp());
    return Body_SmartKick(best_shoot->target_point_,
			  best_shoot->first_ball_speed_,
			  best_shoot->first_ball_speed_ * 0.99,
			  3).execute(this);
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

bool
Agent::doPassTo(int receiver)
{
    Force_Pass pass;
    const WorldModel & wm = this->world();
    pass.get_pass_to_player(wm, receiver);
    if (pass.execute(this) || wm.self().collidesWithBall()) { // can sometimes fix
      return true;
    }
    return false;
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
  bool preprocess_success = doPreprocess();
  ActionChainHolder::instance().update( world() );
  if (Bhv_ChainAction(ActionChainHolder::instance().graph()).execute(this) ||
      preprocess_success) {
    return true;
  }
  return false;
}

/*-------------------------------------------------------------------*/
/*!

*/
bool
Agent::doMove()
{
  Strategy::instance().update( world() );
  return Bhv_BasicMove().execute(this);
}

/*-------------------------------------------------------------------*/
/*!
 * This Action marks the player with the specified uniform number.
*/
bool Agent::doMarkPlayer(int unum) {
  const WorldModel & wm = this->world();
  Vector2D kicker_pos = Vector2D :: INVALIDATED;
  Vector2D player_pos =  Vector2D :: INVALIDATED;
  int kicker_unum = -1;
  const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();
  int count = 0;
  for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin(); it != o_end; ++it ) {
      if ( (*it)->distFromBall() < 5 ) {
	if ((kicker_unum == -1) || (kicker_unum != unum)) { // try to obey action instruction
          kicker_pos = (*it)->pos();
          kicker_unum = (*it)->unum();
	}
      }
  }

  for ( PlayerPtrCont::const_iterator it = wm.opponentsFromSelf().begin(); it !=  o_end; ++it ) {
	  if ( (*it)-> unum() == unum ) {
           player_pos = (*it)->pos();
      }
  }

  if (!player_pos.isValid()) {
      //Player to be marked not found
      return false;
  }
  if (!kicker_pos.isValid()) {
      //Kicker not found
      return false;
  }
  if (unum == kicker_unum || kicker_pos.equals(player_pos)) {
    //Player to be marked is kicker
    return false;
  }
  double x = player_pos.x + (kicker_pos.x - player_pos.x)*0.1;
  double y = player_pos.y + (kicker_pos.y - player_pos.y)*0.1;

  if (Body_GoToPoint(Vector2D(x,y), 0.25, ServerParam::i().maxDashPower()).execute(this) ||
      wm.self().collidesWithPost()) { // latter because sometimes fixes
    return true;
  }
  return false;
}

/*-------------------------------------------------------------------*/
/*!
 *
 * This action cuts off the angle between the shooter and the goal the players always move to a dynamic line in between the kicker and the goal.
 */

/* Comparator for sorting teammates based on y positions.*/
bool compare_y_pos (PlayerObject* i, PlayerObject* j) {
  return i->pos().y < j->pos().y;
}

bool Agent::doReduceAngleToGoal() {
  const WorldModel & wm = this->world();
  Vector2D goal_pos1( -ServerParam::i().pitchHalfLength(), ServerParam::i().goalHalfWidth() );
  Vector2D goal_pos2( -ServerParam::i().pitchHalfLength(), -ServerParam::i().goalHalfWidth() );

  const PlayerPtrCont::const_iterator o_end = wm.opponentsFromSelf().end();

  const BallObject& ball = wm.ball();
  if (! ball.posValid()) {
    return false;
  }

  Vector2D ball_pos = ball.pos();
  double nearRatio = 0.9;
  const PlayerPtrCont::const_iterator o_t_end = wm.teammatesFromSelf().end();

  std::vector<PlayerObject*> teammatesBetweenBallAndGoal;
  Vector2D self_pos = wm.self().pos();
  Line2D goal_line_1 (goal_pos1, ball_pos);
  Line2D goal_line_2 (goal_pos2, ball_pos);
  Vector2D max_angle_end_pt1 = goal_pos2;
  Vector2D max_angle_end_pt2 = goal_pos1;

  //filter out points not lie in the cone
  for (PlayerPtrCont::const_iterator it1 = wm.teammatesFromSelf().begin(); it1 != o_t_end; ++it1 ) {
      Vector2D teammate_pos = (*it1)->pos();
      double y1 = goal_line_1.getY(teammate_pos.x);
      double y2 = goal_line_2.getY(teammate_pos.x);
      //check x coordinate
      if (teammate_pos.x >= ball_pos.x ) {
          continue;
      }
      // check y coordinate

      if (teammate_pos.y <= ((y1>y2)?y2:y1) || teammate_pos.y >= ((y1>=y2)?y1:y2)) {
          continue;
      }

      //push into vector if it passes both tests
      teammatesBetweenBallAndGoal.push_back(*it1);
  }

  sort(teammatesBetweenBallAndGoal.begin(), teammatesBetweenBallAndGoal.end(), compare_y_pos);
  double max_angle = 0;

  /* find max angle and choose endpoints*/
  for (int i = 0; i < teammatesBetweenBallAndGoal.size() + 1; i++) {
      Vector2D first_pos = Vector2D::INVALIDATED;
      if (i == 0) {
          first_pos = goal_pos2;
      } else {
          first_pos = teammatesBetweenBallAndGoal[i-1] -> pos();
      }
      Vector2D second_pos = Vector2D::INVALIDATED;
      if ( i== teammatesBetweenBallAndGoal.size()) {
          second_pos = goal_pos1;
      } else {
          second_pos = teammatesBetweenBallAndGoal[i] -> pos();
      }

      double angle1 = atan2(ball_pos.y - first_pos.y, ball_pos.x - first_pos.x);
      double angle2 = atan2(ball_pos.y - second_pos.y, ball_pos.x - second_pos.x);
      AngleDeg open_angle;
      double open_angle_value = fabs(open_angle.normalize_angle(open_angle.rad2deg(angle1-angle2)));
      if ( open_angle_value > max_angle) {
          max_angle = open_angle_value;
          max_angle_end_pt1 = first_pos;
          max_angle_end_pt2 = second_pos;
      }
  }

  /*Calculate and go to target point */
  Vector2D targetLineEnd1 = max_angle_end_pt1*(1-nearRatio) + ball_pos*nearRatio;
  Vector2D targetLineEnd2 = max_angle_end_pt2*(1-nearRatio) + ball_pos*nearRatio;
  double dist_to_end1 = targetLineEnd1.dist2(ball_pos);
  double dist_to_end2 = targetLineEnd2.dist2(ball_pos);
  double ratio = dist_to_end2/(dist_to_end1+dist_to_end2);
  Vector2D target = targetLineEnd1 * ratio + targetLineEnd2 * (1-ratio);
  if (Body_GoToPoint(target, 0.25, ServerParam::i().maxDashPower()).execute(this) ||
      wm.self().collidesWithPost()) { // latter because sometimes fixes
    return true;
  }
  return false;
}

/*-------------------------------------------------------------------*/

/*!
 *
 * This action cuts off the angle between the shooter and the goal; the player always moves on a fixed line.
 */

bool Agent::doDefendGoal() {
  const WorldModel & wm = this->world();
  Vector2D goal_pos1( -ServerParam::i().pitchHalfLength() + ServerParam::i().goalAreaLength(), ServerParam::i().goalHalfWidth() );
  Vector2D goal_pos2( -ServerParam::i().pitchHalfLength() + ServerParam::i().goalAreaLength(), -ServerParam::i().goalHalfWidth() );
  const BallObject& ball = wm.ball();
  if (! ball.posValid()) {
    return false;
  }

  Vector2D ball_pos = ball.pos();
  double dist_to_post1 = goal_pos1.dist2(ball_pos);
  double dist_to_post2 = goal_pos2.dist2(ball_pos);
  double ratio = dist_to_post2/(dist_to_post1+dist_to_post2);
  Vector2D target = goal_pos1 * ratio + goal_pos2 * (1-ratio);

  if (Body_GoToPoint(target, 0.25, ServerParam::i().maxDashPower()).execute(this) ||
      wm.self().collidesWithPost()) { // latter because sometimes fixes
    return true;
  }
  return false;
}

/*-------------------------------------------------------------------*/

/*!
 *
 * This action makes the agent head directly head towards the ball.
 */


bool Agent::doGoToBall() {
  const WorldModel & wm = this->world();
  const BallObject& ball = wm.ball();
  if (! ball.posValid()) {
    return false;
  }
  return Body_GoToPoint(ball.pos(), 0.25, ServerParam::i().maxDashPower()).execute(this);
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
