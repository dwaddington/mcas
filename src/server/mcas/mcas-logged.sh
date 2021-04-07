#!/bin/bash

# Wrapper to cature output of mcas.
# For use whith tmux, which makes it hard to do redirection inline.

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
LOG=$1; shift
# This script specifies two kinds of profiling:
#
# 1. gperftools profiling:
#
#   Compile: run cmake with -DPROFILE=1
#   Runtime: pass a --profile <file> argument to mcas
#
#   Perhaps -DPROFILE-1 ought to be the default if libprofile is found.
#   I don't know of any penalty to linking libprofile.
#
# 3. Invasive profiling
#
#   Compile: run cmake with -DINVASIVE_PERF=1
#   Runtime: set env variable MCAS_INVASIVE_PERF_ENABLE=1
#
#   Both the compile and runtime enablements will incur a runtime performance penalty.
#
echo Duration enabled $MCAS_INVASIVE_PERF_ENABLE > $LOG
MCAS_INVASIVE_PERF_ENABLE=1 $DIR/mcas --profile mcas.prof ${1+"$@"} 2>&1 | tee -a $LOG
