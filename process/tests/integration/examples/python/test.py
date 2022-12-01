
from dataclasses import dataclass
import json

import pypraas

@dataclass
class Input:
    arg1: int
    arg2: int

@dataclass
class Output:
    res: int

def add(invocation, context):

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']

    result = Output
    result.res = input_data['arg1'] + input_data['arg2']

    out_buf = context.get_output_buffer()

    json.dump(res, pypraas.BufferWriter(out_buf))

    return 0

def zero_return(invocation, context):

    return 0
