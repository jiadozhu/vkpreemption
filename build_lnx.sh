#!/bin/bash

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Debug -GNinja ..

ninja all

cd ..
