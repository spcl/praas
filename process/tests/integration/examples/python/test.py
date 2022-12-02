
import dataclasses
from dataclasses import dataclass
import json
import numpy as np

import pypraas

@dataclass
class Input:
    arg1: int
    arg2: int

@dataclass
class Output:
    result: int = 0

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)

def add(invocation, context):

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']

    result = Output(input_data['arg1'] + input_data['arg2'])

    out_buf = context.get_output_buffer()

    json.dump(result, pypraas.BufferStringWriter(out_buf), cls = EnhancedJSONEncoder)

    context.set_output_buffer(out_buf)

    return 0

def zero_return(invocation, context):

    return 0

def error_function(invocation, context):

    return 1

def large_payload(invocation, context):

    input_data = np.frombuffer(invocation.args[0].view_readable(), dtype=np.int32)
    length = len(input_data) * 4

    out_buf = context.get_output_buffer(length)
    out_data = np.frombuffer(out_buf.view_writable(), dtype=np.int32)

    for i in range(len(input_data)):
        out_data[i] = input_data[i] + 2

    out_buf.length = length
    context.set_output_buffer(out_buf)

    return 0
