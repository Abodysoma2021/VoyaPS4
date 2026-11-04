FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-12 lld-12 make python3 libicu66 libssl1.1 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
ENV CC=clang-12 CXX=clang++-12 LD=ld.lld-12 OO_PS4_TOOLCHAIN=/sdk
WORKDIR /work
