#!/bin/bash

modified=$(echo "$1" | sed 's/a/[[=a=]]/g; s/e/[[=e=]]/g; s/i/[[=i=]]/g; s/o/[[=o=]]/g; s/u/[[=u=]]/g')

grep -E "$modified" autoex*