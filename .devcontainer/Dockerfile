FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04

# Install OpenMP library and Python 3
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        libomp-dev \
        python3 \
        python3-pip \
        python3-venv \
        python3-matplotlib \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*