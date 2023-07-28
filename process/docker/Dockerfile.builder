FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update\
  && apt-get install --reinstall -y --no-install-recommends apt-transport-https ca-certificates\
  && apt-get install -y --no-install-recommends wget git build-essential libboost-all-dev uuid-dev libjsoncpp-dev zlib1g-dev libssl-dev\
  && update-ca-certificates

RUN wget https://github.com/Kitware/CMake/releases/download/v3.27.1/cmake-3.27.1-linux-x86_64.sh\
  && /bin/sh ./cmake-3.27.1-linux-x86_64.sh --prefix=/opt/ --skip-license\
  && rm cmake-3.27.1-linux-x86_64.sh
