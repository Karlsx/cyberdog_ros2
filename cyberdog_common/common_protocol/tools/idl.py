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
                can_len = int(single_var['parser_param'][1]) - \
                    int(single_var['parser_param'][0]) + 1
                vars += "%s%s %s[%d];%s\n" % (
                    space(inclass),
                    get_type(var_type),
                    var_name,
                    can_len,
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
BLOCK_IF_ELSE = """  {$CASES} {
    return false;
  }"""
BLOCK_CASE_STR = """  case {$CAN_ID}:
{$SINGLE_OPERATE}    break;
"""
BLOCK_IF_ELSE_STR = """if (can_id == {$CAN_ID}) {
{$SINGLE_OPERATE}    return true;
  } else """
BLOCK_ORDER_CHECK = """    if (%s_order_check != %s) {
      %s_array_package_unorder_error(%s);
      %s_order_check = 0;
      %s;
    }
"""


def down_code(
        toml_path: str,
        out_path: str,
        canid_offset: int = 0,
        name: str = None):
    dynamic_canid_offset = False
    dynamic_canid_offset = True

    idl_path = os.path.abspath(os.path.dirname(__file__))
    toml_dict = toml.load(toml_path)
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
    c_str = c_str.replace('{$NAME}', name)
    c_str = c_str.replace('{$DEF_NAME}', name.upper())
    c_str = c_str.replace('{$CANID_OFFSET}',
                          ('\nuint32_t canid_offset = %d;\n' % canid_offset)
                          if dynamic_canid_offset else '')
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
                for i in range(u8_num, 1, -1):
                    eq_tmp_str += " | ((%s)can_data[%d] << %d)" % (
                        move_type, i - 1, (i - 1) * 8)
                if(eq_tmp_str != ''):
                    eq_tmp_str = eq_tmp_str[3:] + \
                        ' | ((%s)can_data[0])' % move_type
                else:
                    eq_tmp_str = 'can_data[0]'

                if(single_var['var_type'] == 'u8_array'):
                    tmp_str = 'memcpy(&rx_data.%s[0], &can_data[%d], %d);\n' % (
                        var_name, param[0], u8_num)
                    print(tmp_str)
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
        array_order_check_str = ''
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
            can_len = get_canlen(toml_dict)
            if(package_num == 1):
                pass
            elif(package_num <= 255):
                array_order_check_str += 'uint8_t %s_order_check = 0;\n' % array_name
            else:
                raise "Error, array package num only support <= 255, you need extend type"
            for canid in canid_list:
                tmp_str = ''
                if(canid not in cases_dict):
                    cases_dict.update({canid: ''})
                package_index = canid_list.index(canid)
                if(package_num != 1):
                    tmp_str += BLOCK_ORDER_CHECK % (
                        array_name, str(
                            package_index), array_name, canid, array_name,
                        'return true' if dynamic_canid_offset else 'break')
                    if(package_index != package_num - 1):
                        tmp_str += '    %s_order_check++;\n' % array_name
                    else:
                        tmp_str += '    %s_order_check = 0;\n' % array_name
                tmp_str += '    memcpy(&rx_data.%s[%d], &can_data[0], %d);\n' % (
                    array_name, package_index * 8, can_len)

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
                    move_type = get_move_type(
                        int(bits / 8) + (1 if bits % 8 != 0 else 0))
                    tail_bits_num = 8 - bits_effect_left if bits_effect_left != 0 else 0
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
        if(array_order_check_str != ''):
            array_order_check_str = '\n// Array package recive order check\n' + \
                array_order_check_str
        c_str = c_str.replace('{$ARRAY_ORDER_CHECK}', array_order_check_str)

    cases = ''
    for key in cases_dict.keys():
        cases += (BLOCK_IF_ELSE_STR
                  if dynamic_canid_offset else BLOCK_CASE_STR).replace(
                      '{$CAN_ID}', key).replace('{$SINGLE_OPERATE}', cases_dict[key])
    cases = cases[0:-1]
    c_str = c_str.replace(
        '{$SWITCH_OR_IF}', BLOCK_IF_ELSE if dynamic_canid_offset else BLOCK_SWITCH)
    c_str = c_str.replace('{$CASES}', cases)
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
