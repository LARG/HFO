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
  numFeatures = num_basic_features + features_per_teammate * numTeammates;
  if (numOpponents > 0) {
    // One extra basic feature and one feature per teammate
    numFeatures += 1 + numTeammates;
  }
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
  // addFeature(self_pos.x);

  // Feature[1]: Y-Position
  addNormFeature(self_pos.y, -SP.pitchHalfWidth() - tolerance_y,
                 SP.pitchHalfWidth() + tolerance_y);
  // addFeature(self_pos.y);

  // Feature[2]: Self Angle
  addNormFeature(self_ang, -M_PI, M_PI); // addFeature(self_ang);

  float r;
  float th;
  // features about the ball
  Vector2D ball_pos = wm.ball().pos();
  angleDistToPoint(self_pos, ball_pos, th, r);
  // Feature[3]: Dist to ball
  addNormFeature(r, 0, maxR); // addFeature(r);
  // Feature[4]: Ang to ball
  addNormFeature(th, -M_PI, M_PI); // addFeature(th);
  // Feature[5]: Able to kick
  addNormFeature(self.isKickable(), false, true); // addFeature(self.isKickable());

  // features about distance to goal center
  Vector2D goalCenter(SP.pitchHalfLength(), 0);
  if (!playingOffense) {
    goalCenter.assign(-SP.pitchHalfLength(), 0);
  }
  angleDistToPoint(self_pos, goalCenter, th, r);
  // Feature[6]: Goal Center Distance
  addNormFeature(r, 0, maxR); // addFeature(r);
  // Feature[7]: Angle to goal center
  addNormFeature(th, -M_PI, M_PI); // addFeature(th);
  // Feature[8]: largest open goal angle
  addNormFeature(calcLargestGoalAngle(wm, self_pos), 0, M_PI);
  // addFeature(calcLargestGoalAngle(wm, self_pos));

  // Features [9+T - 9 + 2T]: teammate's open angle to goal
  int detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestGoalAngle(wm, teammate.pos()), 0, M_PI);
      // addFeature(calcLargestGoalAngle(wm, teammate.pos()));
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    addFeature(FEAT_INVALID);
  }

  // dist to our closest opp
  if (numOpponents > 0) {
    calcClosestOpp(wm, self_pos, th, r);
    addNormFeature(r, 0, maxR); // addFeature(r);

    // teammates dists to closest opps
    detected_teammates = 0;
    for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
      const PlayerObject& teammate = *it;
      if (valid(teammate) && detected_teammates < numTeammates) {
        calcClosestOpp(wm, teammate.pos(), th, r);
        addNormFeature(r, 0, maxR); // addFeature(r);
        detected_teammates++;
      }
    }
    // Add zero features for any missing teammates
    for (int i=detected_teammates; i<numTeammates; ++i) {
      addFeature(FEAT_INVALID);
    }
  }

  // Features [9+2T - 9+3T]: open angle to teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      addNormFeature(calcLargestTeammateAngle(wm, self_pos, teammate.pos()),0,M_PI);
      // addFeature(calcLargestTeammateAngle(wm, self_pos, teammate.pos()));
      detected_teammates++;
    }
  }
  // Add zero features for any missing teammates
  for (int i=detected_teammates; i<numTeammates; ++i) {
    addFeature(FEAT_INVALID);
  }

  // Features [9+3T - end]: dist, angle, unum of teammates
  detected_teammates = 0;
  for (PlayerCont::const_iterator it=teammates.begin(); it != teammates.end(); ++it) {
    const PlayerObject& teammate = *it;
    if (valid(teammate) && detected_teammates < numTeammates) {
      angleDistToPoint(self_pos, teammate.pos(), th, r);
      addNormFeature(r,0,maxR); // addFeature(r);
      addNormFeature(th,-M_PI,M_PI); // addFeature(th);
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

  assert(featIndx == numFeatures);
  // checkFeatures();
  //std::cout << "features: " << features.rows(0,ind-1).t();
  return feature_vec;
}
