
import click
import json
import os
import sys

import pypraas

# FIXME: signal definitions

class Functions:

    def __init__(self, code_directory, code_config):

        with open(os.path.join(code_directory, code_config), 'r') as f:
            self._func_data = json.load(f)['functions']['python']

        self._modules = {}
        self._functions = {}

        for func, code in self._func_data.items():

            module_path = code['code']['module']
            function = code['code']['function']
            module_name = os.path.basename(module_path)

            module = self._modules.get(module_path)
            if module is None:
                module = self.load_module(module_name, module_path)
                self._modules[module_path] = module

            self._functions[func] = getattr(module, function)

    def load_module(self, name, path):

        import importlib.machinery
        import importlib.util

        loader = importlib.machinery.SourceFileLoader(name, path)
        spec = importlib.util.spec_from_loader(loader.name, loader)
        assert spec
        mod = importlib.util.module_from_spec(spec)
        loader.exec_module(mod)

        sys.modules[name] = mod

        return mod

    def get_func(self, func_name):

        return self._functions.get(func_name)


@click.command()
@click.option('--process-id', type=str, help='Process ID.')
@click.option('--ipc-mode', type=str, help='IPC mode.')
@click.option('--ipc-name', type=str, help='IPC name.')
@click.option('--code-location', type=str, help='Code location.')
@click.option('--code-config-location', type=str, help='Code config location.')
def invoke(process_id, ipc_mode, ipc_name, code_location, code_config_location):

    functions = Functions(code_location, code_config_location)
    print(functions._func_data)
    print(functions._modules)
    print(functions._functions)

    # FIXME: ipc mode as string
    invoker = pypraas.invoker.Invoker(process_id, pypraas.invoker.IPCMode.POSIX_MQ, ipc_name)

    context = invoker.create_context()

    while True:

        invocation = invoker.poll()

        if invocation is None:
            print(stderr, "Polled empty invocation!", file=sys.stderr)
            break

        func_name = invocation.function_name
        func = functions.get_func(func_name)
        if func is None:
            print(f"Couldn't find function {func_name}", file=sys.stderr)
            continue

        context.start_invocation(invocation.key);

        ret = func(invocation, context)

        if ret is None:
            print("Function did not return status!")
            ret = 1
        else:
            print("Function ended with status {}", ret)

        invoker.finish(context.invocation_id, context.as_buffer(), ret);

        context.end_invocation()

if __name__ == '__main__':
    invoke()

