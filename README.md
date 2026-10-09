# myos

Maintained by **Derek Ko**.

A small 32-bit x86 operating system project written in C++ and assembly.
The kernel includes VGA text output, a global descriptor table, interrupt
handling, and PS/2 keyboard and mouse drivers.

## Build

Use a Linux environment with GNU GCC, GNU binutils, GRUB's `grub-mkrescue`,
and `xorriso` installed. GCC must support compiling 32-bit code.

```sh
make mykernel.bin
make mykernel.iso
```

The ISO displays `myos - Derek Ko` in its boot menu and kernel welcome message.
To use `make run`, create a VirtualBox VM named `My Operating System` and attach
`mykernel.iso` as its boot media.

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).
