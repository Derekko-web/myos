# myos

Maintained by **Derek Ko**.

A small 32-bit x86 operating system project written in C++ and assembly.
The kernel includes VGA text output, a global descriptor table, interrupt
handling, memory management, multitasking, system calls, PS/2 input drivers,
a graphical desktop, ATA disk and FAT32 support, and an Ethernet/IP network
stack with ICMP, UDP, TCP, and an HTTP demo.

Source files live in `src/`, headers in `include/`, and generated object files
in `obj/`. Demo explanations and diagrams are in `docs/`.

The default build enables the FAT32 demo (`B02_FAT32_DEMO` in `src/kernel.cpp`).
The disk demo expects a test disk attached as the primary slave. To run the
network demo instead, disable `B02_FAT32_DEMO` and enable `NETWORK_DEMO`;
use a VirtualBox PCnet/AMD network adapter. The demos are experimental.

## Build

Use a Linux environment with GNU GCC, GNU binutils, GRUB's `grub-mkrescue`,
and `xorriso` installed. GCC must support compiling 32-bit code.

```sh
make mykernel.bin
make mykernel.iso
```

The ISO displays `myos - Derek Ko` in its boot menu. The kernel prints the same
welcome message when `KERNEL_TRACE_OUTPUT` is enabled.
To use `make run`, create a VirtualBox VM named `My Operating System` and attach
`mykernel.iso` as its boot media.

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).
