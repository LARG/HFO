#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lowlevel_feature_extractor.h"
#include <rcsc/common/server_param.h>

using namespace rcsc;

LowLevelFeatureExtractor::LowLevelFeatureExtractor(int num_teammates,
                                                   int num_opponents,
                                                   bool playing_offense) :
    FeatureExtractor(num_teammates, num_opponents, playing_offense)
{
  assert(numTeammates >= 0);
  assert(numOpponents >= 0);
  numFeatures = num_basic_features +
      features_per_player * (numTeammates + numOpponents);
  feature_vec.resize(numFeatures);
}

LowLevelFeatureExtractor::~LowLevelFeatureExtractor() {}

const std::vector<float>& LowLevelFeatureExtractor::ExtractFeatures(
    const WorldModel& wm) {
  featIndx = 0;
  const ServerParam& SP = ServerParam::i();
  // ======================== SELF FEATURES ======================== //
  const SelfObject& self = wm.self();
  const Vector2D& self_pos = self.pos();
  const AngleDeg& self_ang = self.body();
  addFeature(self.posValid() ? FEAT_MAX : FEAT_MIN);
  // ADD_FEATURE(self_pos.x);
  // ADD_FEATURE(self_pos.y);

  // Direction and speed of the agent.
  addFeature(self.velValid() ? FEAT_MAX : FEAT_MIN);
  if (self.velValid()) {
    addAngFeature(self_ang - self.vel().th());
    addNormFeature(self.speed(), 0., observedSelfSpeedMax);
  } else {
    addFeature(0);
    addFeature(0);
    addFeature(0);
  }

  // Global Body Angle -- 0:right -90:up 90:down 180/-180:left
  addAngFeature(self_ang);

  // Neck Angle -- We probably don't need this unless we are
  // controlling the neck manually.
  // std::cout << "Face Error: " << self.faceError() << std::endl;
  // if (self.faceValid()) {
  //   std::cout << "FaceAngle: " << self.face() << std::endl;
  // }

  addNormFeature(self.stamina(), 0., observedStaminaMax);
  addFeature(self.isFrozen() ? FEAT_MAX : FEAT_MIN);

  // Probabilities - Do we want these???
  // std::cout << "catchProb: " << self.catchProbability() << std::endl;
  // std::cout << "tackleProb: " << self.tackleProbability() << std::endl;
  // std::cout << "fouldProb: " << self.foulProbability() << std::endl;

  // Features indicating if we are colliding with an object
  addFeature(self.collidesWithBall()   ? FEAT_MAX : FEAT_MIN);
  addFeature(self.collidesWithPlayer() ? FEAT_MAX : FEAT_MIN);
  addFeature(self.collidesWithPost()   ? FEAT_MAX : FEAT_MIN);
  addFeature(self.isKickable()         ? FEAT_MAX : FEAT_MIN);

  // inertiaPoint estimates the ball point after a number of steps
  // self.inertiaPoint(n_steps);

  // ======================== LANDMARK FEATURES ======================== //
  // Top Bottom Center of Goal
  rcsc::Vector2D goalCenter(pitchHalfLength, 0);
  addLandmarkFeatures(goalCenter, self_pos, self_ang);
  rcsc::Vector2D goalPostTop(pitchHalfLength, -goalHalfWidth);
  addLandmarkFeatures(goalPostTop, self_pos, self_ang);
  rcsc::Vector2D goalPostBot(pitchHalfLength, goalHalfWidth);
  addLandmarkFeatures(goalPostBot, self_pos, self_ang);

  // Top Bottom Center of Penalty Box
  rcsc::Vector2D penaltyBoxCenter(pitchHalfLength - penaltyAreaLength, 0);
  addLandmarkFeatures(penaltyBoxCenter, self_pos, self_ang);
  rcsc::Vector2D penaltyBoxTop(pitchHalfLength - penaltyAreaLength,
                               -penaltyAreaWidth / 2.);
  addLandmarkFeatures(penaltyBoxTop, self_pos, self_ang);
  rcsc::Vector2D penaltyBoxBot(pitchHalfLength - penaltyAreaLength,
                               penaltyAreaWidth / 2.);
  addLandmarkFeatures(penaltyBoxBot, self_pos, self_ang);

  // Corners of the Playable Area
  rcsc::Vector2D centerField(0, 0);
  addLandmarkFeatures(centerField, self_pos, self_ang);
  rcsc::Vector2D cornerTopLeft(0, -pitchHalfWidth);
  addLandmarkFeatures(cornerTopLeft, self_pos, self_ang);
  rcsc::Vector2D cornerTopRight(pitchHalfLength, -pitchHalfWidth);
  addLandmarkFeatures(cornerTopRight, self_pos, self_ang);
  rcsc::Vector2D cornerBotRight(pitchHalfLength, pitchHalfWidth);
  addLandmarkFeatures(cornerBotRight, self_pos, self_ang);
  rcsc::Vector2D cornerBotLeft(0, pitchHalfWidth);
  addLandmarkFeatures(cornerBotLeft, self_pos, self_ang);

  // Distances to the edges of the playable area
  if (self.posValid()) {
    // Distance to Left field line
    addDistFeature(self_pos.x, pitchHalfLength);
    // Distance to Right field line
    addDistFeature(pitchHalfLength - self_pos.x, pitchHalfLength);
    // Distance to top field line
    addDistFeature(pitchHalfWidth + self_pos.y, pitchWidth);
    // Distance to Bottom field line
    addDistFeature(pitchHalfWidth - self_pos.y, pitchWidth);
  } else {
    addFeature(0);
    addFeature(0);
    addFeature(0);
    addFeature(0);
  }

  // ======================== BALL FEATURES ======================== //
  const BallObject& ball = wm.ball();
  // Angle and distance to the ball
  addFeature(ball.rposValid() ? FEAT_MAX : FEAT_MIN);
  if (ball.rposValid()) {
    addLandmarkFeatures(ball.pos(), self_pos, self_ang);
    // addAngFeature(ball.angleFromSelf());
    // addDistFeature(ball.distFromSelf(), maxHFORadius);
  } else {
    addFeature(0);
    addFeature(0);
    addFeature(0);
  }
  // Velocity and direction of the ball
  addFeature(ball.velValid() ? FEAT_MAX : FEAT_MIN);
  if (ball.velValid()) {
    // SeverParam lists ballSpeedMax a 2.7 which is too low
    addNormFeature(ball.vel().r(), 0., observedBallSpeedMax);
    addAngFeature(ball.vel().th());
  } else {
    addFeature(0);
    addFeature(0);
    addFeature(0);
  }

  assert(featIndx == num_basic_features);

  // ======================== TEAMMATE FEATURES ======================== //
  // Vector of PlayerObject pointers sorted by increasing distance from self
  int detected_teammates = 0;
  const PlayerPtrCont& teammates = wm.teammatesFromSelf();
  for (PlayerPtrCont::const_iterator it = teammates.begin();
       it != teammates.end(); ++it) {
    PlayerObject* teammate = *it;
    if (teammate->pos().x > 0 && teammate->unum() > 0 &&
        detected_teammates < numTeammates) {
      addPlayerFeatures(*teammate, self_pos, self_ang);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    for (int j=0; j<features_per_player; ++j) {
      addFeature(0);
    }
  }

  // ======================== OPPONENT FEATURES ======================== //
  int detected_opponents = 0;
  const PlayerPtrCont& opponents = wm.opponentsFromSelf();
  for (PlayerPtrCont::const_iterator it = opponents.begin();
       it != opponents.end(); ++it) {
    PlayerObject* opponent = *it;
    if (opponent->pos().x > 0 && opponent->unum() > 0 &&
        detected_opponents < numOpponents) {
      addPlayerFeatures(*opponent, self_pos, self_ang);
      detected_opponents++;
    }
  }
  // Add zero features for any missing opponents
  for (int i=detected_opponents; i<numOpponents; ++i) {
    for (int j=0; j<features_per_player; ++j) {
      addFeature(0);
    }
  }
  assert(featIndx == numFeatures);
  checkFeatures();
  return feature_vec;
}
