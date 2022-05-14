# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential autoconf libsndfile1-dev libliquid-dev

## Add source code to the build stage.
ADD . /redsea
WORKDIR /redsea

## TODO: ADD YOUR BUILD INSTRUCTIONS HERE.
RUN ./autogen.sh && ./configure && make

# Package Stage
FROM --platform=linux/amd64 ubuntu:20.04

## TODO: Change <Path in Builder Stage>
COPY --from=builder /redsea/src/redsea /
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y libsndfile1-dev libliquid-dev
