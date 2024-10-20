import base64
import dataclasses
from dataclasses import dataclass
from os.path import exists
from dataclasses_json import dataclass_json
from datetime import datetime, timedelta
import json
import os
import subprocess

import pypraas

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

MAIN_DIR='/tmp/state/project'
if not os.path.exists(MAIN_DIR):
    os.makedirs(MAIN_DIR)

def compile(invocation, context):

    input = CompileRequest.from_json(invocation.args[0].str())

    files = context.state_keys()

    start_update = datetime.now()
    updated_files = []
    for path, timestamp in files:

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

            data = context.state(path)

            os.makedirs(os.path.dirname(file_path), exist_ok=True)

            if os.path.splitext(file_path)[1] in ['.pdf', '.png']:
                with open(file_path, 'wb') as input_f:
                    input_f.write(data.view_readable())
            else:
                with open(file_path, 'w') as input_f:
                    input_f.write(data.str())
    end_update = datetime.now()

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

    out_buf = context.get_output_buffer()
    return_data = {}

    if ret.returncode == 0:
        return_data['status'] = 'succcess'
    else:
        return_data['status'] = 'failure'
    return_data['output'] = ret.stdout.decode()
    return_data['updated_files'] = updated_files
    return_data['command'] = cmd
    return_data['full_clean'] = input.full_clean
    if ret2 is not None:
        return_data['full_clean_output'] = ret2.stdout.decode()
    return_data['time_data'] = (end_update - start_update)/timedelta(seconds=1)
    return_data['time_compile'] = (end_compile - start_compile)/timedelta(seconds=1)

    json.dump(return_data, pypraas.BufferStringWriter(out_buf))

    context.set_output_buffer(out_buf)

    return 0

def get_pdf(invocation, context):

    input = FileRequest.from_json(invocation.args[0].str())
 
    file_path = os.path.join(MAIN_DIR, 'pdfs', f"{input.file}.pdf")
    return_dict = {}
    if os.path.exists(file_path):
        return_dict['status'] = 'success'
        with open(file_path, 'rb') as input_f:
            return_dict['data'] = base64.b64encode(input_f.read()).decode()
    else:
        return_dict['status'] = 'failure'

    out_buf = context.get_output_buffer()
    json.dump(return_dict, pypraas.BufferStringWriter(out_buf))
    context.set_output_buffer(out_buf)

    return 0
