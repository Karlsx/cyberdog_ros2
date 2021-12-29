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

#ifndef COMMON_PARSER__UART_PARSER_HPP_
#define COMMON_PARSER__UART_PARSER_HPP_

#include <string>
#include <vector>
#include <utility>
#include <functional>

#include "common_parser/parser_base.hpp"

#define PARSER_NAME_UART "UART"

namespace cyberdog
{
namespace common
{
class UartParser : public ParserBase
{
  class FrameRuleUart : public FrameRuleBase
  {
public:
    explicit FrameRuleUart(
      CHILD_STATE_CLCT clct,
      const toml::table & table,
      const std::string & out_name)
    : FrameRuleBase(clct, table, out_name, PARSER_NAME_UART)
    {}
  };  // class FrameRuleUart

  class VarRuleUart : public VarRuleBase
  {
public:
    explicit VarRuleUart(
      CHILD_STATE_CLCT clct,
      T_FRAMEID id,
      int max_len,
      const toml::table & table,
      const std::string & out_name)
    : VarRuleBase(clct, max_len, table, out_name, PARSER_NAME_UART)
    {
      frame_id = id;
    }
  };  // class VarRuleUart

  class ArrayRuleUart : public ArrayRuleBase
  {
public:
    ArrayRuleUart(
      CHILD_STATE_CLCT clct,
      const toml::table & table,
      const std::string & out_name,
      std::function<bool(const std::string &, T_FRAMEID &)> GetFrameID)
    : ArrayRuleBase(clct, table, out_name, PARSER_NAME_UART)
    {
      std::string parser_name = PARSER_NAME_UART;
      auto tmp_frame_id = toml_at<std::vector<std::string>>(table, "frame_id", error_clct);
      auto frameid_num = tmp_frame_id.size();
      if (frameid_num == package_num) {
        int index = 0;
        for (auto & single_id : tmp_frame_id) {
          T_FRAMEID frame_id;
          if (!GetFrameID(single_id, frame_id)) {continue;}
          if (frame_map.find(frame_id) == frame_map.end()) {
            frame_map.insert(std::pair<T_FRAMEID, int>(frame_id, index++));
          } else {
            error_clct->LogState(ErrorCode::RULEARRAY_SAMECANID_ERROR);
            printf(
              C_RED "[%s_PARSER][ERROR][%s][array:%s] array error, "
              "get same frame_id:\"%s\"\n" C_END,
              parser_name.c_str(), out_name.c_str(), array_name.c_str(), single_id.c_str());
          }
        }
      } else {
        error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][array:%s] toml_array:\"frame_id\" length not match "
          "package_num, toml_array:\"frame_id\" length=%ld but package_num=%ld\n" C_END,
          parser_name.c_str(), out_name.c_str(), array_name.c_str(), frameid_num, package_num);
      }
    }
  };  // class ArrayRuleUart

  class CmdRuleUart : public CmdRuleBase
  {
public:
    explicit CmdRuleUart(
      CHILD_STATE_CLCT clct,
      T_FRAMEID id,
      const toml::table & table,
      const std::string & out_name)
    : CmdRuleBase(clct, table, out_name, PARSER_NAME_UART)
    {
      frame_id = id;
    }
  };  // class CmdRuleUart

public:
  UartParser(
    CHILD_STATE_CLCT error_clct,
    const toml::value & toml_config,
    const std::string & out_name)
  : ParserBase(error_clct, out_name, PARSER_NAME_UART)
  {
    auto frame_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "frame", std::vector<toml::table>());
    auto var_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "var", std::vector<toml::table>());
    auto array_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "array", std::vector<toml::table>());
    auto cmd_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "cmd", std::vector<toml::table>());

    CreateCheck();
    // get frame rule
    for (auto & frame : frame_list) {
      auto rule = FrameRuleUart(error_clct_->CreatChild(), frame, out_name_);
      InitFrame(rule);
    }
    // get var rule
    for (auto & var : var_list) {
      T_FRAMEID frame_id;
      auto frame_name = toml_at<std::string>(var, "frame", error_clct_);
      if (GetFrameID(frame_name, frame_id)) {
        auto rule = VarRuleUart(
          error_clct_->CreatChild(), frame_id, GetFrameDataLen(frame_id), var, out_name_);
        InitVar(rule);
      }
    }
    // get array rule
    for (auto & single_array : array_list) {
      auto rule = ArrayRuleUart(
        error_clct_->CreatChild(), single_array, out_name_,
        std::bind(&UartParser::GetFrameID, this, std::placeholders::_1, std::placeholders::_2));
      InitArray(rule);
    }
    // get cmd rule
    for (auto & cmd : cmd_list) {
      T_FRAMEID frame_id;
      auto frame_name = toml_at<std::string>(cmd, "frame", error_clct_);
      if (GetFrameID(frame_name, frame_id)) {
        auto rule = CmdRuleUart(error_clct_->CreatChild(), frame_id, cmd, out_name_);
        InitCmd(rule);
      }
    }
    ClearCheck();
  }
};  // class UartParser

}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PARSER__UART_PARSER_HPP_
