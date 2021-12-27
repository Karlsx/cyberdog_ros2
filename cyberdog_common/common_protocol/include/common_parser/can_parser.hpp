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
      str_frame_id = UINTto8HEX(frame_id);
    }
  };  // class VarRuleCan

  class ArrayRuleCan
  {
public:
    explicit ArrayRuleCan(
      CHILD_STATE_CLCT clct,
      bool extended,
      const toml::table & table,
      const std::string & out_name,
      const std::string & parser_name)
    {
      error_clct = clct;
      warn_flag = false;
      can_package_num = toml_at<size_t>(table, "can_package_num", error_clct);
      array_name = toml_at<std::string>(table, "array_name", error_clct);
      if (array_name == "") {
        error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_ARRAYNAME);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] array_name error, not support empty string\n" C_END,
          parser_name.c_str(), out_name.c_str());
      }
      auto tmp_can_id = toml_at<std::vector<std::string>>(table, "can_id", error_clct);
      auto canid_num = tmp_can_id.size();
      if (canid_num == can_package_num) {
        int index = 0;
        for (auto & single_id : tmp_can_id) {
          canid_t canid = HEXtoUINT(single_id, error_clct);
          CanidRangeCheck(canid, extended, out_name, array_name, error_clct);
          if (can_id.find(canid) == can_id.end()) {
            can_id.insert(std::pair<canid_t, int>(canid, index++));
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
        for (auto & id : can_id) {
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
      } else if (can_package_num > 2 && canid_num == 2) {
        canid_t start_id = HEXtoUINT(tmp_can_id[0], error_clct);
        canid_t end_id = HEXtoUINT(tmp_can_id[1], error_clct);
        if (end_id - start_id + 1 == can_package_num) {
          int index = 0;
          for (canid_t a = start_id; a <= end_id; a++) {
            can_id.insert(std::pair<canid_t, int>(a, index++));
          }
        } else {
          error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][array:%s] simple array method must follow: "
            "(canid low to high) and (high - low + 1 == can_package_num)\n" C_END,
            parser_name.c_str(), out_name.c_str(), array_name.c_str());
        }
      } else {
        error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][array:%s] toml_array:\"can_id\" length not match "
          "can_package_num, toml_array:\"can_id\" length=%ld but can_package_num=%ld\n" C_END,
          parser_name.c_str(), out_name.c_str(), array_name.c_str(), canid_num, can_package_num);
      }
    }
    CHILD_STATE_CLCT error_clct;
    bool warn_flag;
    size_t can_package_num;
    std::map<canid_t, int> can_id;
    std::string array_name;

    inline int get_offset(canid_t canid)
    {
      if (can_id.find(canid) != can_id.end()) {
        return can_id.at(canid);
      }
      return -1;
    }
  }; // class ArrayRuleBase

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

    CrectCheck();
    // get var rule
    for (auto & var : var_list) {
      auto rule = VarRuleCan(error_clct_->CreatChild(), CAN_LEN(), extended_, var, out_name_);
      InitVar(rule, CAN_LEN());
    }
    // get array rule
    for (auto & array : array_list) {
      auto single_array = ArrayRuleCan(
        error_clct_->CreatChild(), extended_, array, out_name_, PARSER_NAME_CAN);
      if (single_array.warn_flag) {warn_num_++;}
      if (single_array.error_clct->GetSelfStateTimesNum() == 0) {
        // check error and warning
        if (same_var_error(single_array.array_name)) {continue;}
        check_data_area_error(single_array);
        parser_array_.push_back(single_array);
      }
    }
    // get cmd rule
    for (auto & cmd : cmd_list) {
      auto rule = CmdRuleCan(error_clct_->CreatChild(), extended_, cmd, out_name_);
      InitCmd(rule, CAN_LEN());
    }
    ClearCheck();
  }

  uint8_t CAN_LEN() {return canfd_ ? CANFD_MAX_DLEN : CAN_MAX_DLEN;}
  bool IsCanfd() {return canfd_;}

  std::vector<canid_t> GetRecvList()
  {
    auto recv_list = std::vector<canid_t>();
    for (auto & a : parser_var_map_) {recv_list.push_back(a.first);}
    for (auto & a : parser_array_) {for (auto & b : a.can_id) {recv_list.push_back(b.first);}}
    return recv_list;
  }

  // return true when finish all package
  bool Decode(
    PROTOCOL_DATA_MAP & protocol_data_map,
    std::shared_ptr<canfd_frame> rx_frame,
    bool & error_flag)
  {
    return Decode(protocol_data_map, rx_frame->can_id, rx_frame->data, error_flag);
  }
  // return true when finish all package
  bool Decode(
    PROTOCOL_DATA_MAP & protocol_data_map,
    std::shared_ptr<can_frame> rx_frame,
    bool & error_flag)
  {
    return Decode(protocol_data_map, rx_frame->can_id, rx_frame->data, error_flag);
  }

  bool Encode(can_frame & tx_frame, const std::string & CMD, const std::vector<uint8_t> & data)
  {
    if (canfd_ == true) {
      error_clct_->LogState(ErrorCode::CAN_MIXUSING_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][cmd:%s] Can't encode std_can via fd_can params\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), CMD.c_str());
      return false;
    }
    tx_frame.can_dlc = CAN_LEN();
    return Encode(CMD, tx_frame.can_id, tx_frame.data, data);
  }

  bool Encode(canfd_frame & tx_frame, const std::string & CMD, const std::vector<uint8_t> & data)
  {
    if (canfd_ == false) {
      error_clct_->LogState(ErrorCode::CAN_MIXUSING_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][cmd:%s] Can't encode fd_can via std_can params\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), CMD.c_str());
      return false;
    }
    tx_frame.len = CAN_LEN();
    return Encode(CMD, tx_frame.can_id, tx_frame.data, data);
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
        std::string var_name = rule.var_name;
        if (protocol_data_map.find(var_name) == protocol_data_map.end()) {
          error_flag = true;
          error_clct_->LogState(ErrorCode::RUNTIME_NOLINK_ERROR);
          printf(
            C_RED "[%s_PARSER][ERROR][%s] Can't find var_name:\"%s\" in protocol_data_map\n"
            "\tYou may need use LINK_VAR() to link data class/struct in protocol_data_map\n" C_END,
            parser_name_.c_str(), out_name_.c_str(), var_name.c_str());
          continue;
        }
        EncodeVarBase(protocol_data_map.at(var_name), data, rule, error_flag);
      }
      // send out
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
      // clear data buff
      std::memset(data, 0, CAN_LEN());
    }
    // array encode
    int frame_index = 0;
    for (auto & rule : parser_array_) {
      std::string array_name = rule.array_name;
      if (protocol_data_map.find(array_name) == protocol_data_map.end()) {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RUNTIME_NOLINK_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] Can't find array_name:\"%s\" in protocol_data_map\n"
          "\tYou may need use LINK_VAR() to link data class/struct in protocol_data_map\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), array_name.c_str());
        continue;
      }
      const ProtocolData * const var = &protocol_data_map.at(array_name);
      int frame_num = static_cast<int>(rule.can_id.size());
      if (frame_num * CAN_LEN() != var->len) {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] array_name:\"%s\" size not match, "
          "can't write to can frame data for send\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), array_name.c_str());
      }
      auto ids = std::vector<canid_t>(frame_num);
      for (auto & a : rule.can_id) {ids[a.second] = a.first;}
      uint8_t * protocol_array = static_cast<uint8_t *>(var->addr);
      for (int index = frame_index; index < frame_num; index++) {
        *can_id = ids[index];
        for (int a = 0; a < CAN_LEN(); a++) {
          data[a] = *protocol_array;
          protocol_array++;
        }
        // send out
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
    }

    if (canfd_) {delete fd_frame;} else {delete std_frame;}
    return !error_flag;
  }

private:
  bool canfd_;
  bool extended_;

  std::vector<ArrayRuleCan> parser_array_ = std::vector<ArrayRuleCan>();

  // return true when finish all package
  bool Decode(
    PROTOCOL_DATA_MAP & protocol_data_map,
    canid_t can_id,
    uint8_t * raw_data,
    bool & error_flag)
  {
    DecodeVar(protocol_data_map, can_id, raw_data, error_flag);
    // array decode
    for (auto & rule : parser_array_) {
      auto offset = rule.get_offset(can_id);
      if (offset == -1) {continue;}
      if (protocol_data_map.find(rule.array_name) != protocol_data_map.end()) {
        ProtocolData * var = &protocol_data_map.at(rule.array_name);
        if (var->len < rule.can_package_num * CAN_LEN()) {
          error_flag = true;
          error_clct_->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s] array_name:\"%s\" length overflow\n" C_END,
            parser_name_.c_str(), out_name_.c_str(), rule.array_name.c_str());
          continue;
        }
        if (offset == var->array_expect) {
          // main decode begin
          uint8_t * data_area = reinterpret_cast<uint8_t *>(var->addr);
          data_area += offset * CAN_LEN();
          for (int a = 0; a < static_cast<int>(CAN_LEN()); a++) {
            data_area[a] = raw_data[a];
          }
          var->array_expect++;
          if (offset == static_cast<int>(rule.can_package_num) - 1) {
            var->loaded = true;
            var->array_expect = 0;
          }
        } else {
          canid_t expect_id = 0x0;
          for (auto & id : rule.can_id) {
            if (id.second == var->array_expect) {
              expect_id = id.first;
              break;
            }
          }
          var->array_expect = 0;
          error_flag = true;
          error_clct_->LogState(ErrorCode::RUNTIME_UNEXPECT_ORDERPACKAGE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s] array_name:\"%s\", expect can frame 0x%x, "
            "but get 0x%x, reset expect can_id and you need send array in order\n" C_END,
            parser_name_.c_str(), out_name_.c_str(), rule.array_name.c_str(), expect_id, can_id);
        }
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RUNTIME_NOLINK_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] Can't find array_name:\"%s\" in protocol_data_map\n"
          "\tYou may need use LINK_VAR() to link data class/struct in protocol_data_map\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), rule.array_name.c_str());
      }
    }
    return CheckAllFrameRecv(protocol_data_map);
  }

  bool Encode(
    const std::string & CMD,
    canid_t & frame_id,
    uint8_t * can_data,
    const std::vector<uint8_t> & data)
  {
    if (parser_cmd_map_.find(CMD) != parser_cmd_map_.end()) {
      auto & cmd = parser_cmd_map_.at(CMD);
      frame_id = cmd.frame_id;
      uint8_t ctrl_len = cmd.ctrl_len;
      bool no_warn = true;
      if (ctrl_len + data.size() > CAN_LEN()) {
        no_warn = false;
        error_clct_->LogState(ErrorCode::RULEARRAY_ILLEGAL_PARSERPARAM_VALUE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][cmd:%s] CMD data overflow, "
          "ctrl_len:%d + data_len:%ld > max_can_len:%d\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), CMD.c_str(), ctrl_len, data.size(), CAN_LEN());
      }
      for (int a = 0; a < CAN_LEN() && a < static_cast<int>(cmd.ctrl_data.size()); a++) {
        can_data[a] = cmd.ctrl_data[a];
      }
      for (int a = ctrl_len; a < CAN_LEN() && a - ctrl_len < static_cast<int>(data.size()); a++) {
        can_data[a] = data[a - ctrl_len];
      }
      return no_warn;
    } else {
      error_clct_->LogState(ErrorCode::RULECMD_MISSING_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] can't find cmd:\"%s\"\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), CMD.c_str());
      return false;
    }
  }

  void check_data_area_error(ArrayRuleCan & rule)
  {
    for (auto & can_id : rule.can_id) {
      if (data_check_->find(can_id.first) == data_check_->end()) {
        data_check_->insert(
          std::pair<canid_t, std::vector<uint8_t>>(
            can_id.first, std::vector<uint8_t>(CAN_LEN())));
      }
      var_area_error(can_id.first, UINTto8HEX(can_id.first), 0, CAN_LEN() - 1);
    }
  }
};  // class CanParser
}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PARSER__CAN_PARSER_HPP_
