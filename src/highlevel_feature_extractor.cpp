#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "highlevel_feature_extractor.h"
#include <rcsc/common/server_param.h>

using namespace rcsc;

HighLevelFeatureExtractor::HighLevelFeatureExtractor(int num_teammates,
                                                     int num_opponents,
                                                     bool playing_offense) :
    FeatureExtractor(num_teammates, num_opponents, playing_offense)
{
  assert(numTeammates >= 0);
  assert(numOpponents >= 0);
  numFeatures = num_basic_features + features_per_teammate * numTeammates
      + features_per_opponent * numOpponents;
  feature_vec.resize(numFeatures);
}

HighLevelFeatureExtractor::~HighLevelFeatureExtractor() {}

const std::vector<float>& HighLevelFeatureExtractor::ExtractFeatures(
    const WorldModel& wm) {
  featIndx = 0;
  const ServerParam& SP = ServerParam::i();
  const SelfObject& self = wm.self();
  const Vector2D& self_pos = self.pos();
  const float self_ang = self.body().radian();
  const PlayerCont& teammates = wm.teammates();
  const PlayerCont& opponents = wm.opponents();
  float maxR = sqrtf(SP.pitchHalfLength() * SP.pitchHalfLength()
                     + SP.pitchHalfWidth() * SP.pitchHalfWidth());
  // features about self pos
  // Allow the agent to go 10% over the playfield in any direction
  float tolerance_x = .1 * SP.pitchHalfLength();
  float tolerance_y = .1 * SP.pitchHalfWidth();
  // Feature[0]: X-postion
  if (playingOffense) {
    addNormFeature(self_pos.x, -tolerance_x, SP.pitchHalfLength() + tolerance_x);
  } else {
    addNormFeature(self_pos.x, -SP.pitchHalfLength()-tolerance_x, tolerance_x);
  }

  // Feature[1]: Y-Position
  addNormFeature(self_pos.y, -SP.pitchHalfWidth() - tolerance_y,
                 SP.pitchHalfWidth() + tolerance_y);

  // Feature[2]: Self Angle
  addNormFeature(self_ang, -M_PI, M_PI);

  float r;
  float th;
  // Features about the ball
  Vector2D ball_pos = wm.ball().pos();
  angleDistToPoint(self_pos, ball_pos, th, r);
  // Feature[3]: Dist to ball
  addNormFeature(r, 0, maxR);
  // Feature[4]: Ang to ball
  addNormFeature(th, -M_PI, M_PI);
  // Feature[5]: Able to kick
  addNormFeature(self.isKickable(), false, true);

  // Features about distance to goal center
  Vector2D goalCenter(SP.pitchHalfLength(), 0);
  if (!playingOffense) {
    goalCenter.assign(-SP.pitchHalfLength(), 0);
  }
  angleDistToPoint(self_pos, goalCenter, th, r);
  // Feature[6]: Goal Center Distance
  addNormFeature(r, 0, maxR);
  // Feature[7]: Angle to goal center
  addNormFeature(th, -M_PI, M_PI);
  // Feature[8]: largest open goal angle
  addNormFeature(calcLargestGoalAngle(wm, self_pos), 0, M_PI);
  // Feature[9]: Dist to our closest opp
  if (numOpponents > 0) {
    calcClosestOpp(wm, self_pos, th, r);
    addNormFeature(r, 0, maxR);
  } else {
    addFeature(FEAT_INVALID);
  }

  // Features[9 - 9+T]: teammate's open angle to goal
  int detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestGoalAngle(wm, teammate.pos()), 0, M_PI);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    addFeature(FEAT_INVALID);
  }

  // Features[9+T - 9+2T]: teammates' dists to closest opps
  if (numOpponents > 0) {
    detected_teammates = 0;
    for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
      const PlayerObject& teammate = *it;
      if (valid(teammate) && detected_teammates < numTeammates) {
        calcClosestOpp(wm, teammate.pos(), th, r);
        addNormFeature(r, 0, maxR);
        detected_teammates++;
      }
    }
    // Add zero features for any missing teammates
    for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
    }
  } else { // If no opponents, add invalid features
    for (int i=0; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
    }
  }

  // Features [9+2T - 9+3T]: open angle to teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestTeammateAngle(wm, self_pos, teammate.pos()),0,M_PI);
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    addFeature(FEAT_INVALID);
  }

  // Features [9+3T - 9+6T]: dist, angle, unum of teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      angleDistToPoint(self_pos, teammate.pos(), th, r);
      addNormFeature(r,0,maxR);
      addNormFeature(th,-M_PI,M_PI);
      addFeature(teammate.unum());
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    addFeature(FEAT_INVALID);
    addFeature(FEAT_INVALID);
    addFeature(FEAT_INVALID);
  }

  // Features [9+6T - 9+6T+3O]: dist, angle, unum of opponents
  int detected_opponents = 0;
  for (PlayerCont::const_iterator it = opponents.begin(); it != opponents.end(); ++it) {
    const PlayerObject& opponent = *it;
    if (valid(opponent) && detected_opponents < numOpponents) {
      angleDistToPoint(self_pos, opponent.pos(), th, r);
      addNormFeature(r,0,maxR);
      addNormFeature(th,-M_PI,M_PI);
      addFeature(opponent.unum());
      detected_opponents++;
    }
  }
  // Add zero features for any missing opponents
  for (int i=detected_opponents; i<numOpponents; ++i) {
    addFeature(FEAT_INVALID);
    addFeature(FEAT_INVALID);
    addFeature(FEAT_INVALID);
  }

  assert(featIndx == numFeatures);
  // checkFeatures();
  return feature_vec;
}
