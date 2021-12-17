// Copyright (c) 2021 Beijing Xiaomi Mobile Software Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DEVICE_RANGE__TOF_HPP_
#define DEVICE_RANGE__TOF_HPP_

#include <variant>
#include <vector>

#include "common_base/common_type.hpp"
#include "common_base/input_device.hpp"

namespace cyberdog
{
namespace device
{
struct TOFTargetT
{
  uint16_t id;
  uint8_t type;
  uint8_t calibrated;
  PoseT relat_pose;
};
struct TOFCalibT
{
  double lim_min;
  double lim_max;
  double hfov;
  double vfov;
};

typedef std::vector<double> PointsDataT;
typedef std::variant<PointsDataT> TOFDataT;
typedef uint32_t TOFModeT;
typedef uint32_t TOFArgK;

/**
 * @brief TOF is designed for Global Navigation Satellite System devices with serial points data.
 * You will got sequential data from this device after setting callback function.
 * You must initialize device with function init(), set device modules informations, and
 * synchronize data if you need. The synchronization mechanism is up to devices and protocol.
 * After initialization, set callback function, please.
 * Argument Key to Argument Map is designed for calibration test online.
 */
class TOFSensor : public virtual InputDevice
  <TOFTargetT, TOFDataT, TOFModeT, TOFArgK, TOFCalibT, TOFCalibT> {};

}  // namespace device
}  // namespace cyberdog

#endif  // DEVICE_RANGE__TOF_HPP_
