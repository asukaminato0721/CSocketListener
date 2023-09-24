# CSockets
Blazing ~~FAST asynchronious socket interface for Wolfram Language written in pure C following the UNIX traditions.

- Linux - __Stable__
- MacOS - __Stable__
- Windows - __Stable__

More than `\sim 7` times faster, than the native implemetation shepped with Wolfram Kernel.
Implemented with zero overhead using native low-level sockets combined together with sort of event-loop. The reading and writting are done via queries and polling with no `sleep()` or `delay()` involved.

## Why is it better than any other C-implementations of a tiny TCP servers?
- __it does not require any libraries__ expect the standart C tools shipped with any OS
- it handles `35` error, which most implementations treats like a regular error and breaks the data transferring. In practice this is common thing (depends on a machine and network), when it comes a payload that does not fit the buffer of a network card. Our server saves failed leftover bytes for later and tries again after some time. 
- the read/write routines is non-blocking and randomized each time after the polling, therefore the process will not be fully occupied, even if there is a large payload came from one of connected client
- no delayed cycles or timers are used - no waste of CPU resources!


## Examples

### Single page
```shell
wolframscript -f Tests/Simple.wls
```

### Dynamic app (involves websockets)
```shell
wolframscript -f Tests/Full.wls
```

## Building
In the `LibraryResurces` we placed all prebuild binaries.
__Skip this section if you want just to run this package__

If there are some issues with a shipped binaries, one can try to compile it.
```bash
wolframscript -f Scritps/BuildLibrary.wls
```