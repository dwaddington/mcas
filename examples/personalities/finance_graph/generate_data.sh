#!/bin/bash
if [ -d "./data" ]
then
    echo "Local ./data directory already exists. Remove to regenerate."
    exit 0
fi

pipenv install --dev
cd transactions-graph-generator ; pipenv run python ./generateGraph.py 500 --data ../data
