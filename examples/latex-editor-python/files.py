
import dataclasses
from dataclasses import dataclass
from dataclasses_json import dataclass_json
import base64
import json
import os

import pypraas

@dataclass_json
@dataclass
class File:
    path: str
    file: str
    data: str = ""


class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)

def update_file(invocation, context):

    input = File.from_json(invocation.args[0].str())
    if os.path.splitext(input.file)[1] in ['.pdf', '.png']:
        input.data = base64.b64decode(input.data)
        print(len(input.data))

    path = os.path.join(input.path, input.file)
    print(path)
    context.state(path, input.data)

    out_buf = context.get_output_buffer()
    json.dump({'message': f"Saved file to {path}"}, pypraas.BufferStringWriter(out_buf))
    context.set_output_buffer(out_buf)

    return 0

def get_file(invocation, context):

    input = File.from_json(invocation.args[0].str())

    path = os.path.join(input.path, input.file)
    data = context.state(path)
    input.data = data.str()

    out_buf = context.get_output_buffer()
    json.dump(input, pypraas.BufferStringWriter(out_buf), cls=EnhancedJSONEncoder)
    context.set_output_buffer(out_buf)

    return 0

