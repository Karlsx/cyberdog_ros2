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

#ifndef COMMON_PARSER__CAN_PARSER_HPP_
#define COMMON_PARSER__CAN_PARSER_HPP_

#include <set>
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <iostream>

#include "protocol/can/can_utils.hpp"
#include "common_parser/parser_base.hpp"

#define PARSER_NAME_CAN "CAN"

namespace cyberdog
{
namespace common
{
class CanParser : public ParserBase
{
  class VarRuleCan : public VarRuleBase
  {
public:
    explicit VarRuleCan(
      CHILD_STATE_CLCT clct,
      int max_len,
      bool extended,
      const toml::table & table,
      const std::string & out_name)
    : VarRuleBase(clct, max_len, table, out_name, PARSER_NAME_CAN)
    {
      frame_id = HEXtoUINT(toml_at<std::string>(table, "can_id", error_clct), error_clct);
      CanidRangeCheck(frame_id, extended, out_name, var_name, error_clct);
    }
  };  // class VarRuleCan

  class ArrayRuleCan : public ArrayRuleBase
  {
public:
    explicit ArrayRuleCan(
      CHILD_STATE_CLCT clct,
      bool extended,
      const toml::table & table,
      const std::string & out_name)
    : ArrayRuleBase(clct, table, out_name, PARSER_NAME_CAN)
    {
      std::string parser_name = PARSER_NAME_CAN;
      auto tmp_can_id = toml_at<std::vector<std::string>>(table, "can_id", error_clct);
      auto canid_num = tmp_can_id.size();
      if (canid_num == package_num) {
        int index = 0;
        for (auto & single_id : tmp_can_id) {
          canid_t canid = HEXtoUINT(single_id, error_clct);
          CanidRangeCheck(canid, extended, out_name, array_name, error_clct);
          if (frame_map.find(canid) == frame_map.end()) {
            frame_map.insert(std::pair<canid_t, int>(canid, index++));
          } else {
            error_clct->LogState(ErrorCode::RULEARRAY_SAMECANID_ERROR);
            printf(
              C_RED "[%s_PARSER][ERROR][%s][array:%s] array error, "
              "get same can_id:\"0x%x\"\n" C_END,
              parser_name.c_str(), out_name.c_str(), array_name.c_str(), canid);
          }
        }
        // continuous check
        bool first = true;
        canid_t last_id;
        for (auto & id : frame_map) {
          if (first == true) {
            first = false;
            last_id = id.first;
            continue;
          }
          if (id.first - last_id != 1) {
            warn_flag = true;
            printf(
              C_YELLOW "[%s_PARSER][WARN][%s][array:%s] toml_array:\"can_id\" not "
              "continuous increase\n" C_END,
              parser_name.c_str(), out_name.c_str(), array_name.c_str());
            break;
          }
        }
      } else if (package_num > 2 && canid_num == 2) {
        canid_t start_id = HEXtoUINT(tmp_can_id[0], error_clct);
        canid_t end_id = HEXtoUINT(tmp_can_id[1], error_clct);
        if (end_id - start_id + 1 == package_num) {
          int index = 0;
          for (canid_t a = start_id; a <= end_id; a++) {
            frame_map.insert(std::pair<canid_t, int>(a, index++));
          }
        } else {
          error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][array:%s] simple array method must follow: "
            "(canid low to high) and (high - low + 1 == package_num)\n" C_END,
            parser_name.c_str(), out_name.c_str(), array_name.c_str());
        }
      } else {
        error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][array:%s] toml_array:\"can_id\" length not match "
          "package_num, toml_array:\"can_id\" length=%ld but package_num=%ld\n" C_END,
          parser_name.c_str(), out_name.c_str(), array_name.c_str(), canid_num, package_num);
      }
    }
  }; // class ArrayRuleCan

  class CmdRuleCan : public CmdRuleBase
  {
public:
    explicit CmdRuleCan(
      CHILD_STATE_CLCT clct,
      bool extended,
      const toml::table & table,
      const std::string & out_name)
    : CmdRuleBase(clct, table, out_name, PARSER_NAME_CAN)
    {
      frame_id = HEXtoUINT(toml_at<std::string>(table, "can_id", error_clct), error_clct);
      CanidRangeCheck(frame_id, extended, out_name, cmd_name, error_clct);
    }
  };  // class CmdRuleCan

public:
  CanParser(
    CHILD_STATE_CLCT error_clct,
    const toml::value & toml_config,
    const std::string & out_name)
  : ParserBase(error_clct, out_name, PARSER_NAME_CAN)
  {
    canfd_ = toml::find_or<bool>(toml_config, "canfd_enable", false);
    extended_ = toml::find_or<bool>(toml_config, "extended_frame", false);

    auto var_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "var",
      std::vector<toml::table>());
    auto array_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "array",
      std::vector<toml::table>());
    auto cmd_list = toml::find_or<std::vector<toml::table>>(
      toml_config, "cmd",
      std::vector<toml::table>());

    CreateCheck();
    // get var rule
    for (auto & var : var_list) {
      auto rule = VarRuleCan(error_clct_->CreatChild(), CAN_LEN(), extended_, var, out_name_);
      if (rule.error_clct->GetAllStateTimesNum() == 0) {
        auto frame = FrameRuleBase(
          error_clct_->CreatChild(), CAN_LEN(), UINTto8HEX(rule.frame_id), rule.frame_id);
        InitFrame(frame, false);
      }
      InitVar(rule);
    }
    // get array rule
    for (auto & array : array_list) {
      auto rule = ArrayRuleCan(error_clct_->CreatChild(), extended_, array, out_name_);
      for (auto & a : rule.frame_map) {
        auto frame_id = a.first;
        auto frame = FrameRuleBase(
          error_clct_->CreatChild(), CAN_LEN(), UINTto8HEX(frame_id), frame_id);
        InitFrame(frame, false);
      }
      InitArray(rule);
    }
    // get cmd rule
    for (auto & cmd : cmd_list) {
      auto rule = CmdRuleCan(error_clct_->CreatChild(), extended_, cmd, out_name_);
      if (rule.error_clct->GetAllStateTimesNum() == 0) {
        auto frame = FrameRuleBase(
          error_clct_->CreatChild(), CAN_LEN(), UINTto8HEX(rule.frame_id), rule.frame_id);
        InitFrame(frame, false);
      }
      InitCmd(rule);
    }
    ClearCheck();
  }

  uint8_t CAN_LEN() {return canfd_ ? CANFD_MAX_DLEN : CAN_MAX_DLEN;}
  bool IsCanfd() {return canfd_;}

  // return true when finish all package
  bool Decode(
    PROTOCOL_DATA_MAP & protocol_data_map,
    std::shared_ptr<canfd_frame> rx_frame,
    bool & error_flag)
  {
    return DecodeMain(protocol_data_map, rx_frame->can_id, rx_frame->data, error_flag);
  }
  // return true when finish all package
  bool Decode(
    PROTOCOL_DATA_MAP & protocol_data_map,
    std::shared_ptr<can_frame> rx_frame,
    bool & error_flag)
  {
    return DecodeMain(protocol_data_map, rx_frame->can_id, rx_frame->data, error_flag);
  }

  bool Encode(const std::string & CMD, can_frame & tx_frame, const std::vector<uint8_t> & data)
  {
    if (canfd_ == true) {
      error_clct_->LogState(ErrorCode::CAN_MIXUSING_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][cmd:%s] Can't encode std_can via fd_can params\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), CMD.c_str());
      return false;
    }
    tx_frame.can_dlc = CAN_LEN();
    return EncodeCMD(CMD, tx_frame.can_id, tx_frame.data, data);
  }

  bool Encode(const std::string & CMD, canfd_frame & tx_frame, const std::vector<uint8_t> & data)
  {
    if (canfd_ == false) {
      error_clct_->LogState(ErrorCode::CAN_MIXUSING_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][cmd:%s] Can't encode fd_can via std_can params\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), CMD.c_str());
      return false;
    }
    tx_frame.len = CAN_LEN();
    return EncodeCMD(CMD, tx_frame.can_id, tx_frame.data, data);
  }

  bool Encode(
    const PROTOCOL_DATA_MAP & protocol_data_map,
    std::shared_ptr<CanDev> can_op)
  {
    bool error_flag = false;
    canid_t * can_id;
    uint8_t * data;

    can_frame * std_frame = nullptr;
    canfd_frame * fd_frame = nullptr;
    if (canfd_) {
      fd_frame = new canfd_frame;
      fd_frame->len = 64;
      can_id = &fd_frame->can_id;
      data = &fd_frame->data[0];
    } else {
      std_frame = new can_frame;
      std_frame->can_dlc = 8;
      can_id = &std_frame->can_id;
      data = &std_frame->data[0];
    }

    // var encode
    for (auto & parser_var : parser_var_map_) {
      *can_id = parser_var.first;
      for (auto & rule : parser_var.second) {
        EncodeVar(protocol_data_map, rule, data, error_flag);
      }
      // send out
      SendOut(std_frame, fd_frame, can_op, error_flag);
      // clear data buff
      std::memset(data, 0, CAN_LEN());
    }
    // array encode
    for (auto & rule : parser_array_vec_) {
      for (int a = 0; a < static_cast<int>(rule.frame_map.size()); a++) {
        if (!EncodeArray(protocol_data_map, rule, data, a, error_flag)) {break;}
        *can_id = (*rule.frame_send_vec)[a];
        SendOut(std_frame, fd_frame, can_op, error_flag);
      }
    }

    if (canfd_) {delete fd_frame;} else {delete std_frame;}
    return !error_flag;
  }

private:
  bool canfd_;
  bool extended_;

  void SendOut(
    can_frame * std_frame,
    canfd_frame * fd_frame,
    std::shared_ptr<CanDev> can_op,
    bool & error_flag)
  {
    if (canfd_) {
      if (fd_frame == nullptr || can_op->send_can_message(*fd_frame) == false) {
        error_flag = true;
        error_clct_->LogState(ErrorCode::CAN_FD_SEND_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] Send fd_frame error\n" C_END,
          parser_name_.c_str(), out_name_.c_str());
      }
    } else {
      if (std_frame == nullptr || can_op->send_can_message(*std_frame) == false) {
        error_flag = true;
        error_clct_->LogState(ErrorCode::CAN_STD_SEND_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] Send std_frame error\n" C_END,
          parser_name_.c_str(), out_name_.c_str());
      }
    }
  }
};  // class CanParser
}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PARSER__CAN_PARSER_HPP_
