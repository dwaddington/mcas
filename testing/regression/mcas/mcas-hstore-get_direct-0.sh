#!/bin/bash
DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

export KEY_LENGTH=${KEY_LENGTH:-8}
export VALUE_LENGTH=${VALUE_LENGTH:-3000000}
export ELEMENT_COUNT=${ELEMENT_COUNT:-6000}
RECOMMENDED_STORE_SIZE=$((ELEMENT_COUNT*(200+KEY_LENGTH+VALUE_LENGTH)*10/10)) # sufficient

GOAL=${GOAL:-200} \
STORE=hstore \
STORE_SIZE=${STORE_SIZE:-$RECOMMENDED_STORE_SIZE} \
PERFTEST=get_direct \
"$DIR/mcas-perf-hstore.sh" $@
