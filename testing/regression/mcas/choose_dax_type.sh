#!/bin/bash

# Choose which config to use: devdax or fsdax, depending on the available devices
if test -c /dev/dax0.0
then	echo devdax
else	echo fsdax
fi

exit 0
