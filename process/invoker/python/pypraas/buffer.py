
import pickle

class BufferWriter:

    def __init__(self, buf):
        self.buffer = buf
        self.view = buf.view_writable()

    def write(self, b):

        b_len = len(b)
        pos = self.buffer.length

        self.view[ pos : pos + b_len ] = b
        self.buffer.length = pos + b_len

class BufferStringWriter:

    def __init__(self, buf):
        self.buffer = buf
        self.view = buf.view_writable()

    def write(self, string):

        str_encoded = string.encode()
        str_len = len(str_encoded)
        pos = self.buffer.length

        self.view[ pos : pos + str_len ] = str_encoded
        self.buffer.length = pos + str_len

def serialize(buffer, obj):

    pickle.dump(obj, BufferWriter(buffer))

def deserialize(buffer):

    return pickle.loads(buffer.view_readable())
