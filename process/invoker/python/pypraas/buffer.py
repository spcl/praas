
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
        print(buf.length)
        print(len(self.view))

    def write(self, string):

        str_encoded = string.encode()
        str_len = len(str_encoded)
        pos = self.buffer.length

        print(str_encoded, str_len, pos, type(pos), type(str_len))
        print(self.view)
        print(pos, pos + str_len)
        print(len(self.view[ pos : (pos + str_len) ]))
        self.view[ pos : pos + str_len ] = str_encoded
        self.buffer.length = pos + str_len
