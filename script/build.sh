#!/usr/bin/env bash

# Build the project

cmake -S . -B build -G Ninja "$@"
ninja -j8 -C build
