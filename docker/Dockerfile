# Dockerfile for SC1000 buildroot environment
# Uses Ubuntu 18.04 to match the buildroot 2018.08 era

FROM ubuntu:18.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install buildroot dependencies and cmake
RUN apt-get update && apt-get install -y \
    build-essential \
    libncurses5-dev \
    git \
    wget \
    curl \
    unzip \
    bc \
    cpio \
    rsync \
    python \
    file \
    locales \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Set up locale (buildroot needs this)
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Create build user (buildroot doesn't like running as root)
RUN useradd -m -s /bin/bash builder
USER builder
WORKDIR /home/builder

# Download and extract buildroot 2018.08
RUN wget -q https://buildroot.org/downloads/buildroot-2018.08.tar.gz \
    && tar xzf buildroot-2018.08.tar.gz \
    && rm buildroot-2018.08.tar.gz

WORKDIR /home/builder/buildroot-2018.08

# Copy SC1000 config will be done at runtime via volume mount

# Default command
CMD ["/bin/bash"]
