
import json

import pypraas

def hello_world(invocation, context):

    out_buf = context.get_output_buffer()

    json.dump({'message': "Hello, world!"}, pypraas.BufferStringWriter(out_buf))

    context.set_output_buffer(out_buf)

    return 0

