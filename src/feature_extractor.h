#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <rcsc/player/player_agent.h>
#include <vector>

class FeatureExtractor {
public:
  FeatureExtractor();
  virtual ~FeatureExtractor();

  // Updated the state features stored in feature_vec
  virtual const std::vector<float>& ExtractFeatures(const rcsc::WorldModel& wm) = 0;

  // Record the current state
  void LogFeatures();

  // How many features are there?
  inline int getNumFeatures() { return numFeatures; }

protected:
  // Encodes an angle feature as the sin and cosine of that angle,
  // effectively transforming a single angle into two features.
  void addAngFeature(const rcsc::AngleDeg& ang);

  // Encodes a proximity feature which is defined by a distance as
  // well as a maximum possible distance, which acts as a
  // normalizer. Encodes the distance as [0-far, 1-close]. Ignores
  // distances greater than maxDist or less than 0.
  void addDistFeature(float dist, float maxDist);

  // Add the angle and distance to the landmark to the feature_vec
  void addLandmarkFeatures(const rcsc::Vector2D& landmark,
                           const rcsc::Vector2D& self_pos,
                           const rcsc::AngleDeg& self_ang);

  // Add features corresponding to another player.
  void addPlayerFeatures(rcsc::PlayerObject& player,
                         const rcsc::Vector2D& self_pos,
                         const rcsc::AngleDeg& self_ang);

  // Add a feature without normalizing
  void addFeature(float val);

  // Add a feature and normalize to the range [FEAT_MIN, FEAT_MAX]
  void addNormFeature(float val, float min_val, float max_val);

  // Checks that features are in the range of FEAT_MIN - FEAT_MAX
  void checkFeatures();

protected:
  int featIndx;
  std::vector<float> feature_vec; // Contains the current features
  const static float FEAT_MIN = -1;
  const static float FEAT_MAX = 1;
  int numFeatures; // Total number of features
  // Observed values of some parameters.
  const static float observedSelfSpeedMax   = 0.46;
  const static float observedPlayerSpeedMax = 0.75;
  const static float observedStaminaMax     = 8000.;
  const static float observedBallSpeedMax   = 5.0;
  float maxHFORadius; // Maximum possible distance in HFO playable region
  // Useful measures defined by the Server Parameters
  float pitchLength, pitchWidth, pitchHalfLength, pitchHalfWidth,
    goalHalfWidth, penaltyAreaLength, penaltyAreaWidth;
};

#endif // FEATURE_EXTRACTOR_H
