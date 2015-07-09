#!/bin/bash

# fill simulation domain
mpirun -np 4 $LIGGGHTS_BINARY -in in.rotaryKiln_init

# rotate mesh and simulate
#mpirun -np 1 $LIGGGHTS_BINARY -in in.rotaryKiln_run
#mpirun -np 2 $LIGGGHTS_BINARY -in in.rotaryKiln_run
#mpirun -np 4 $LIGGGHTS_BINARY -in in.rotaryKiln_run -var XPROCS 1 -var YPROCS 2 -var ZPROCS 2
mpirun -np 4 $LIGGGHTS_BINARY -in in.rotaryKiln_run -var XPROCS 1 -var YPROCS 4 -var ZPROCS 1
#mpirun -np 4 $LIGGGHTS_BINARY -in in.rotaryKiln_run -var XPROCS 4 -var YPROCS 1 -var ZPROCS 1
