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

#ifndef COMMON_PROTOCOL__UART_PROTOCOL_HPP_
#define COMMON_PROTOCOL__UART_PROTOCOL_HPP_

#include <string>
#include <memory>

#include "common_protocol/common.hpp"
#include "common_protocol/protocol_base.hpp"

namespace cyberdog
{
namespace common
{
template<typename TDataClass>
class UartProtocol : public ProtocolBase<TDataClass>
{
public:
  UartProtocol(
    CHILD_STATE_CLCT error_clct,
    const std::string & out_name,
    const toml::value & toml_config,
    bool for_send)
  {
    this->out_name_ = out_name;
    this->error_clct_ = (error_clct == nullptr) ? std::make_shared<StateCollector>() : error_clct;
    this->for_send_ = for_send;
  }

};


}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PROTOCOL__UART_PROTOCOL_HPP_
