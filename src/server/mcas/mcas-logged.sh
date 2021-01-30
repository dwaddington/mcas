#!/bin/bash

# Wrapper to cature output of mcas.
# For use whith tmux, which makes it hard to do redirection inline.

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
LOG=$1; shift
echo Duration enabled $MCAS_DURATION_CLOCK_ENABLED > $LOG
MCAS_DURATION_CLOCK_ENABLED=1 $DIR/mcas ${1+"$@"} 2>&1 | tee -a $LOG
