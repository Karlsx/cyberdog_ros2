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
    if var_type in type_map:
        return type_map[var_type]
    return var_type


def space(inclass: bool) -> str:
    return '    ' if inclass else '  '


def up_code(toml_dict: dict, inclass: bool) -> str:
    path = os.path.abspath(os.path.dirname(__file__))
    path += "/templates/upcode_inclass.txt" if inclass else "/templates/upcode_outclass.txt"
    reader = open(path, 'r')
    out_str = reader.read()

    vars = ""
    link_vars = ""
    if('var' in toml_dict):
        for single_var in toml_dict['var']:
            var_type = single_var['var_type']
            var_name = single_var['var_name']
            if (var_type == 'u8_array'):
                can_len = int(single_var['parser_param'][1]) - \
                    int(single_var['parser_param'][0]) + 1
                vars += "%s%s %s[%d];\n" % (space(inclass),
                                            get_type(var_type), var_name, can_len)
            else:
                vars += "%s%s %s;\n" % (space(inclass),
                                        get_type(var_type), var_name)
            link_vars += "%sptc->LINK_VAR(ptc->GetData()->%s);\n" % (
                space(inclass), var_name)
    if('array' in toml_dict):
        for single_array in toml_dict['array']:
            array_name = single_array['array_name']
            package_num = single_array['package_num']
            can_len = 8
            if('canfd_enable' in toml_dict and toml_dict['canfd_enable']):
                can_len = 64
            vars += "%s%s %s[%d];\n" % (space(inclass), 'uint8_t', array_name,
                                        can_len * package_num)
            link_vars += "%sptc->LINK_VAR(ptc->GetData()->%s);\n" % (
                space(inclass), array_name)
    if(vars == ""):
        out_str = out_str.replace('{$VARS}',
                                  '%s// Todo: Add vars in this class.' % space(inclass)).replace(
            '{$LINK_VARS}',
            '%s// Todo: Link vars your add in class.' % space(inclass))
    else:
        vars = vars[0: -1]
        link_vars = link_vars[0: -1]
        out_str = out_str.replace('{$VARS}', vars)
        out_str = out_str.replace('{$LINK_VARS}', link_vars)
    return out_str


if __name__ == '__main__':
    cmd = sys.argv[1]
    if(cmd == 'up'):
        print(up_code(toml.load(sys.argv[2]), False))
    elif(cmd == 'up-inclass'):
        print(up_code(toml.load(sys.argv[2]), True))
    else:
        print('unknow cmd:"%s"' % cmd)
