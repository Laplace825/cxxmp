# build and run the project

all: build

build:
	@bash ./script/build.sh -DCMAKE_BUILD_TYPE=Release

clean:
	@rm -rf ./build ./bin/* ./lib/*

test: build
	@SPDLOG_LEVLE=trace ./bin/cxxmp-Test


.PHONY: build clean
