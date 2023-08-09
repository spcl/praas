
import dataclasses
from dataclasses import dataclass
from datetime import datetime
import json
import numpy as np
import pickle

import pypraas

@dataclass
class Input:
    arg1: int
    arg2: int

@dataclass
class Output:
    result: int = 0

@dataclass
class InputMsgKey:
    message_key: str = ""

@dataclass
class Message:
    message: str = ""
    some_data: int = 0

@dataclass
class StateMessage:
    first_data: int = 0
    second_data: int = 0

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

    input_data = np.frombuffer(invocation.args[0].view_readable(), dtype=np.int32).astype(dtype=np.int_)
    length = len(input_data) * 4

    out_buf = context.get_output_buffer(length)
    out_data = np.frombuffer(out_buf.view_writable(), dtype=np.int32)

    for i in range(len(input_data)):
        out_data[i] = input_data[i] + 2

    out_buf.length = length
    context.set_output_buffer(out_buf)

    return 0

def send_message(invocation, context):

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    msg = Message()
    msg.some_data = 42
    msg.message = "THIS IS A TEST MESSAGE"

    pypraas.serialize(buf, msg)

    context.put(pypraas.function.Context.SELF, message_key, buf);

    return 0

def get_message(invocation, source, context):

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    msg_buf = context.get(source, message_key)
    if msg_buf.length <= 0:
        return 1
    msg = pypraas.deserialize(msg_buf)

    if type(msg) != Message:
        return 1

    if msg.some_data != 42 or msg.message != "THIS IS A TEST MESSAGE":
        return 1

    return 0

def get_message_self(invocation, context):
    return get_message(invocation, pypraas.function.Context.SELF, context)

def get_message_any(invocation, context):
    return get_message(invocation, pypraas.function.Context.ANY, context)

def get_message_explicit(invocation, context):
    return get_message(invocation, context.process_id, context)

def power(invocation, context):

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)

    if input_data['input']['arg2'] > 2:

        input_data['input']['arg2'] = input_data['input']['arg2'] - 1
        buf = context.get_buffer(1024)
        json.dump(input_data, pypraas.BufferStringWriter(buf))

        invoc_result = context.invoke(
            context.process_id,
            "power",
            "second_add" + str(input_data['input']['arg2'] - 1),
            buf
        )
        result_data = json.loads(invoc_result.payload.str())

        result = Output(result_data['result'] * input_data['input']['arg1'])

    else:
        result = Output(input_data['input']['arg1'] ** 2)

    out_buf = context.get_output_buffer()

    json.dump(result, pypraas.BufferStringWriter(out_buf), cls = EnhancedJSONEncoder)

    context.set_output_buffer(out_buf)

    return 0

def send_remote_message(invocation, context):

    active_processes = context.active_processes
    if active_processes[0] == context.process_id:
        other_process_id = active_processes[1]
    else:
        other_process_id = active_processes[0]

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)
    second_buf = context.get_buffer(MSG_SIZE)

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    msg = Message()
    msg.some_data = 42
    msg.message = "THIS IS A TEST MESSAGE"

    second_msg = Message()
    second_msg.some_data = 33
    second_msg.message = "THIS IS A SECOND TEST MESSAGE"

    pypraas.serialize(buf, msg)
    pypraas.serialize(second_buf, second_msg)

    context.put(other_process_id, message_key, buf);
    context.put(other_process_id, message_key + "_2", second_buf);

    return 0

def get_remote_message(invocation, context):

    active_processes = context.active_processes
    if active_processes[0] == context.process_id:
        other_process_id = active_processes[1]
    else:
        other_process_id = active_processes[0]

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    msg_buf = context.get(pypraas.function.Context.ANY, message_key)
    if msg_buf.length <= 0:
        return 1
    msg = pypraas.deserialize(msg_buf)

    if type(msg) != Message:
        return 1

    if msg.some_data != 42 or msg.message != "THIS IS A TEST MESSAGE":
        return 1

    msg_buf = context.get(other_process_id, message_key + "_2")
    msg = pypraas.deserialize(msg_buf)

    if type(msg) != Message:
        return 1

    if msg.some_data != 33 or msg.message != "THIS IS A SECOND TEST MESSAGE":
        return 1

    return 0

def put_get_remote_message(invocation, context):

    active_processes = context.active_processes
    if active_processes[0] == context.process_id:
        other_process_id = active_processes[1]
    else:
        other_process_id = active_processes[0]

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)

    msg = Message()
    msg.some_data = 42
    msg.message = f"MESSAGE {other_process_id}"
    pypraas.serialize(buf, msg)

    # Send message
    context.put(other_process_id, message_key, buf);

    msg_buf = context.get(other_process_id, message_key)
    if msg_buf.length <= 0:
        return 1
    msg = pypraas.deserialize(msg_buf)

    if type(msg) != Message:
        return 1

    if msg.some_data != 42 or msg.message != f"MESSAGE {context.process_id}":
        return 1

    return 0

def remote_invocation(invocation, context):

    active_processes = context.active_processes
    if active_processes[0] == context.process_id:
        other_process_id = active_processes[1]
    else:
        other_process_id = active_processes[0]

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)
    json.dump(input_data, pypraas.BufferStringWriter(buf), cls = EnhancedJSONEncoder)

    invoc_result = context.invoke(
        other_process_id,
        "add",
        "second_add",
        buf
    )

    invoc_data = json.loads(invoc_result.payload.str())

    result = Output(invoc_data['result'] * 2)

    out_buf = context.get_output_buffer()

    json.dump(result, pypraas.BufferStringWriter(out_buf), cls = EnhancedJSONEncoder)

    context.set_output_buffer(out_buf)

    return 0

def remote_invocation_unknown(invocation, context):

    active_processes = context.active_processes
    if active_processes[0] == context.process_id:
        other_process_id = active_processes[1]
    else:
        other_process_id = active_processes[0]

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)
    json.dump(input_data, pypraas.BufferStringWriter(buf), cls = EnhancedJSONEncoder)

    invoc_result = context.invoke(
        other_process_id,
        "unknown_function",
        "second_add2",
        buf
    )

    if invoc_result.return_code != -1:
        return 1
    if invoc_result.payload.length == 0:
        return 1

    if "Ignoring invocation of an unknown function" not in invoc_result.payload.str():
        return 1

    return 0

def local_invocation_unknown(invocation, context):

    MSG_SIZE = 1024
    buf = context.get_buffer(MSG_SIZE)

    invoc_result = context.invoke(
        context.process_id,
        "unknown_function",
        "unknown_id",
        buf
    )

    if invoc_result.return_code != -1:
        return 1
    if invoc_result.payload.length == 0:
        return 1

    if "Ignoring invocation of an unknown function" not in invoc_result.payload.str():
        return 1

    return 0

def state(invocation, context):

    MSG_SIZE = 1024

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    state_buf = context.state(message_key)
    if state_buf.length > 0:
        return 1

    msg = StateMessage()
    msg.first_data = 42
    msg.second_data = 33

    buf = context.get_buffer(MSG_SIZE)
    pypraas.serialize(buf, msg)

    # store state 
    context.state(message_key, buf)

    def state_verification():
        state_buf = context.state(message_key)
        if state_buf.length <= 0:
            return 1
        msg = pypraas.deserialize(state_buf)

        if type(msg) != StateMessage:
            return 1
        if msg.first_data != 42 or msg.second_data != 33:
            return 1

    # verify data is in state
    state_verification()

    # verify the state access doesn't errase the data
    state_verification()

    return 0

def state_get(invocation, context):

    MSG_SIZE = 1024

    input_str = invocation.args[0].str()
    input_data = json.loads(input_str)['input']
    message_key = input_data['message_key']

    # verify data is in state
    state_buf = context.state(message_key)
    if state_buf.length <= 0:
        return 1
    msg = pypraas.deserialize(state_buf)

    if type(msg) != StateMessage:
        return 1
    if msg.first_data != 42 or msg.second_data != 33:
        return 1

    return 0

def state_keys(invocation, context):

    input_keys = ["first_key", "second_key", "another_key"]

    before = datetime.now().timestamp()
    for key in input_keys:
        context.state(key, "")

    received_keys = context.state_keys()

    if len(received_keys) != len(input_keys):
        return 1

    for i in range(len(received_keys)):

        if received_keys[i][0] != input_keys[i]:
            return 1

        if received_keys[i][1] < before:
            return 1

    after = datetime.now().timestamp()
    context.state(input_keys[0], "")
    new_received_keys = context.state_keys()

    if len(new_received_keys) != len(input_keys):
        return 1

    if new_received_keys[0][1] == received_keys[0][1]:
        return 1

    if new_received_keys[0][1] < after:
        return 1

    return 0

