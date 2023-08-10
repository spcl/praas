
import dataclasses
from dataclasses import dataclass
from dataclasses_json import dataclass_json
import base64
import json
import os
import io

import boto3

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

s3_client = boto3.client('s3')

def state(path, data):

    if isinstance(data, bytes):
        print(len(data))
        s3_client.put_object(Body=data, Bucket='praas-benchmarks', Key=path)
    else:
        print(len(data.encode()))
        s3_client.put_object(Body=data.encode(), Bucket='praas-benchmarks', Key=path)

def get_state(path):

    return s3_client.get_object(Bucket='praas-benchmarks', Key=path)['Body'].read()

def update_file(event, context):

    if 'body' in event:
        event = json.loads(event['body'])

    input = File.from_dict(event)
    if os.path.splitext(input.file)[1] in ['.pdf', '.png']:
        input.data = base64.b64decode(input.data)

    path = os.path.join(input.path, input.file)
    print(path, len(input.data))
    state(path, input.data)

    return {'message': f"Saved file to {path}"}

def get_file(event, context):

    if 'body' in event:
        event = json.loads(event['body'])

    input = File.from_dict(event)

    path = os.path.join(input.path, input.file)
    data = get_state(path)
    if os.path.splitext(input.file)[1] in ['.pdf', '.png']:
        input.data = base64.b64encode(data)
    else:
        input.data = data.decode()

    return input.to_dict()

if __name__ == "__main__":
    
    #f = File("/test/test2", "newfile", "sssasa")
    #update_file(f.to_json(), {})
    f = File("/test/test2", "newfile", "")
    new_f = File.from_json(get_file(f.to_json(), {}))
    print(type(new_f.data))
    print(new_f.data)
