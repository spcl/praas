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
    full_clean: bool

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

    return s3_client.get_object(Bucket='praas-benchmarks', Key=path)['Body'].read()

def compile(event, context):

    if 'body' in event:
        event = json.loads(event['body'])

    input = CompileRequest.from_dict(event)

    start_update = datetime.now()
    updated_files = []
    files = s3_client.list_objects_v2(Bucket='praas-benchmarks')['Contents']
    for file in files:

        path = file['Key']
        timestamp = file['LastModified'].timestamp()
        file_path = os.path.join(MAIN_DIR, path)
        updated = False
        if os.path.exists(file_path):
            fs_timestamp = os.path.getmtime(file_path)
            updated = fs_timestamp < timestamp
        else:
            updated = True

        if updated:

            if path.startswith('pdfs/'):
                continue

            updated_files.append(path)
            data = get_state(path)

            #print("update", path, file_path)
            os.makedirs(os.path.dirname(file_path), exist_ok=True)

            if os.path.splitext(file_path)[1] in ['.pdf', '.png']:
                with open(file_path, 'wb') as input_f:
                    input_f.write(data)
            else:
                with open(file_path, 'w') as input_f:
                    input_f.write(data.decode('utf-8'))
    end_update = datetime.now()

    #print("Compile")
    #print(os.listdir(MAIN_DIR))
    start_compile = datetime.now()
    cmd = ['latexmk', '-pdf', '--output-directory=pdfs', input.file]
    if input.clean:
        cmd.append('-gg')
    ret2 = None
    if input.full_clean:
        ret2 = subprocess.run(
            ["latexmk", "-C", '--output-directory=pdfs'],
            stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
            cwd=MAIN_DIR,
        )
    ret = subprocess.run(
        cmd,
        stderr=subprocess.STDOUT, stdout=subprocess.PIPE,
        cwd=MAIN_DIR,
    )
    end_compile = datetime.now()

    return_data = {}

    #with open('/tmp/output', 'r') as in_f:
    #    logs = in_f.read()
    #    print('logs', len(logs))

    if ret.returncode == 0:
        pre, ext = os.path.splitext(input.file)
        path = os.path.join('pdfs', f"{pre}.pdf")
        s3_client.upload_file(os.path.join(MAIN_DIR, path), "praas-benchmarks", path)

        return_data['status'] = 'succcess'
    else:
        return_data['status'] = 'failure'
    return_data['output'] = base64.b64encode(ret.stdout).decode()
    return_data['updated_files'] = updated_files
    return_data['command'] = cmd
    return_data['full_clean'] = input.full_clean
    if ret2 is not None:
        return_data['full_clean_output'] = ret2.stdout.decode()
    return_data['time_data'] = (end_update - start_update)/timedelta(seconds=1)
    return_data['time_compile'] = (end_compile - start_compile)/timedelta(seconds=1)

    #print(return_data)

    return return_data

def get_pdf(event, context):

    if 'body' in event:
        event = json.loads(event['body'])

    input = FileRequest.from_dict(event)

    result = {}
    file_path = os.path.join('pdfs', f"{input.file}.pdf")
    try:
        d = s3_client.get_object(Bucket='praas-benchmarks', Key=file_path)['Body'].read()
        result['data'] = base64.b64encode(d).decode()
        result['status'] = 'success'
    except:
        result['status'] = 'failure'
 
    return result

if __name__ == "__main__":
    #print(get_pdf({'file': 'sample-sigconf'}, {}))
    print(compile({'file': 'sample-sigconf', 'clean': False}, {}))
