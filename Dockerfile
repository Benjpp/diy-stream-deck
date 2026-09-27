FROM debian:latest AS base
WORKDIR /app

FROM base as Dev

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN g++ -o listener mqttListener.cpp -lpaho-mqttpp3 -lpaho-mqtt3as
CMD ["./listener"]

FROM base as Prod

COPY --from=Dev /app/listener .
CMD ["./listener"]
