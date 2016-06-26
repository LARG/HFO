#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "feature_extractor.h"
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <sstream>

using namespace rcsc;

FeatureExtractor::FeatureExtractor(int num_teammates,
                                   int num_opponents,
                                   bool playing_offense) :
    numFeatures(-1),
    numTeammates(num_teammates),
    numOpponents(num_opponents),
    playingOffense(playing_offense)
{
  const ServerParam& SP = ServerParam::i();

  // Grab the field dimensions
  pitchLength = SP.pitchLength();
  pitchWidth = SP.pitchWidth();
  pitchHalfLength = SP.pitchHalfLength();
  pitchHalfWidth = SP.pitchHalfWidth();
  goalHalfWidth = SP.goalHalfWidth();
  penaltyAreaLength = SP.penaltyAreaLength();
  penaltyAreaWidth = SP.penaltyAreaWidth();

  // Maximum possible radius in HFO
  maxHFORadius = sqrtf(pitchHalfLength * pitchHalfLength +
                       pitchHalfWidth * pitchHalfWidth);
}

FeatureExtractor::~FeatureExtractor() {}

void FeatureExtractor::LogFeatures() {
#ifdef ELOG
  assert(feature_vec.size() == numFeatures);
  std::stringstream ss;
  for (int i=0; i<numFeatures; ++i) {
    ss << feature_vec[i] << " ";
  }
  elog.addText(Logger::WORLD, "StateFeatures %s", ss.str().c_str());
  elog.flush();
#endif
}

void FeatureExtractor::addAngFeature(const rcsc::AngleDeg& ang) {
  addFeature(ang.sin());
  addFeature(ang.cos());
}

void FeatureExtractor::addDistFeature(float dist, float maxDist) {
  float proximity = 1.f - std::max(0.f, std::min(1.f, dist/maxDist));
  addNormFeature(proximity, 0., 1.);
}

// Add the angle and distance to the landmark to the feature_vec
void FeatureExtractor::addLandmarkFeatures(const rcsc::Vector2D& landmark,
                                           const rcsc::Vector2D& self_pos,
                                           const rcsc::AngleDeg& self_ang) {
  if (self_pos == Vector2D::INVALIDATED) {
    addFeature(0);
    addFeature(0);
    addFeature(0);
  } else {
    Vector2D vec_to_landmark = landmark - self_pos;
    addAngFeature(vec_to_landmark.th() - self_ang);
    addDistFeature(vec_to_landmark.r(), maxHFORadius);
  }
}

void FeatureExtractor::addPlayerFeatures(rcsc::PlayerObject& player,
                                         const rcsc::Vector2D& self_pos,
                                         const rcsc::AngleDeg& self_ang) {
  assert(player.posValid());
  // Angle dist to player.
  addLandmarkFeatures(player.pos(), self_pos, self_ang);
  // Player's body angle
  addAngFeature(player.body());
  if (player.velValid()) {
    // Player's speed
    addNormFeature(player.vel().r(), 0., observedPlayerSpeedMax);
    // Player's velocity direction
    addAngFeature(player.vel().th());
  } else {
    addFeature(0);
    addFeature(0);
    addFeature(0);
  }
}

void FeatureExtractor::addFeature(float val) {
  assert(featIndx < numFeatures);
  feature_vec[featIndx++] = val;
}

float FeatureExtractor::normalize(float val, float min_val, float max_val) {
  if (val < min_val || val > max_val) {
    std::cout << "Feature " << featIndx << " Violated Feature Bounds: " << val
              << " Expected min/max: [" << min_val << ", "
              << max_val << "]" << std::endl;
    val = std::min(std::max(val, min_val), max_val);
  }
  return ((val - min_val) / (max_val - min_val))
      * (FEAT_MAX - FEAT_MIN) + FEAT_MIN;
}

float FeatureExtractor::unnormalize(float val, float min_val, float max_val) {
  if (val < FEAT_MIN || val > FEAT_MAX) {
    std::cout << "Unnormalized value Violated Feature Bounds: " << val
              << " Expected min/max: [" << FEAT_MIN << ", "
              << FEAT_MAX << "]" << std::endl;
    float ft_max = FEAT_MAX; // Linker error on OSX otherwise...?
    float ft_min = FEAT_MIN;
    val = std::min(std::max(val, ft_min), ft_max);
  }
  return ((val - FEAT_MIN) / (FEAT_MAX - FEAT_MIN))
      * (max_val - min_val) + min_val;
}

void FeatureExtractor::addNormFeature(float val, float min_val, float max_val) {
  assert(featIndx < numFeatures);
  feature_vec[featIndx++] = normalize(val, min_val, max_val);
}

void FeatureExtractor::checkFeatures() {
  assert(feature_vec.size() == numFeatures);
  for (int i=0; i<numFeatures; ++i) {
    if (feature_vec[i] == FEAT_INVALID) {
      continue;
    }
    if (feature_vec[i] < FEAT_MIN || feature_vec[i] > FEAT_MAX) {
      std::cout << "Invalid Feature! Indx:" << i << " Val:" << feature_vec[i] << std::endl;
      exit(1);
    }
  }
}

bool FeatureExtractor::valid(const rcsc::PlayerObject& player) {
  const rcsc::Vector2D& pos = player.pos();
  if (!player.posValid()) {
    return false;
  }
  return pos.isValid();
}

float FeatureExtractor::angleToPoint(const rcsc::Vector2D &self,
                                     const rcsc::Vector2D &point) {
  return (point - self).th().radian();
}

void FeatureExtractor::angleDistToPoint(const rcsc::Vector2D &self,
                                        const rcsc::Vector2D &point,
                                        float &ang, float &dist) {
  Vector2D d = point - self;
  ang = d.th().radian();
  dist = d.r();
}

float FeatureExtractor::angleBetween3Points(const rcsc::Vector2D &point1,
                                            const rcsc::Vector2D &centerPoint,
                                            const rcsc::Vector2D &point2) {
  Vector2D diff1 = point1 - centerPoint;
  Vector2D diff2 = point2 - centerPoint;
  float angle1 = atan2(diff1.y,diff1.x);
  float angle2 = atan2(diff2.y,diff2.x);
  return fabs(angle1 - angle2);
}

void FeatureExtractor::calcClosestOpp(const rcsc::WorldModel &wm,
                                      const rcsc::Vector2D &point,
                                      float &ang, float &minDist) {
  minDist = std::numeric_limits<float>::max();
  const PlayerCont& opps = wm.opponents();
  for (PlayerCont::const_iterator it=opps.begin(); it != opps.end(); ++it) {
    const PlayerObject& opponent = *it;
    if (valid(opponent)) {
      float dist;
      float th;
      angleDistToPoint(point, opponent.pos(), th, dist);
      if (dist < minDist) {
        minDist = dist;
        ang = th;
      }
    }
  }
}

float FeatureExtractor::calcLargestTeammateAngle(const rcsc::WorldModel &wm,
                                                 const rcsc::Vector2D &self,
                                                 const Vector2D &teammate) {
  float angTeammate = angleToPoint(self, teammate);
  float angTop = angTeammate + M_PI / 4;
  float angBot = angTeammate - M_PI / 4;
  return calcLargestOpenAngle(wm, self, angTop, angBot, (self - teammate).r());
}

float FeatureExtractor::calcLargestGoalAngle(const rcsc::WorldModel &wm,
                                             const rcsc::Vector2D &self) {
  const rcsc::ServerParam & SP = rcsc::ServerParam::i();
  Vector2D goalPostTop(SP.pitchHalfLength(), SP.goalHalfWidth());
  Vector2D goalPostBot(SP.pitchHalfLength(), -SP.goalHalfWidth());
  float angTop = angleToPoint(self, goalPostTop);
  float angBot = angleToPoint(self, goalPostBot);
  //std::cout << "starting: " << RAD_T_DEG * angTop << " " << RAD_T_DEG * angBot << std::endl;
  float res = calcLargestOpenAngle(wm, self, angTop, angBot, 99999);
  //std::cout << angTop << " " << angBot << " | " << res << std::endl;
  return res;
}

float FeatureExtractor::calcLargestOpenAngle(const rcsc::WorldModel &wm,
                                             const rcsc::Vector2D &self,
                                             float angTop, float angBot,
                                             float maxDist) {
  const rcsc::ServerParam & SP = rcsc::ServerParam::i();
  std::vector<OpenAngle> openAngles;
  openAngles.push_back(OpenAngle(angBot,angTop));
  const PlayerCont& opps = wm.opponents();
  for (PlayerCont::const_iterator it=opps.begin(); it != opps.end(); ++it) {
    const PlayerObject& opp = *it;
    if (valid(opp)) {
      float oppAngle, oppDist;
      angleDistToPoint(self, opp.pos(), oppAngle, oppDist);
      // theta = arctan (opponentWidth / opponentDist)
      float halfWidthAngle = atan2(SP.defaultKickableArea() * 0.5, oppDist);
      //float oppAngleBottom = oppAngle;
      //float oppAngleTop = oppAngle;
      float oppAngleBottom = oppAngle - halfWidthAngle;
      float oppAngleTop = oppAngle + halfWidthAngle;
      // std::cout << "    to split? " << oppDist << " " << maxDist << std::endl;
      if (oppDist < maxDist) {
        splitAngles(openAngles,oppAngleBottom,oppAngleTop);
      }
    }
  }
  float largestOpening = 0;
  for (uint i = 0; i < openAngles.size(); ++i) {
    OpenAngle &open = openAngles[i];
    // std::cout << "  opening: " << RAD_T_DEG * open.first << " " << RAD_T_DEG * open.second << std::endl;
    float opening = open.second - open.first;
    if (opening > largestOpening) {
      largestOpening = opening;
    }
  }
  return largestOpening;
}

void FeatureExtractor::splitAngles(std::vector<OpenAngle> &openAngles,
                                   float oppAngleBottom, float oppAngleTop) {
  std::vector<OpenAngle> resAngles;
  for (uint i = 0; i < openAngles.size(); ++i) {
    OpenAngle& open = openAngles[i];
    if ((oppAngleTop < open.first) || (oppAngleBottom > open.second)) {
      resAngles.push_back(open);
    } else {
      resAngles.push_back(OpenAngle(open.first, oppAngleBottom));
      resAngles.push_back(OpenAngle(oppAngleTop, open.second));
    }
  }
  openAngles = resAngles;
}

float FeatureExtractor::normalizedXPos(float absolute_x_pos) {
  float tolerance_x = .1 * pitchHalfLength;
  if (playingOffense) {
    return normalize(absolute_x_pos, -tolerance_x, pitchHalfLength + tolerance_x);
  } else {
    return normalize(absolute_x_pos, -pitchHalfLength-tolerance_x, tolerance_x);
  }
}

float FeatureExtractor::normalizedYPos(float absolute_y_pos) {
  float tolerance_y = .1 * pitchHalfWidth;
  return normalize(absolute_y_pos, -pitchHalfWidth - tolerance_y,
                   pitchHalfWidth + tolerance_y);
}

float FeatureExtractor::absoluteXPos(float normalized_x_pos) {
  float tolerance_x = .1 * pitchHalfLength;
  if (playingOffense) {
    return unnormalize(normalized_x_pos, -tolerance_x, pitchHalfLength + tolerance_x);
  } else {
    return unnormalize(normalized_x_pos, -pitchHalfLength-tolerance_x, tolerance_x);
  }
}

float FeatureExtractor::absoluteYPos(float normalized_y_pos) {
  float tolerance_y = .1 * pitchHalfWidth;
  return unnormalize(normalized_y_pos, -pitchHalfWidth - tolerance_y,
                     pitchHalfWidth + tolerance_y);
}
