# LC3-VM
A little virtual machine for [LC3 architecture](https://en.wikipedia.org/wiki/Little_Computer_3) that can execute compiled LC3 programs.

## Build
To build `lc3-vm` install an ANSI C compiler and GNU make then run `make` to build the project.
You can find `lc3-vm` executable in `./src` directory.

## Run the VM
Download [2048](https://www.jmeiners.com/lc3-vm/supplies/2048.obj) or [Rogue](https://www.jmeiners.com/lc3-vm/supplies/rogue.obj) games and run them with `lc3-vm`.
Those are pre-compiled lc3 programs.
```bash
./src/lc3-vm 2048.obj
# or
./src/lc3-vm rogue.obj
```

## Learn
You can learn to write your own LC3 VM by following [Write your Own Virtual Machine](https://www.jmeiners.com/lc3-vm/) tutorial.

