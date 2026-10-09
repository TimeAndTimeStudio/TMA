# TMA — thin wrapper around ./build.sh for people who like make.
# `./build.sh` works without make installed, so it is the primary entry point.

SHELL := /bin/bash
.DEFAULT_GOAL := all

.PHONY: all deps setup build run package clean distclean

all:
	./build.sh

deps:
	./build.sh deps

setup:
	./build.sh setup

build:
	./build.sh build

run:
	./build.sh run

package:
	./build.sh package

clean:
	./build.sh clean

distclean:
	./build.sh distclean
