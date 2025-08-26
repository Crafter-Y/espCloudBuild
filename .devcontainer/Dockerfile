FROM debian:bookworm-slim

# Basis-Pakete
RUN apt-get update && apt-get install -y \
    curl unzip python3 git bash \
    && rm -rf /var/lib/apt/lists/*

# Arduino CLI installieren
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh

# ESP32 Core installieren
RUN arduino-cli core update-index 
RUN arduino-cli core install esp32:esp32

# Libraries installieren
RUN arduino-cli lib install FastLED
RUN arduino-cli lib install ArduinoJson

# User einrichten (für Codespaces)
RUN useradd -ms /bin/bash vscode
USER vscode
WORKDIR /workspace