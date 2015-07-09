#!/bin/bash

# fill simulation domain
$LIGGGHTS_BINARY -in in.rotaryKiln_init -var NTHREADS 4

# rotate mesh and simulate
#$LIGGGHTS_BINARY -in in.rotaryKiln_run -var NTHREADS 1
#$LIGGGHTS_BINARY -in in.rotaryKiln_run -var NTHREADS 2
$LIGGGHTS_BINARY -in in.rotaryKiln_run -var NTHREADS 4
