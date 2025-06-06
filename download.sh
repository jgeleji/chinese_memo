#!/bin/bash
mkdir data
cd data
for i in {1..82}
do
  wget http://hanzidb.org/character-list/general-standard?page=${i}
done
cd ..
