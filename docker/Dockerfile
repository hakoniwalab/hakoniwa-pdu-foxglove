FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        curl \
        g++ \
        git \
        libboost-dev \
        libfastcdr-dev \
        libgtest-dev \
        make \
        nlohmann-json3-dev \
        unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/hakoniwa-pdu-foxglove

COPY . .

RUN if [ -d .git ]; then git submodule update --init --recursive; fi

RUN cmake -S . -B build \
    && cmake --build build -j"$(nproc)" \
    && ctest --test-dir build --output-on-failure

EXPOSE 8765

CMD ["./build/cdr_publisher_example", "docker/config/endpoint_foxglove.json"]
