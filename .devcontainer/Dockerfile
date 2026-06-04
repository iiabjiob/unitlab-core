FROM mcr.microsoft.com/devcontainers/base:ubuntu

# Install basic tools
RUN apt-get update && apt-get install -y \
    curl \
    git \
    cmake \
    libpq-dev \
    build-essential \
    tcpdump \
    iputils-ping \
    netcat-openbsd \
    && apt-get clean

# Install Node.js 20 + pnpm
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && npm install -g pnpm

# Open AI Codex 
RUN npm i -g @openai/codex

# Install uv for Python
RUN curl -LsSf https://astral.sh/uv/install.sh | sh \
    && mv /root/.local/bin/uv /usr/local/bin/uv

# Python virtual environment inside container
ENV UV_SYSTEM_PYTHON=1

# Bake the IEC 61850 source into the image so the compose-service IED
# can build and start without depending on a host bind mount.
COPY iec61850_ied /workspace/iec61850_ied
COPY external/libiec61850 /workspace/external/libiec61850

# Set working directory
WORKDIR /workspace
