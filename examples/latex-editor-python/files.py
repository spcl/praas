
import dataclasses
from dataclasses import dataclass
from dataclasses_json import dataclass_json
import base64
import json
import os

import pypraas

import boto3

@dataclass_json
@dataclass
class File:
    path: str
    file: str
    data: str = ""

s3_client = boto3.client('s3')

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)

def s3_state(path, data):

    if isinstance(data, bytes):
        s3_client.put_object(Body=data, Bucket='praas-benchmarks', Key=path)
    else:
        s3_client.put_object(Body=data.encode(), Bucket='praas-benchmarks', Key=path)

def update_file(invocation, context):

    input = File.from_json(invocation.args[0].str())
    if os.path.splitext(input.file)[1] in ['.pdf', '.png']:
        input.data = base64.b64decode(input.data)

    path = os.path.join(input.path, input.file)
    context.state(path, input.data)
    s3_state(path, input.data)

    out_buf = context.get_output_buffer()
    json.dump({'message': f"Saved file {input.file} to {path} and S3"}, pypraas.BufferStringWriter(out_buf))
    context.set_output_buffer(out_buf)

    return 0

def get_file(invocation, context):

    input = File.from_json(invocation.args[0].str())

    path = os.path.join(input.path, input.file)
    data = context.state(path)
    if os.path.splitext(input.file)[1] in ['.pdf', '.png']:
        input.data = base64.b64encode(data.view_readable()).decode()
    else:
        input.data = data.str()

    out_buf = context.get_output_buffer()
    json.dump(input, pypraas.BufferStringWriter(out_buf), cls=EnhancedJSONEncoder)
    context.set_output_buffer(out_buf)

    return 0

