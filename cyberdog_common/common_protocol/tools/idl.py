#!/usr/bin/python3
#
# Copyright (c) 2022 Beijing Xiaomi Mobile Software Co., Ltd. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
import toml

type_map = {
    'u8': 'uint8_t',
    'u16': 'uint16_t',
    'u32': 'uint32_t',
    'u64': 'uint64_t',
    'i8': 'int8_t',
    'i16': 'int16_t',
    'i32': 'int32_t',
    'i64': 'int64_t',
    'u8_array': 'uint8_t'
}


def get_type(var_type: str) -> str:
    if(var_type in type_map):
        return type_map[var_type]
    return var_type


def get_description(dict_with_desc: dict) -> str:
    if('description' in dict_with_desc and dict_with_desc['description'] != ""):
        return "  // %s" % dict_with_desc['description']
    return ""


def get_canid(hex_str: str, dynamic: bool, canid_offset: int = 0) -> str:
    hex_str = hex_str.replace("'", '').replace(" ", '')
    need_offset = True
    if(hex_str[-1] != '+'):
        canid_offset = 0
        need_offset = False
    hex_str = hex_str.replace("+", '')
    if(dynamic):
        return hex(int(hex_str, base=16)) + (' + canid_offset'
                                             if need_offset else '')
    else:
        return hex(int(hex_str, base=16) + canid_offset)


def get_canlen(toml_dict: dict) -> int:
    if('canfd_enable' in toml_dict and toml_dict['canfd_enable']):
        return 64
    return 8


def get_move_type(i: int) -> str:
    if(i > 4):
        move_type = "uint64_t"
    elif(i > 2):
        move_type = "uint32_t"
    elif(i > 1):
        move_type = "uint16_t"
    else:
        move_type = "uint8_t"
    return move_type


def get_vars_linkvars(toml_dict: dict, inclass: bool) -> tuple:
    vars = ""
    link_vars = ""
    if('var' in toml_dict):
        for single_var in toml_dict['var']:
            var_type = single_var['var_type']
            var_name = single_var['var_name']
            if (var_type == 'u8_array'):
                array_len = int(single_var['parser_param'][1]) - \
                    int(single_var['parser_param'][0]) + 1
                vars += "%s%s %s[%d];%s\n" % (
                    space(inclass),
                    get_type(var_type),
                    var_name,
                    array_len,
                    get_description(single_var))
            else:
                vars += "%s%s %s;%s\n" % (
                    space(inclass),
                    get_type(var_type),
                    var_name,
                    get_description(single_var))
            link_vars += "%sptc->LINK_VAR(ptc->GetData()->%s);\n" % (
                space(inclass), var_name)
    if('array' in toml_dict):
        for single_array in toml_dict['array']:
            array_name = single_array['array_name']
            package_num = single_array['package_num']
            can_len = get_canlen(toml_dict)
            vars += "%s%s %s[%d];%s\n" % (
                space(inclass),
                'uint8_t',
                array_name,
                can_len * package_num,
                get_description(single_array))
            link_vars += "%sptc->LINK_VAR(ptc->GetData()->%s);\n" % (
                space(inclass), array_name)
            if('content' in single_array):
                for single_content in single_array['content']:
                    vars += "%s%s %s;%s\n" % (
                        space(inclass),
                        get_type(single_content['type']),
                        single_content['name'],
                        get_description(single_content))
    if(vars == ""):
        vars = '%s// Todo: Add vars in this class.' % space(inclass)
        link_vars = '%s// Todo: Link vars your add in class.' % space(inclass)
    else:
        vars = vars[0: -1]
        link_vars = link_vars[0: -1]
    return (vars, link_vars)


def space(inclass: bool) -> str:
    return '    ' if inclass else '  '


def front_space(dynamic: bool) -> str:
    return '    ' if dynamic else '    '


def get_buff_type(tmp_buff_dict: dict, buff_type: str):
    if (buff_type in tmp_buff_dict):
        return ''
    else:
        tmp_buff_dict.update({buff_type: True})
        return buff_type


def get_0bmask(h: int, l: int, len: int = 8) -> str:
    out = '0b'
    for i in range(len - 1, -1, -1):
        if (h >= i >= l):
            out += '1'
        else:
            out += '0'
    return out


def readall(file_path: str) -> str:
    reader = open(file_path, 'r')
    out_str = reader.read()
    reader.close()
    return out_str


def up_code(toml_dict: dict, inclass: bool) -> str:
    path = os.path.abspath(os.path.dirname(__file__))
    path += '/templates/upcode_inclass.txt' if inclass else '/templates/upcode_outclass.txt'
    out_str = readall(path)

    vars, link_vars = get_vars_linkvars(toml_dict, inclass)
    out_str = out_str.replace('{$VARS}', vars)
    out_str = out_str.replace('{$LINK_VARS}', link_vars)
    return out_str


BLOCK_SWITCH = """  switch (can_id)
  {
{$CASES}
  default:
    return false;
  }
  return true;"""
SINGLE_CASE_STR = """  case {$CAN_ID}:
{$ORDER_CHECK}{$SINGLE_OPERATE}    break;
"""

BLOCK_IF_ELSE = """  {$CASES} {return false;}
  return true;"""
SINGLE_IF_ELSE_STR = """if (can_id == {$CAN_ID}) {
{$ORDER_CHECK}{$SINGLE_OPERATE}  } else """

BLOCK_ORDER_CHECKER = """
// package recive order check
ORDER_CHECK_TYPE {$NAME}_OC = 0;  // OC: order_checker
bool {$NAME}_OCEF = 0;  // OCEF: order_check_error_flag
"""
BLOCK_ORDER_CHECK = """    if (!is_package_inorder\
(&{$NAME}_OC, %d, &{$NAME}_OCEF, %s)) {%s}
"""


def down_code(
        toml_path: str,
        out_path: str,
        canid_offset: int = 0,
        name: str = None):
    dynamic_canid_offset = False
    # dynamic_canid_offset = True

    idl_path = os.path.abspath(os.path.dirname(__file__))
    toml_dict = toml.load(toml_path)
    extended_frame = True if (
        'extended_frame' in toml_dict and toml_dict['extended_frame']) else False
    canfd_enable = True if (
        'canfd_enable' in toml_dict and toml_dict['canfd_enable']) else False
    can_len = get_canlen(toml_dict)
    h_str = readall(idl_path + '/templates/downcode_h.txt')
    c_str = readall(idl_path + '/templates/downcode_c.txt')
    vars, link_vars = get_vars_linkvars(toml_dict, False)
    if(name is None):
        name = toml_path.split('/')[-1].split('.')[0]
    toml_name = toml_path.split('/')[-1]
    h_str = h_str.replace('{$TOML_NAME}', toml_name)
    h_str = h_str.replace('{$VARS}', vars)
    h_str = h_str.replace('{$NAME}', name)
    h_str = h_str.replace('{$DEF_NAME}', name.upper())
    writer = open('%s/%s.h' % (out_path, name), 'w')
    writer.write(h_str)
    writer.close()

    c_str = c_str.replace('{$TOML_NAME}', toml_name)
    c_str = c_str.replace('{$HEADER_NAME}', name + ".h")

    # RX CODE
    cases_dict = {}
    if('var' in toml_dict):
        for single_var in toml_dict['var']:
            tmp_str = ''
            canid = get_canid(
                single_var['can_id'], dynamic_canid_offset, canid_offset)
            if(canid not in cases_dict):
                cases_dict.update({canid: ''})
            param = single_var['parser_param']
            var_name = single_var['var_name']
            if(len(param) == 2):
                # var
                var_type = get_type(single_var['var_type'])
                u8_num = param[1] - param[0] + 1
                eq_tmp_str = ''
                move_type = get_move_type(u8_num)
                for i in range(param[0], param[1] + 1):
                    move_byte_num = (u8_num - (i - param[0]) - 1) * 8
                    if(move_byte_num != 0):
                        eq_tmp_str += '((%s)can_data[%d] << %d) | ' % (
                            move_type, i, move_byte_num)
                    else:
                        eq_tmp_str += '(%s)can_data[%d] | ' % (
                            move_type, i)
                eq_tmp_str = eq_tmp_str[:-3]

                if(single_var['var_type'] == 'u8_array'):
                    tmp_str = 'memcpy(&rx_data.%s[0], &can_data[%d], %d);\n' % (
                        var_name, param[0], u8_num)
                elif(var_type != move_type):
                    if(var_type == 'float'):
                        if(u8_num == 4):
                            tmp_str = 'rx_data.%s = u32_bits_to_float(%s);\n' % (
                                var_name, eq_tmp_str)
                        elif(u8_num == 2):
                            tmp_str = 'rx_data.%s = (float)((int16_t)(%s)) * %f;\n' % (
                                var_name, eq_tmp_str, single_var['var_zoom'])
                    elif(var_type == 'double'):
                        if(u8_num == 8):
                            tmp_str = 'rx_data.%s = u64_bits_to_double(%s);\n' % (
                                var_name, eq_tmp_str)
                        elif(u8_num == 4):
                            tmp_str = 'rx_data.%s = (double)((int32_t)(%s)) * %f;\n' % (
                                var_name, eq_tmp_str, single_var['var_zoom'])
                        elif(u8_num == 2):
                            tmp_str = 'rx_data.%s = (double)((int16_t)(%s)) * %f;\n' % (
                                var_name, eq_tmp_str, single_var['var_zoom'])
                    else:
                        tmp_str = 'rx_data.%s = (%s)(%s);\n' % (
                            var_name, var_type, eq_tmp_str)
                else:
                    tmp_str = 'rx_data.%s = %s;\n' % (var_name, eq_tmp_str)
            elif(len(param) == 3):
                # bits
                mask = get_0bmask(param[1], param[2])
                tmp_str = 'rx_data.%s = (can_data[%d] & %s) >> %d;\n' % (
                    var_name, param[0], mask, param[2])
                if(param[2] == 0):
                    tmp_str = tmp_str.split(' >>')[0] + ';\n'
                if(mask == '0b11111111' or param[1] == 7):
                    tmp_str = tmp_str[0: tmp_str.find(
                        ' &')] + tmp_str[tmp_str.find(')'): len(tmp_str)]
                if(tmp_str.find('>>') == -1 or tmp_str.find('&') == -1):
                    tmp_str = tmp_str.replace('(', '').replace(')', '')
            cases_dict[canid] += ('    ' + tmp_str)
    if('array' in toml_dict):
        for single_array in toml_dict['array']:
            package_num = single_array['package_num']
            canid_list = single_array['can_id']
            array_name = single_array['array_name']
            if(package_num > 2 and len(canid_list) == 2):
                start = get_canid(
                    canid_list[0], dynamic_canid_offset, canid_offset)
                end = get_canid(
                    canid_list[1], dynamic_canid_offset, canid_offset)
                canid_list = []
                for i in range(int(start.split('+')[0], base=16),
                               int(end.split('+')[0], base=16) + 1):
                    canid_list.append(hex(i) + (' + canid_offset'
                                                if len(start.split('+')) == 2 else ''))
            else:
                for i in range(len(canid_list)):
                    canid_list[i] = get_canid(
                        canid_list[i], dynamic_canid_offset, canid_offset)
            for canid in canid_list:
                tmp_str = ''
                if(canid not in cases_dict):
                    cases_dict.update({canid: ''})
                package_index = canid_list.index(canid)
                tmp_str += '    memcpy(&rx_data.%s[%d], &can_data[0], %d);\n' % (
                    array_name, package_index * can_len, can_len)
                cases_dict[canid] += (tmp_str)
            if('content' in single_array and len(single_array['content']) != 0):
                # |76543210|   Example:
                # |===-----|   = : left                  3
                # |--------|   - : single_content.bits  15
                # |--______|   _ : 8 - effect_left       6
                start_bits = 0
                for single_content in single_array['content']:
                    content_tmp_str = ''
                    content_name = single_content['name']
                    min = single_content['min']
                    max = single_content['max']
                    bits = single_content['bits']
                    whole_array = 'rx_data.%s' % array_name

                    bits_index = int(start_bits / 8)
                    bits_left = start_bits % 8
                    bits_effect_left = (bits_left + bits) % 8
                    bits_effect_num = int(
                        (bits_left + bits) / 8) + (1 if bits_effect_left != 0 else 0)
                    tail_bits_num = 8 - bits_effect_left if bits_effect_left != 0 else 0

                    move_type = get_move_type(
                        int(bits / 8) + (1 if bits % 8 != 0 else 0))
                    if(move_type != 'uint8_t'):
                        bits_block = '(%s)%s[%d]' % (
                            move_type, whole_array, bits_index)
                    else:
                        bits_block = '%s[%d]' % (whole_array, bits_index)
                    if(bits_effect_num == 1):
                        bits_block = '(%s & %s) >> %d' % (
                            bits_block[bits_block.find(')') + 1:], get_0bmask(
                                7 - bits_left, tail_bits_num), tail_bits_num)
                        if(tail_bits_num == 0):
                            bits_block = bits_block[1:bits_block.find(')')]
                        if(bits_left == 0):
                            bits_block = bits_block[1:bits_block.find(' & ')] + \
                                bits_block[bits_block.find(')') + 1:]
                    else:
                        if(bits_left != 0):
                            bits_block = '(%s) & %s' % (
                                bits_block, get_0bmask(7 - bits_left, 0))
                        elif(bits_block.find(' & ') != -1):
                            bits_block = bits_block[:bits_block.find(' & ')] + \
                                bits_block[bits_block.find(')') + 1:]
                        if(bits_effect_num - 1 != 0):
                            bits_block = '((%s) << %d)' % (
                                bits_block, (bits_effect_num - 1) * 8 - tail_bits_num)
                        mid_str = ''
                        for_index = bits_index
                        for i in range(0, bits_effect_num - 2):
                            for_index += 1
                            mid_str += ' | ((%s)%s[%d] << %d)' % (
                                move_type, whole_array, for_index,
                                (bits_effect_num - 2 - i) * 8 - tail_bits_num)
                        bits_block += mid_str
                        bits_block += ' | (%s[%d] >> %d)' % (
                            whole_array, for_index + 1, tail_bits_num)
                    if(bits <= 32):
                        content_tmp_str += \
                            'rx_data.%s = u32_range_to_float((%s), %f, %f, %d);\n' % (
                                content_name, bits_block, min, max, bits)
                    else:
                        content_tmp_str += \
                            'rx_data.%s = u64_range_to_double((%s), %f, %f, %d);\n' % (
                                content_name, bits_block, min, max, bits)
                    start_bits += bits
                    cases_dict[canid] += ('    ' + content_tmp_str)

    cases = ''
    need_check: bool = (len(cases_dict) > 1)
    c_str = c_str.replace(
        '{$ORDER_CHECKER}', BLOCK_ORDER_CHECKER if need_check else '')
    key_index = 0
    for key in cases_dict.keys():
        if(key_index == len(cases_dict) - 1):
            cases_dict[key] += '    {$NAME}_callback(&rx_data);\n'
        cases += (SINGLE_IF_ELSE_STR if dynamic_canid_offset else SINGLE_CASE_STR).replace(
            '{$CAN_ID}', key).replace(
            '{$SINGLE_OPERATE}', cases_dict[key]).replace(
            '{$ORDER_CHECK}', (BLOCK_ORDER_CHECK % (
                key_index, str(key_index == len(cases_dict) - 1).lower(),
                ('return true;' if dynamic_canid_offset else 'break;'))) if need_check else '')
        key_index += 1
    cases = cases[:-1]
    c_str = c_str.replace(
        '{$BLOCK_DECODE}', BLOCK_IF_ELSE if dynamic_canid_offset else BLOCK_SWITCH)
    c_str = c_str.replace('{$CASES}', cases)

    # TX CODE
    tx_dict = {}
    tmp_buff_dict = {}
    if('var' in toml_dict):
        for single_var in toml_dict['var']:
            canid = get_canid(
                single_var['can_id'], dynamic_canid_offset, canid_offset)
            if(canid not in tx_dict):
                tx_dict.update({canid: ''})
                if(len(tx_dict) != 1):
                    tx_dict[canid] += '\n  memset(&tx_buffer[0], 0, sizeof(tx_buffer));\n'

            param = single_var['parser_param']
            var_name = single_var['var_name']
            var_type = single_var['var_type']
            if(len(param) == 2):
                # var
                index = 0
                u8_num = param[1] - param[0] + 1
                move_var_name = 'tx_data->%s' % var_name
                if(var_type == 'float'):
                    move_var_name = 'tmp_u32_buff'
                    if(u8_num == 4):
                        tx_dict[canid] += '  %s%s = float_bits_to_u32(tx_data->%s);\n' % (
                            get_buff_type(tmp_buff_dict, 'uint32_t '), move_var_name, var_name)
                    else:
                        tx_dict[canid] += '  %s%s = tx_data->%s * %f;\n' % (
                            get_buff_type(tmp_buff_dict, 'uint32_t '),
                            move_var_name, var_name, 1 / float(single_var['var_zoom']))
                elif(var_type == 'double'):
                    move_var_name = 'tmp_u64_buff'
                    if(u8_num == 8):
                        tx_dict[canid] += '  %s%s = double_bits_to_u64(tx_data->%s);\n' % (
                            get_buff_type(tmp_buff_dict, 'uint64_t '), move_var_name, var_name)
                    else:
                        tx_dict[canid] += '  %s%s = tx_data->%s * %f;\n' % (
                            get_buff_type(tmp_buff_dict, 'uint64_t '),
                            move_var_name, var_name, 1 / float(single_var['var_zoom']))
                if(var_type == 'u8_array'):
                    tx_dict[canid] += '  memcpy(&tx_buffer[%d], &tx_data->%s[0], %d);\n' % (
                        param[0], var_name, param[1] - param[0] + 1)
                else:
                    for i in range(param[0], param[1] + 1):
                        move_num = (param[1] - param[0] - index) * 8
                        index += 1
                        if(move_num != 0):
                            tx_dict[canid] += '  tx_buffer[%d] = (%s >> %d) & 0xFF;\n' % (
                                i, move_var_name, move_num)
                        else:
                            tx_dict[canid] += '  tx_buffer[%d] = %s & 0xFF;\n' % (
                                i, move_var_name)
            elif(len(param) == 3):
                # bits
                if(param[2] != 0):
                    tx_dict[canid] += '  tx_buffer[%d] |= (tx_data->%s & %s) << %d;\n' % (
                        param[0], var_name, get_0bmask(param[1] - param[2], 0), param[2])
                elif(param[1] - param[2] != 7):
                    tx_dict[canid] += '  tx_buffer[%d] |= tx_data->%s & %s;\n' % (
                        param[0], var_name, get_0bmask(param[1] - param[2], 0))
                else:
                    tx_dict[canid] += '  tx_buffer[%d] = tx_data->%s;\n' % (
                        param[0], var_name)
    if('array' in toml_dict):
        for single_array in toml_dict['array']:
            package_num = single_array['package_num']
            canid_list = single_array['can_id']
            array_name = single_array['array_name']
            if(package_num > 2 and len(canid_list) == 2):
                start = get_canid(
                    canid_list[0], dynamic_canid_offset, canid_offset)
                end = get_canid(
                    canid_list[1], dynamic_canid_offset, canid_offset)
                canid_list = []
                for i in range(int(start.split('+')[0], base=16),
                               int(end.split('+')[0], base=16) + 1):
                    canid_list.append(hex(i) + (' + canid_offset'
                                                if len(start.split('+')) == 2 else ''))
            else:
                for i in range(len(canid_list)):
                    canid_list[i] = get_canid(
                        canid_list[i], dynamic_canid_offset, canid_offset)

            tx_dict.update({canid_list[0]: '\n'})
            if('content' in single_array and len(single_array['content']) != 0):
                # |76543210|   Example:
                # |===-----|   = : left                  3
                # |--------|   - : single_content.bits  15
                # |--______|   _ : 8 - effect_left       6
                start_bits = 0
                tx_dict[canid_list[0]] += \
                    '  memset(&tx_data->%s[0], 0, sizeof(tx_data->%s));\n' % (
                    array_name, array_name)
                for single_content in single_array['content']:
                    content_name = single_content['name']
                    min = single_content['min']
                    max = single_content['max']
                    bits = single_content['bits']
                    whole_array = 'tx_data->%s' % array_name

                    bits_index = int(start_bits / 8)
                    bits_left = start_bits % 8
                    bits_effect_left = (bits_left + bits) % 8
                    bits_effect_num = int(
                        (bits_left + bits) / 8) + (1 if bits_effect_left != 0 else 0)
                    tail_bits_num = 8 - bits_effect_left if bits_effect_left != 0 else 0

                    buff_choose = ''
                    if(bits <= 32):
                        buff_choose = 'tmp_u32_buff'
                        tx_dict[canid_list[0]] += \
                            '  %s%s = float_range_to_u32(tx_data->%s, %f, %f, %d);\n' \
                            % (get_buff_type(tmp_buff_dict, 'uint32_t '), buff_choose,
                               content_name, min, max, bits)
                    else:
                        buff_choose = 'tmp_u64_buff'
                        tx_dict[canid_list[0]] += \
                            '  %s%s = double_range_to_u64(tx_data->%s, %f, %f, %d);\n' \
                            % (get_buff_type(tmp_buff_dict, 'uint64_t '), buff_choose,
                               content_name, min, max, bits)
                    tmp_content_str = ''
                    if(bits_effect_num == 1):
                        tmp_content_str += '  %s[%d] |= %s << %d;\n' % (
                            whole_array, bits_index, buff_choose, tail_bits_num)
                        tmp_content_str.replace(' << 0', '')
                    else:
                        left_bits = bits - (8 - bits_left)
                        tmp_content_str += '  %s[%d] |= %s >> %d;\n' % (
                            whole_array, bits_index, buff_choose, left_bits)
                        loaded_index = 1
                        while(left_bits >= 8):
                            left_bits -= 8
                            loaded_index += 1
                            if(left_bits != 0):
                                tmp_content_str += '  %s[%d] = (%s >> %d) & 0xFF;\n' % (
                                    whole_array, bits_index + loaded_index - 1,
                                    buff_choose, left_bits)
                            else:
                                tmp_content_str += '  %s[%d] = %s & 0xFF;\n' % (
                                    whole_array, bits_index + loaded_index - 1, buff_choose)
                        if(left_bits != 0):
                            tmp_content_str += '  %s[%d] |= (%s << %d) & 0xFF;\n' % (
                                whole_array, bits_index + bits_effect_num - 1,
                                buff_choose, tail_bits_num)
                    tx_dict[canid_list[0]] += tmp_content_str
                    start_bits += bits
            array_index = 0
            for canid in canid_list:
                if(canid not in tx_dict):
                    tx_dict.update({canid: ''})
                tx_dict[canid] += '  memcpy(&tx_buffer[0], &tx_data->%s[%d], %d);\n' % (
                    array_name, array_index * can_len, can_len)
                array_index += 1

    tx_str = ''
    for key in tx_dict.keys():
        tx_str += (tx_dict[key] + '  send_%s_can(%s, %s, &tx_buffer[0]);\n' % (
            'fd' if canfd_enable else 'std', key, str(extended_frame).lower()))
    tx_str = tx_str[:-1]
    c_str = c_str.replace('{$BLOCK_ENCODE}', tx_str)

    c_str = c_str.replace('{$NAME}', name)
    c_str = c_str.replace('{$CAN_LEN}', str(can_len))
    c_str = c_str.replace('{$DEF_NAME}', name.upper())
    c_str = c_str.replace('{$CANID_OFFSET}',
                          ('\nuint32_t canid_offset = %d;\n' % canid_offset)
                          if dynamic_canid_offset else '')
    writer = open('%s/%s.c' % (out_path, name), 'w')
    writer.write(c_str)
    writer.close()

    common_str = readall(idl_path + '/templates/common_protocol.txt')
    writer = open('%s/common_protocol.h' % out_path, 'w')
    writer.write(common_str)
    writer.close()


if __name__ == '__main__':
    cmd = sys.argv[1]
    if(cmd == 'up'):
        print(up_code(toml.load(sys.argv[2]), False))
    elif(cmd == 'up-inclass'):
        print(up_code(toml.load(sys.argv[2]), True))
    elif(cmd == 'down'):
        down_code(sys.argv[2], '/home/karls/work/tmp', 0)
    else:
        print('unknow cmd:"%s"' % cmd)
