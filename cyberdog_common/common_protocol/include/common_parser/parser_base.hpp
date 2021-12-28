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

#ifndef COMMON_PARSER__PARSER_BASE_HPP_
#define COMMON_PARSER__PARSER_BASE_HPP_

#include <set>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "toml11/toml.hpp"
#include "common_protocol/common.hpp"

namespace cyberdog
{
namespace common
{
#define T_FRAMEID uint32_t

class FrameRuleBase
{
public:
  explicit FrameRuleBase(
    CHILD_STATE_CLCT clct,
    const toml::table & table,
    const std::string & out_name,
    const std::string & parser_name)
  {
    error_clct = clct;
    warn_flag = false;
    frame_id = all_frame_num++;
    data_len = toml_at<size_t>(table, "data_len", error_clct);
    frame_name = toml_at<std::string>(table, "frame_name", error_clct);

    if (frame_name == "") {
      error_clct->LogState(ErrorCode::RULEFRAME_ILLEGAL_FRAMENAME);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] frame_name error, not support empty string\n" C_END,
        parser_name.c_str(), out_name.c_str());
    }
  }

  explicit FrameRuleBase(CHILD_STATE_CLCT clct, size_t len, std::string name, T_FRAMEID id)
  {
    error_clct = clct;
    warn_flag = false;
    data_len = len;
    frame_name = name;
    frame_id = id;
  }

  CHILD_STATE_CLCT error_clct;
  bool warn_flag;
  size_t data_len;
  T_FRAMEID frame_id;
  std::string frame_name;

private:
  inline static uint all_frame_num;
};  // class FrameRuleBase

class VarRuleBase
{
public:
  explicit VarRuleBase(
    CHILD_STATE_CLCT clct,
    int max_len,
    const toml::table & table,
    const std::string & out_name,
    const std::string & parser_name)
  {
    error_clct = clct;
    warn_flag = false;
    var_name = toml_at<std::string>(table, "var_name", error_clct);
    var_type = toml_at<std::string>(table, "var_type", error_clct);

    if (var_name == "") {
      error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_VARNAME);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] var_name error, not support empty string\n" C_END,
        parser_name.c_str(), out_name.c_str());
    }
    if (common_type.find(var_type) == common_type.end()) {
      error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_VARTYPE);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][var:%s] var_type error, type:\"%s\" not support; "
        "only support:[",
        parser_name.c_str(), out_name.c_str(), var_name.c_str(), var_type.c_str());
      for (auto & t : common_type) {
        printf("%s, ", t.c_str());
      }
      printf("]\n" C_END);
    }
    if (table.find("var_zoom") == table.end()) {var_zoom = 1.0;} else {
      if (var_type != "float" && var_type != "double") {
        warn_flag = true;
        printf(
          C_YELLOW "[%s_PARSER][WARN][%s][var:%s] Only double/float need var_zoom\n" C_END,
          parser_name.c_str(), out_name.c_str(), var_name.c_str());
      }
      var_zoom = toml_at<float>(table, "var_zoom", error_clct);
    }
    parser_type = toml_at<std::string>(table, "parser_type", "auto");
    auto tmp_parser_param = toml_at<std::vector<uint8_t>>(table, "parser_param", error_clct);
    size_t param_size = tmp_parser_param.size();
    if (parser_type == "auto") {
      if (param_size == 3) {
        parser_type = "bit";
      } else if (param_size == 2) {
        parser_type = "var";
      } else {
        error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_SIZE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][var:%s] Can't get parser_type via parser_param, "
          "only param_size == 2 or 3, but get param_size = %ld\n" C_END,
          parser_name.c_str(), out_name.c_str(), var_name.c_str(), param_size);
      }
    }
    if (parser_type == "bit") {
      if (var_type == "u8_array") {
        error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_VARTYPE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][var:%s] parser_type:\"bit\" not support mixed with "
          "var_type:\"u8_array\"\n" C_END,
          parser_name.c_str(), out_name.c_str(), var_name.c_str());
      }
      if (param_size == 3) {
        parser_param[0] = tmp_parser_param[0];
        parser_param[1] = tmp_parser_param[1];
        parser_param[2] = tmp_parser_param[2];
        if (parser_param[0] >= max_len) {
          error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"bit\" parser_param error, "
            "parser_param[0] value need between 0-%d\n" C_END,
            parser_name.c_str(), out_name.c_str(), var_name.c_str(), max_len - 1);
        }
        if (parser_param[1] < parser_param[2]) {
          error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"bit\" parser_param error, "
            "parser_param[1] need >= parser_param[2]\n" C_END,
            parser_name.c_str(), out_name.c_str(), var_name.c_str());
        }
        if (parser_param[1] >= 8 || parser_param[2] >= 8) {
          error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"bit\" parser_param error, "
            "parser_param[1] and parser_param[2] value need between 0-7\n" C_END,
            parser_name.c_str(), out_name.c_str(), var_name.c_str());
        }
      } else {
        error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_SIZE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"bit\" parser error, "
          "parser[bit] need 3 parser_param, but get %d\n" C_END,
          parser_name.c_str(), out_name.c_str(), var_name.c_str(),
          static_cast<uint8_t>(param_size));
      }
    } else if (parser_type == "var") {
      if (param_size == 2) {
        parser_param[0] = tmp_parser_param[0];
        parser_param[1] = tmp_parser_param[1];
        if (parser_param[0] > parser_param[1]) {
          error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"var\" parser_param error, "
            "parser_param[0] need <= parser_param[1]\n" C_END,
            parser_name.c_str(), out_name.c_str(), var_name.c_str());
        }
        if (parser_param[0] >= max_len || parser_param[1] >= max_len) {
          error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_VALUE);
          printf(
            C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"var\" parser_param error, "
            "parser_param[0] and parser_param[1] value need between 0-%d\n" C_END,
            parser_name.c_str(), out_name.c_str(), var_name.c_str(), max_len - 1);
        }
      } else {
        error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERPARAM_SIZE);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"var\" parser error, "
          "parser[var] need 2 parser_param, but get %d\n" C_END,
          parser_name.c_str(), out_name.c_str(), var_name.c_str(),
          static_cast<uint8_t>(param_size));
      }
    } else if (parser_type != "auto") {
      error_clct->LogState(ErrorCode::RULEVAR_ILLEGAL_PARSERTYPE);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][var:%s] type:\"var\" parser error, "
        "only support \"bit/var\", but get %s\n" C_END,
        parser_name.c_str(), out_name.c_str(), var_name.c_str(), parser_type.c_str());
    }
  }

  CHILD_STATE_CLCT error_clct;
  bool warn_flag;
  float var_zoom;
  T_FRAMEID frame_id;
  std::string var_name;
  std::string var_type;
  std::string parser_type;
  uint8_t parser_param[3];
};  // class VarRuleBase

class ArrayRuleBase
{
public:
  explicit ArrayRuleBase(
    CHILD_STATE_CLCT clct,
    const toml::table & table,
    const std::string & out_name,
    const std::string & parser_name)
  {
    array_expect = 0;
    waiting_index = 0;
    error_clct = clct;
    warn_flag = false;
    package_num = toml_at<size_t>(table, "package_num", error_clct);
    array_name = toml_at<std::string>(table, "array_name", error_clct);
    if (array_name == "") {
      error_clct->LogState(ErrorCode::RULEARRAY_ILLEGAL_ARRAYNAME);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] array_name error, not support empty string\n" C_END,
        parser_name.c_str(), out_name.c_str());
    }
  }

  CHILD_STATE_CLCT error_clct;
  bool warn_flag;
  int array_expect;
  size_t all_max_len;
  size_t package_num;
  size_t waiting_index;
  std::string array_name;
  std::map<T_FRAMEID, int> frameID_map;

  inline int get_offset(T_FRAMEID frame_id)
  {
    if (frameID_map.find(frame_id) != frameID_map.end()) {
      return frameID_map.at(frame_id);
    }
    return -1;
  }
};  // class ArrayRuleBase

class CmdRuleBase
{
public:
  explicit CmdRuleBase(
    CHILD_STATE_CLCT clct,
    const toml::table & table,
    const std::string & out_name,
    const std::string & parser_name)
  {
    error_clct = clct;
    warn_flag = false;
    cmd_name = toml_at<std::string>(table, "cmd_name", error_clct);
    if (cmd_name == "") {
      error_clct->LogState(ErrorCode::RULECMD_ILLEGAL_CMDNAME);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] cmd_name error, not support empty string\n" C_END,
        parser_name.c_str(), out_name.c_str());
    }
    ctrl_len = toml_at<uint8_t>(table, "ctrl_len", 0);
    auto tmp_ctrl_data = toml_at<std::vector<std::string>>(table, "ctrl_data");
    ctrl_data = std::vector<uint8_t>();
    for (auto & str : tmp_ctrl_data) {
      auto uint_hex = HEXtoUINT(str, error_clct);
      if (uint_hex != (uint_hex & 0xFF)) {
        error_clct->LogState(ErrorCode::RULECMD_CTRLDATA_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s][cmd:%s] ctrl_data HEX to uint8 overflow, "
          "HEX_string:\"%s\"\n" C_END,
          parser_name.c_str(), out_name.c_str(), cmd_name.c_str(), str.c_str());
      }
      ctrl_data.push_back(static_cast<uint8_t>(uint_hex & 0xFF));
    }
    int size = ctrl_data.size();
    if (ctrl_len < size) {
      error_clct->LogState(ErrorCode::RULECMD_CTRLDATA_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s][cmd:%s] ctrl_data overflow, "
        "ctrl_len:%d < ctrl_data.size:%d\n" C_END,
        parser_name.c_str(), out_name.c_str(), cmd_name.c_str(), ctrl_len, size);
    }
  }

  CHILD_STATE_CLCT error_clct;
  bool warn_flag;
  uint8_t ctrl_len;
  T_FRAMEID frame_id;
  std::string cmd_name;
  std::vector<uint8_t> ctrl_data;
};  // class CmdRuleBase

class ParserBase
{
public:
  ParserBase(
    CHILD_STATE_CLCT error_clct,
    const std::string & out_name,
    const std::string & parser_name)
  {
    out_name_ = out_name;
    parser_name_ = parser_name;
    error_clct_ = (error_clct == nullptr) ? std::make_shared<StateCollector>() : error_clct;
  }

  int GetInitErrorNum() {return error_clct_->GetAllStateTimesNum();}
  int GetInitWarnNum() {return warn_num_;}

protected:
  int warn_num_ = 0;
  std::string out_name_;
  std::string parser_name_;
  CHILD_STATE_CLCT error_clct_;

  std::map<T_FRAMEID, FrameRuleBase> parser_frame_map_ = std::map<T_FRAMEID, FrameRuleBase>();
  std::map<T_FRAMEID, std::vector<VarRuleBase>> parser_var_map_ =
    std::map<T_FRAMEID, std::vector<VarRuleBase>>();
  std::map<std::string, CmdRuleBase> parser_cmd_map_ =
    std::map<std::string, CmdRuleBase>();

  // for check
  std::shared_ptr<std::set<std::string>> var_name_check_ = nullptr;
  std::shared_ptr<std::map<T_FRAMEID, std::vector<uint8_t>>> data_check_ = nullptr;

  // Init ////////////////////////////////////////////////////////////////////////////////////////

  void InitFrame(FrameRuleBase & rule, bool check_id = true)
  {
    if (rule.warn_flag) {warn_num_++;}
    if (rule.error_clct->GetSelfStateTimesNum() == 0) {
      T_FRAMEID frame_id = rule.frame_id;
      if (parser_frame_map_.find(frame_id) == parser_frame_map_.end()) {
        parser_frame_map_.insert(std::pair<T_FRAMEID, FrameRuleBase>(frame_id, rule));
      } else if (check_id) {
        error_clct_->LogState(ErrorCode::RULEFRAME_SAMEFRAMEID_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] get same frame_id:\"%x\"\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), frame_id);
      }
    }
  }

  void InitVar(VarRuleBase & rule)
  {
    if (rule.warn_flag) {warn_num_++;}
    if (rule.error_clct->GetSelfStateTimesNum() == 0) {
      T_FRAMEID frame_id = rule.frame_id;
      if (parser_var_map_.find(frame_id) == parser_var_map_.end()) {
        parser_var_map_.insert(
          std::pair<T_FRAMEID, std::vector<VarRuleBase>>(frame_id, std::vector<VarRuleBase>()));
      }
      // check error and warning
      if (IsSameVarError(rule.var_name)) {return;}
      CheckDataConflict(rule, GetFrameDataLen(rule.frame_id));  // report error but get pass
      parser_var_map_.at(frame_id).push_back(rule);
    }
  }

  void InitArray(ArrayRuleBase & rule)
  {

  }

  void InitCmd(CmdRuleBase & rule)
  {
    if (rule.warn_flag) {warn_num_++;}
    if (rule.error_clct->GetSelfStateTimesNum() == 0) {
      auto data_len = GetFrameDataLen(rule.frame_id);
      if (rule.ctrl_len > data_len) {
        error_clct_->LogState(ErrorCode::RULECMD_CTRLDATA_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] cmd_name:\"%s\", ctrl_len:%d > MAX_DATA_LEN:%ld\n" C_END,
          parser_name_.c_str(), out_name_.c_str(),
          rule.cmd_name.c_str(), rule.ctrl_len, data_len);
        return;
      }
      std::string cmd_name = rule.cmd_name;
      if (parser_cmd_map_.find(cmd_name) == parser_cmd_map_.end()) {
        parser_cmd_map_.insert(std::pair<std::string, CmdRuleBase>(cmd_name, rule));
      } else {
        error_clct_->LogState(ErrorCode::RULECMD_SAMECMD_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] get same cmd_name:\"%s\"\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), cmd_name.c_str());
      }
    }
  }

  // Tools //////////////////////////////////////////////////////

  size_t GetFrameDataLen(T_FRAMEID frame_id)
  {
    if (parser_frame_map_.find(frame_id) != parser_frame_map_.end()) {
      return parser_frame_map_.at(frame_id).data_len;
    } else {
      error_clct_->LogState(ErrorCode::RUNTIME_NOFRAMEID_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] Can't find frame_id:\"%x\"\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), frame_id);
    }
    return 0;
  }

  std::string GetFrameName(T_FRAMEID frame_id)
  {
    if (parser_frame_map_.find(frame_id) != parser_frame_map_.end()) {
      return parser_frame_map_.at(frame_id).frame_name;
    } else {
      error_clct_->LogState(ErrorCode::RUNTIME_NOFRAMEID_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] Can't find frame_id:\"%x\"\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), frame_id);
    }
    return "#unknow";
  }

  uint8_t CreatMask(uint8_t h_bit, uint8_t l_bit)
  {
    uint8_t tmp = 0x1;
    uint8_t mask = 0x0;
    for (int a = 0; a < 8; a++) {
      if (l_bit <= a && a <= h_bit) {mask |= tmp;}
      tmp <<= 1;
    }
    return mask;
  }

  // Check ///////////////////////////////////////////////////////////////////////////////////////
  void CreateCheck()
  {
    data_check_ = std::make_shared<std::map<T_FRAMEID, std::vector<uint8_t>>>();
    var_name_check_ = std::make_shared<std::set<std::string>>();
  }

  void ClearCheck()
  {
    data_check_ = nullptr;
    var_name_check_ = nullptr;
  }

  std::string ShowConflict(uint8_t mask)
  {
    uint8_t tmp = 0b10000000;
    std::string result = "";
    for (int a = 0; a < 8; a++) {
      result += ((mask & tmp) != 0) ? ("*") : ("-");
      tmp >>= 1;
    }
    return result;
  }

  bool IsSameVarError(std::string & name)
  {
    if (var_name_check_->find(name) != var_name_check_->end()) {
      error_clct_->LogState(ErrorCode::RULE_SAMENAME_ERROR);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] get same var_name:\"%s\"\n" C_END,
        parser_name_.c_str(), out_name_.c_str(), name.c_str());
      return true;
    } else {var_name_check_->insert(name);}
    return false;
  }

  void CheckVarConflict(const VarRuleBase & rule)
  {
    CheckVarConflict(rule.frame_id, rule.parser_param[0], rule.parser_param[1]);
  }

  void CheckVarConflict(
    const T_FRAMEID & frame_id,
    int data_l,
    int data_h)
  {
    uint8_t mask = 0xFF;
    uint8_t conflict = 0x00;
    bool first_index = true;
    for (int index = data_l; index <= data_h; index++) {
      conflict = data_check_->at(frame_id)[index] & mask;
      if (conflict != 0x0) {
        if (first_index) {
          first_index = false;
          error_clct_->LogState(ErrorCode::DATA_AREA_CONFLICT);
          printf(
            C_RED "[%s_PARSER][ERROR][%s] data area decode/encode many times at pos'*':\n"
            " FrameID[%s]:\n\tDATA[%d]%s\n" C_END,
            parser_name_.c_str(), out_name_.c_str(), GetFrameName(frame_id).c_str(), index,
            ShowConflict(conflict).c_str());
        } else {
          printf(
            C_RED "\tDATA[%d]%s\n" C_END,
            index, ShowConflict(mask).c_str());
        }
      }
      data_check_->at(frame_id)[index] |= mask;
    }
  }

  void CheckDataConflict(VarRuleBase & rule, size_t raw_data_len)
  {
    if (data_check_->find(rule.frame_id) == data_check_->end()) {
      data_check_->insert(
        std::pair<T_FRAMEID, std::vector<uint8_t>>(
          rule.frame_id, std::vector<uint8_t>(raw_data_len)));
    }

    if (rule.parser_type == "bit") {
      uint8_t data_index = rule.parser_param[0];
      uint8_t mask = CreatMask(rule.parser_param[1], rule.parser_param[2]);
      uint8_t conflict = data_check_->at(rule.frame_id)[data_index] & mask;
      if (conflict != 0x0) {
        error_clct_->LogState(ErrorCode::DATA_AREA_CONFLICT);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] data area decode/encode many times at pos'*':\n"
          " FrameID[%s]:\n\tDATA[%d]%s\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), GetFrameName(rule.frame_id).c_str(), data_index,
          ShowConflict(conflict).c_str());
      }
      data_check_->at(rule.frame_id)[data_index] |= mask;
    } else if (rule.parser_type == "var") {
      CheckVarConflict(rule);
    }
  }

  bool CheckAllFrameRecv(PROTOCOL_DATA_MAP & protocol_data_map)
  {
    for (auto & single_var : protocol_data_map) {
      if (single_var.second.loaded == false) {return false;}
    }
    for (auto & single_var : protocol_data_map) {
      single_var.second.loaded = false;
    }
    return true;
  }

  // Encode //////////////////////////////////////////////////////////////////////////////////////
  template<typename Target>
  inline void PutVar(
    const ProtocolData & var,
    uint8_t * raw_data,
    const VarRuleBase & rule,
    const std::string & out_name,
    bool & error_flag)
  {
    PutVar<Target, Target>(var, raw_data, rule, out_name, error_flag);
  }

  template<typename Target, typename Source>
  void PutVar(
    const ProtocolData & var,
    uint8_t * raw_data,
    const VarRuleBase & rule,
    const std::string & out_name,
    bool & error_flag)
  {
    if (rule.parser_type == "bit") {
      uint8_t h_pos = rule.parser_param[1];
      uint8_t l_pos = rule.parser_param[2];
      raw_data[rule.parser_param[0]] |=
        (*static_cast<uint8_t *>(var.addr) << l_pos) & CreatMask(h_pos, l_pos);
    } else if (rule.parser_type == "var") {
      uint8_t u8_num = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (sizeof(Target) != u8_num) {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RUNTIME_SIZENOTMATCH);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] var_name:\"%s\" size not match, Target need:%ld - get:%d"
          ", can't write data for send\n" C_END,
          parser_name_.c_str(), out_name.c_str(), rule.var_name.c_str(), sizeof(Target), u8_num);
      } else {
        Target target;
        if (rule.var_type == "float" || rule.var_type == "double") {
          target = static_cast<Target>(*static_cast<Source *>(var.addr) / rule.var_zoom);
        } else {target = static_cast<Target>(*static_cast<Source *>(var.addr));}
        uint64_t * hex = reinterpret_cast<uint64_t *>(&target);
        uint8_t min = rule.parser_param[0];
        uint8_t max = rule.parser_param[1];
        for (int a = min; a <= max; a++) {
          raw_data[a] = (*hex >> (max - a) * 8) & 0xFF;
        }
      }
    }
  }

  void EncodeVarBase(
    const ProtocolData & single_protocol_data,
    uint8_t * raw_data,
    const VarRuleBase & rule,
    bool & error_flag)
  {
    if (rule.var_type == "double") {
      uint8_t u8_num = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (u8_num == 2) {
        PutVar<int16_t, double>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 4) {
        PutVar<int32_t, double>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 8) {
        PutVar<double>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::DOUBLE_SIMPLIFY_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] size %d can't send double\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), u8_num);
      }
    } else if (rule.var_type == "float") {
      uint8_t u8_num = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (u8_num == 2) {
        PutVar<int16_t, float>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 4) {
        PutVar<float>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::FLOAT_SIMPLIFY_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] size %d can't send float\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), u8_num);
      }
    } else if (rule.var_type == "bool") {
      PutVar<bool>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u64") {
      PutVar<uint64_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u32") {
      PutVar<uint32_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u16") {
      PutVar<uint16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u8") {
      PutVar<uint8_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i64") {
      PutVar<int64_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i32") {
      PutVar<int32_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i16") {
      PutVar<int16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i8") {
      PutVar<int8_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u8_array") {
      int len = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (single_protocol_data.len == len) {
        uint8_t * p_data = static_cast<uint8_t *>(single_protocol_data.addr);
        for (int a = rule.parser_param[0]; a <= rule.parser_param[1]; a++, p_data++) {
          raw_data[a] = *p_data;
        }
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RUNTIME_SIZENOTMATCH);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] var_name:\"%s\" size not match, Target need:%d - get:%d"
          ", can't write data for send\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), rule.var_name.c_str(),
          single_protocol_data.len, len);
      }
    }
  }

  // Decode //////////////////////////////////////////////////////////////////////////////////////
  template<typename Target>
  inline void GetVar(
    ProtocolData & var,
    const uint8_t * const raw_data,
    const VarRuleBase & rule,
    const std::string & out_name,
    bool & error_flag)
  {
    GetVar<Target, Target>(var, raw_data, rule, out_name, error_flag);
  }

  template<typename Target, typename Source>
  void GetVar(
    ProtocolData & var,
    const uint8_t * const raw_data,
    const VarRuleBase & rule,
    const std::string & out_name,
    bool & error_flag)
  {
    if (sizeof(Target) > var.len) {
      error_flag = true;
      error_clct_->LogState(ErrorCode::RUNTIME_SIZEOVERFLOW);
      printf(
        C_RED "[%s_PARSER][ERROR][%s] var_name:\"%s\" size overflow, "
        "can't write to protocol TDataClass\n" C_END,
        parser_name_.c_str(), out_name.c_str(), rule.var_name.c_str());
    } else {
      uint64_t result = 0;
      if (rule.parser_type == "var") {
        // var
        for (int a = rule.parser_param[0]; a <= rule.parser_param[1]; a++) {
          if (a != rule.parser_param[0]) {result <<= 8;}
          result |= raw_data[a];
        }
      } else if (rule.parser_type == "bit") {
        // bit
        result =
          (raw_data[rule.parser_param[0]] &
          CreatMask(rule.parser_param[1], rule.parser_param[2])) >> rule.parser_param[2];
      }

      *static_cast<Target *>(var.addr) = *(reinterpret_cast<Source *>(&result));
      var.loaded = true;
    }
  }

  template<typename T>
  inline void ZoomVar(ProtocolData & var, float kp)
  {
    *static_cast<T *>(var.addr) *= kp;
  }

  void DecodeVarBase(
    ProtocolData & single_protocol_data,
    const uint8_t * const raw_data,
    const VarRuleBase & rule,
    bool & error_flag)
  {
    if (rule.var_type == "double") {
      uint8_t u8_num = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (u8_num == 2) {
        GetVar<double, int16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 4) {
        GetVar<double, int32_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 8) {
        GetVar<double>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::DOUBLE_SIMPLIFY_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] size %d can't get double\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), u8_num);
      }
      ZoomVar<double>(single_protocol_data, rule.var_zoom);
    } else if (rule.var_type == "float") {
      uint8_t u8_num = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (u8_num == 2) {
        GetVar<float, int16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else if (u8_num == 4) {
        GetVar<float>(single_protocol_data, raw_data, rule, out_name_, error_flag);
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::FLOAT_SIMPLIFY_ERROR);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] size %d can't get float\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), u8_num);
      }
      ZoomVar<float>(single_protocol_data, rule.var_zoom);
    } else if (rule.var_type == "bool") {
      GetVar<bool>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u64") {
      GetVar<uint64_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u32") {
      GetVar<uint32_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u16") {
      GetVar<uint16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u8") {
      GetVar<uint8_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i64") {
      GetVar<int64_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i32") {
      GetVar<int32_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i16") {
      GetVar<int16_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "i8") {
      GetVar<int8_t>(single_protocol_data, raw_data, rule, out_name_, error_flag);
    } else if (rule.var_type == "u8_array") {
      int len = rule.parser_param[1] - rule.parser_param[0] + 1;
      if (single_protocol_data.len >= len) {
        uint8_t * p_data = static_cast<uint8_t *>(single_protocol_data.addr);
        for (int a = rule.parser_param[0]; a <= rule.parser_param[1]; a++, p_data++) {
          *p_data = raw_data[a];
        }
        single_protocol_data.loaded = true;
      } else {
        error_flag = true;
        error_clct_->LogState(ErrorCode::RUNTIME_SIZEOVERFLOW);
        printf(
          C_RED "[%s_PARSER][ERROR][%s] var_name:\"%s\" size overflow, "
          "can't write to protocol TDataClass\n" C_END,
          parser_name_.c_str(), out_name_.c_str(), rule.var_name.c_str());
      }
    }
  }

  void DecodeVar(
    PROTOCOL_DATA_MAP & protocol_data_map,
    T_FRAMEID frame_id,
    uint8_t * raw_data,
    bool & error_flag)
  {
    if (parser_var_map_.find(frame_id) != parser_var_map_.end()) {
      for (auto & rule : parser_var_map_.at(frame_id)) {
        if (protocol_data_map.find(rule.var_name) != protocol_data_map.end()) {
          DecodeVarBase(protocol_data_map.at(rule.var_name), raw_data, rule, error_flag);
        } else {
          error_flag = true;
          error_clct_->LogState(ErrorCode::RUNTIME_NOLINK_ERROR);
          printf(
            C_RED "[%s_PARSER][ERROR][%s] Can't find var_name:\"%s\" in protocol_data_map\n"
            "\tYou may need use LINK_VAR() to link data class/struct in protocol_data_map\n" C_END,
            parser_name_.c_str(), out_name_.c_str(), rule.var_name.c_str());
        }
      }
    }
  }
};

}  // namespace common
}  // namespace cyberdog

#endif  // COMMON_PARSER__PARSER_BASE_HPP_
