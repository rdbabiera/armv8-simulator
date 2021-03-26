# armv8-simulator

Architecture Project
This program simulates a single core ARMv8 processor by taking in assembly code 
and processing instructions through a pipeline. It includes the following features:
  - Instruction and Data cache with 50-cycle stalling
    - Note: The cache pipeline implementation is sometimes off by an instruction during stalls, that is it may not queue an instruction to execute 
    during the last step of a data cache stall.
  - GShare Branch prediction
  - Register/Data dependency support

Sidenote: This repository will be private during summer/fall quarters (course runs in fall).
