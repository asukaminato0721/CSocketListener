# CSockets

CSockets is a FAST socket interface for Wolfram Language, written in C

- Linux - Stable (UV based)
- MacOS - Stable (UV based)
- Windows - Stable (native)

It is more than 15 times faster than the native implementation shipped with the Wolfram Kernel. This is achieved with zero overhead using native Windows/Unix low-level sockets.
## Why is it better than other C implementations of tiny TCP servers?
- It handles error `35`, which most implementations treat as a regular error and break the data transfer. In practice, this is a common occurrence (depending on the machine and network) when a payload exceeds the buffer capacity of a network card / pipe. Our server saves failed leftover bytes for later and retries after some time.


## Examples

### Single page
```shell
wolframscript -f Tests/Simple.wls
```

### Dynamic app (involves websockets)
```shell
wolframscript -f Tests/Full.wls
```

### Stress test
```shell
wolframscript -f Tests/Metaballs.wls
```

## Building (NO NEED)
In the `LibraryResurces` we placed all prebuild binaries.
__Skip this section if you want just to run this package__

If there are some issues with a shipped binaries, one can try to compile it.
```bash
wolframscript -f Scritps/BuildLibrary.wls
```