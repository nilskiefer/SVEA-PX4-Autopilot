FROM px4io/px4-dev-nuttx-focal:2022-08-12

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        openocd \
        python3-lxml \
        stlink-tools \
        usbutils \
        zsh \
        zsh-autosuggestions \
        zsh-syntax-highlighting \
    && rm -rf /var/lib/apt/lists/*
