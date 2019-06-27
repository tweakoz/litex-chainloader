# Summary

This is a chainloader for easing Litex FPGA TFTP bootloading workflow when your files change sizes frequently, or you are heavily iterating making changes to your boot environment (including changing file destination addresses) and you don't want to regenerate your FPGA bitstream every time you add, remove, rename, relocate (in memory) or resize a file. This has only been tested with the vexrisc / arty / linux port, but conceptually at least it should cross over to the other cpu's and OS's.


# To Build:
* set SOC_BUILD_DIR env var to the output dir of your soc's build products (where the gateware and software output directories are)
* set CHAINLOADER_BIN in chainloader.ld to match your system (there is a TODO below to make this dynamic)
* run make
* Alternatively: ```SOC_BUILD_DIR=path/to/my/soc/build/dir make```

# To Use:
  *  generate a boot.manifest which should look something like the snippet below. The boot.manifest is the list of files that the netbooter will pull from the TFTP server. It also includes each file's destination address and and length. I generate this as a part of the build process. Make sure to regenerate it whenever rebuilding buildroot, emulator.bin, your SOC or your devicetree.
  
```
download emulator.bin 0x20000000 8776
download rv32.dtb 0xc0ff0000 1885
download Image 0xc0000000 4676580
download rootfs.cpio 0xc1000000 15118848
boot 0x20000000
end
```

  * place the boot.manifest and any referenced files in your TFTP server directory. your folder should look something like the below.
```
  ls -l tftp_root/arty
    total 19360
    -rw-rw-r-- 1 someuser someuser      168 Jun 26 21:27 boot.manifest
    -rw-rw-r-- 1 someuser someuser     8776 Jun 26 21:27 emulator.bin
    -rw-r--r-- 1 someuser someuser  4676580 Jun 26 21:27 Image
    -rw-r--r-- 1 someuser someuser 15118848 Jun 26 21:27 rootfs.cpio
    -rw-rw-r-- 1 someuser someuser     1885 Jun 26 21:27 rv32.dtb
    -rw-rw-r-- 1 someuser someuser     1895 Jun 26 21:27 rv32.dts
```


  * run TFTP server (one is included)
     if using the included TFTP server, place files in a subdirectory named after a 'class' eg - arty. The included tftp server will route different folders different hosts based on the internal ip->class dictionary. Presumably this behaviour could be used to target specific devices with different software images.

  * boot up the fpga bitstream.

  * ```litex_term --speed <baudrate> <tty> --kernel $SOC_BUILD_DIR/chainloader/chainloader.bin --kernel-adr CHAINLOADER_BIN-address```

# TODO:
  * chainloader.ld : the CHAINLOADER_BIN region / address should be autogenerated to point somewhere in the last section of your soc's SDRAM region - as included it is set up for an arty (the end of arty's 256MiB SDRAM space). I noticed the the chainloader hangs when trying to overlay it @ the standard SDRAM address (0xc00000000), hence why I decided to move it 'out of the way'. Im guessing it had interfered with the bios's stack.

  * improve cross platform story
  * Some of the code was copied out of the Litex bios. Perhaps we can integrate it back in.
  * IP addresses are hard coded (for TFTP server and client).
  * IP addresses are fixed in the bitstream. Short of DHCP which sounds complicated - not sure how to fix this, atm..
  * probably could/should get the flash booter using the manifest.
  * add hashes to the manifest ?
