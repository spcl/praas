import base64
import dataclasses
from dataclasses import dataclass
from os.path import exists
from dataclasses_json import dataclass_json
from datetime import datetime, timedelta
import json
import os
import subprocess

@dataclass_json
@dataclass
class CompileRequest:
    file: str
    clean: bool

@dataclass_json
@dataclass
class FileRequest:
    file: str

class EnhancedJSONEncoder(json.JSONEncoder):
    def default(self, o):
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        return super().default(o)

MAIN_DIR='/tmp/project'
if not os.path.exists(MAIN_DIR):
    os.makedirs(MAIN_DIR)

import boto3
s3_client = boto3.client('s3')
def state(path, data):

    s3_client.put_object(Body=data.encode(), Bucket='praas-benchmarks', Key=path)

def get_state(path):

    return s3_client.get_object(Bucket='praas-benchmarks', Key=path)['Body'].read().decode('utf-8')

def compile(event, context):

    if 'body' in event:
        event = json.loads(event['body'])

    input = CompileRequest.from_dict(event)

    start_update = datetime.now()
    files = s3_client.list_objects_v2(Bucket='praas-benchmarks')['Contents']
    for file in files:

        path = file['Key']
        timestamp = file['LastModified'].timestamp()
        file_path = MAIN_DIR + path
        updated = False
        if os.path.exists(file_path):
            fs_timestamp = os.path.getmtime(file_path)
            updated = fs_timestamp < timestamp
        else:
            updated = True

        if updated:
            data = get_state(path)

            os.makedirs(os.path.dirname(file_path), exist_ok=True)

            if os.path.splitext(file_path)[1] in ['.pdf', '.png']:
                with open(file_path, 'wb') as input_f:
                    input_f.write(data)
            else:
                with open(file_path, 'w') as input_f:
                    input_f.write(data)
    end_update = datetime.now()

    start_compile = datetime.now()
    cmd = ['latexmk', '-pdf', '--output-directory=pdfs', input.file]
    if input.clean:
        cmd.append('-gg')
    ret = subprocess.run(
        cmd,
        stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
        cwd=MAIN_DIR,
    )
    end_compile = datetime.now()

    return_data = {}

    if ret.returncode == 0:
        return_data['status'] = 'succcess'
    else:
        return_data['status'] = 'failure'
    return_data['output'] = ret.stdout.decode()
    return_data['time_data'] = (end_update - start_update)/timedelta(seconds=1)
    return_data['time_compile'] = (end_compile - start_compile)/timedelta(seconds=1)

    return return_data

if __name__ == "__main__":
    compile({'file': 'sample-sigconf.txt', 'clean': False}, {})

#def get_pdf(invocation, context):
#
#    input = FileRequest.from_json(invocation.args[0].str())
# 
#    file_path = os.path.join(MAIN_DIR, 'pdfs', f"{input.file}.pdf")
#    return_dict = {}
#    if os.path.exists(file_path):
#        return_dict['status'] = 'success'
#        with open(file_path, 'rb') as input_f:
#            return_dict['data'] = base64.b64encode(input_f.read()).decode()
#    else:
#        return_dict['status'] = 'failure'
#
#    out_buf = context.get_output_buffer()
#    json.dump(return_dict, pypraas.BufferStringWriter(out_buf))
#    context.set_output_buffer(out_buf)
#
#    return 0
