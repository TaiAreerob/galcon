#!/bin/sh

map=1
while [  $map -lt 101 ]; do
    echo $map
    for bot in example_bots/*Bot.jar ; do
        echo "$bot"
        ./test.sh $bot $map 2>&1 | tail -n 2
    done
    let map=map+1
done
