#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "feature_extractor.h"
#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <sstream>

using namespace rcsc;

FeatureExtractor::FeatureExtractor() :
    numFeatures(-1)
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
  assert(feature_vec.size() == numFeatures);
  std::stringstream ss;
  for (int i=0; i<numFeatures; ++i) {
    ss << feature_vec[i] << " ";
  }
  elog.addText(Logger::WORLD, "StateFeatures %s", ss.str().c_str());
  elog.flush();
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
    addAngFeature(self_ang - vec_to_landmark.th());
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

void FeatureExtractor::addNormFeature(float val, float min_val, float max_val) {
  assert(featIndx < numFeatures);
  if (val < min_val || val > max_val) {
    std::cout << "Feature " << featIndx << " Violated Feature Bounds: " << val
              << " Expected min/max: [" << min_val << ", " << max_val << "]" << std::endl;
    val = std::min(std::max(val, min_val), max_val);
  }
  feature_vec[featIndx++] = ((val - min_val) / (max_val - min_val))
      * (FEAT_MAX - FEAT_MIN) + FEAT_MIN;
}

void FeatureExtractor::checkFeatures() {
  assert(feature_vec.size() == numFeatures);
  for (int i=0; i<numFeatures; ++i) {
    if (feature_vec[i] < FEAT_MIN || feature_vec[i] > FEAT_MAX) {
      std::cout << "Invalid Feature! Indx:" << i << " Val:" << feature_vec[i] << std::endl;
      exit(1);
    }
  }
}
