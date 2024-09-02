
# ukprofiler

`ukprofiler` is a tool designed for collecting and analyzing stack traces of Unikraft applications using the LibVMI library. It supports both human-readable and [flamegraph](https://github.com/brendangregg/FlameGraph)-compatible output formats.

## Usage

```bash
./ukprofiler -n <vmname> -s <socket_path> [options]
```

### Options

- `-n <vmname>`: **(Required)** Name of the Unikraft application to profile.
- `-s <socket_path>`: **(Required)** Path to the socket for KVMI communication.
- `-o <output_file>`: Path to the output file where stack traces will be saved. Defaults to `out.trace`.
- `-i <interval>`: Interval (in milliseconds) between consecutive stack traces. Defaults to 10 ms.
- `-t <timeout>`: Timeout (in seconds) after which the profiling will stop automatically. Defaults to 0 (no timeout).
- `-f <format>`: Output format, either `human` for human-readable output or `flamegraph` for flamegraph-compatible output. Defaults to `flamegraph`.
- `-v`: Enable virtual memory address translation.
- `-h`: Display usage information.

### Examples

1. **Basic usage**:
    ```bash
    ./ukprofiler -n nginx -s /var/run/kvmi.socket
    ```

2. **Human-readable output**:
    ```bash
    ./ukprofiler -n nginx -s /var/run/kvmi.socket -f human -o my_vm_stack.txt
    ```

3. **With timeout and interval**:
    ```bash
    ./ukprofiler -n nginx -s /var/run/kvmi.socket -t 60 -i 100
    ```


