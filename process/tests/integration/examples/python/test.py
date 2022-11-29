
from dataclasses import dataclass

@dataclass
class input:
    arg1: int
    arg2: int

@dataclass
class output:
    res: int

def add(invocation, context):

    print(invocation.args[0])

def zero_return(invocation, context):

    return 0
