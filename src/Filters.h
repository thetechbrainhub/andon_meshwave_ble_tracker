#ifndef FILTERS_H
#define FILTERS_H

#include <vector>
#include "Config.h"

// Simple Kalman Filter implementation
class KalmanFilter {
private:
  float Q; // Process noise variance
  float R; // Measurement noise variance
  float P; // Estimation error variance
  float K; // Kalman gain
  float X; // Estimated value

public:
  KalmanFilter(float initialValue = 0, float processNoise = PROCESS_NOISE, float measurementNoise = MEASUREMENT_NOISE);
  float update(float measurement);
  float getValue();
  
  // ✅ NEU: Dynamisches Update der Filter-Parameter
  void updateNoise(float newProcessNoise, float newMeasurementNoise) {
    Q = newProcessNoise;
    R = newMeasurementNoise;
  }
};

// Moving Average Filter implementation
class MovingAverageFilter {
private:
  std::vector<float> window;
  int windowSize;
  int currentIndex;
  bool windowFilled;
  float sum;

public:
  MovingAverageFilter(int size = WINDOW_SIZE);
  float update(float newValue);
  float getValue();
};

#endif // FILTERS_H