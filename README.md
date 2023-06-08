# HS-KMON
An attempt to extend HVPP for gamehacking. Note that this was part of a larger project and hence i didn't bother collecting all solution files, so you might not be able to compile this. Once i have time i'll publish some of the other components (launcher,website,backend,netsdk).

# Intro
This project named HS or "Hellscythe", aims to extend the HVPP-Hypervisor for gamehacking purposes. Although i have deployed this successfully for about a year previously, i strongly advise you against usage in 2023.
I have made a mistake and jumped into a project which by far exceeds my knowledge and hence caused me to get burnt out from it. That is also one of the many reasons why some things have been implemented in a very questionable way. I simply did not know better.

# Contents
Following components are included in this repo:
  - HVSDK | A library for interacting with the hypervisor from UM.
  - HVDRV | Hypervisor component utilizing the modular HVPP interface to override vmexit behavior.
  - KMON | Kernel Monitor component handling hooks and OS level things (standalon/isolated from the HV).

# Features
  - Shadow Page Interface and Manager for faked pages (supports multiple).
  - Hooking library with implementation of both normal and EPT-assisted.
  - Left over hooks for `EasyAntiCheat.sys`
  - Mapper for loading an image into an EAC protected game (stores it inside EAC_IMOD)

# Disclaimer
Almost all of the above features:
  - are not viable anymore in 2023.
  - do not work.
  - come with various caveats, too much to justify the hassle.
  - cause system instability.

This can be reasoned with me simply not having the fundamentals at the time of designing and writing this, which caused it to have major flaws in both logic and implementation.

# Use Cases
Well you might ask then what the fucking point is of me uploading it.
  - I want it gone from my disks.
  - Some smaller parts could be useful.
  - Shows that retarded ideas can sometimes work out.

About using this on an anti-cheat protected game. Let's talk about EAC since it's the only AC which i have implemented features for:
  - Talking anything before mid 2022: HVPP mitigated most of these DoS attacks except the CR3 trashing one. I've fixed this by simply using my own top-level page table.
  - They have implemented new DoS methods so this doesn't suffice anymore.

# References
I cannot remember every single one off the top of my head, but i'll try:
  - https://github.com/wbenny/hvpp
  - https://git.back.engineering/_xeroxz/bluepill
  - https://www.unknowncheats.me/forum/anti-cheat-bypass

# Last Words
In case you're trying to debug this (including VMXROOT), then i'd recommend using serial ports for logging. I haven't done it with a physical machine but i can talk about proxmox, logging worked fine with a virtual serial port and outputting data with `__outbyte` intrinsic. However for some reason, VMWARE doesn't seem to receive any input on its serial ports when directly writing to the serial port using `__outbyte`. That could've been a configuration issue on my side though. KVM works out of the box.<br/><br/>
If you want to actually use a debugger and not just have logs, check out this one: https://www.triplefault.io/2017/07/setup-vmm-debugging-using-vmwares-gdb_9.html
