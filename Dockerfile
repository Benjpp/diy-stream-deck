FROM debian:12.1 as base
WORKDIR /app

FROM base as Dev

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    docker.io \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN g++ -o listener ./prod-listener/src/mqttListener.cpp -lpaho-mqttpp3 -lpaho-mqtt3as

FROM base as Prod

RUN apt-get update && apt-get install -y \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    docker.io \
    && rm -rf /var/lib/apt/lists/*

COPY --from=Dev /app/listener .
CMD ["./listener"]
