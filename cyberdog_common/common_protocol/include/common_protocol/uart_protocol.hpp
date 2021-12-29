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

#include <vector>
#include <string>
#include <memory>

#include "common_protocol/common.hpp"
#include "common_protocol/protocol_base.hpp"
#include "common_parser/uart_parser.hpp"

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

    uart_parser_ = std::make_shared<UartParser>(
      this->error_clct_->CreatChild(), toml_config, this->out_name_);
    printf(
      "[UART_PROTOCOL][INFO] Creat uart protocol[%s]: %d error, %d warning\n",
      this->out_name_.c_str(), GetInitErrorNum(), GetInitWarnNum());
  }
  ~UartProtocol() {}

  bool Operate(
    const std::string & CMD,
    const std::vector<uint8_t> & data = std::vector<uint8_t>()) override
  {return false;}

  bool SendSelfData() override
  {return false;}

  int GetInitErrorNum() override {return uart_parser_->GetInitErrorNum();}
  int GetInitWarnNum() override {return uart_parser_->GetInitWarnNum();}

  bool IsRxTimeout() override {return false;}
  bool IsTxTimeout() override {return false;}

private:
  std::shared_ptr<UartParser> uart_parser_;
};


}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PROTOCOL__UART_PROTOCOL_HPP_
