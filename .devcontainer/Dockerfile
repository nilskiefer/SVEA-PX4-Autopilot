#
# PX4 NuttX development environment for devcontainers.
# Mirrors PX4-containers base+nuttx noble closely, but selects the
# Arm GNU toolchain by container architecture so amd64 and arm64 hosts
# both work without changing the devcontainer config.
#

FROM ubuntu:24.04


ARG TARGETARCH

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV CCACHE_UMASK=000
ENV PATH="/usr/lib/ccache:$PATH:/opt/gcc/bin"
ENV TERM=xterm
ENV TZ=UTC
ENV DISPLAY=:99

SHELL ["/bin/bash", "-lc"]

RUN apt-get update && apt-get -y --quiet --no-install-recommends install \
	ant \
	autoconf \
	automake \
	binutils-dev \
	bison \
	build-essential \
	bzip2 \
	ca-certificates \
	ccache \
	cmake \
	cppcheck \
	curl \
	cppzmq-dev \
	dirmngr \
	doxygen \
	file \
	flex \
	gdb \
	genromfs \
	gettext \
	git \
	gnupg \
	gosu \
	gperf \
	lcov \
	libelf-dev \
	libexpat-dev \
	libfreetype6-dev \
	libgmp-dev \
	libgstreamer-plugins-base1.0-dev \
	libgstreamer1.0-dev \
	libgtest-dev \
	libisl-dev \
	libmpc-dev \
	libmpfr-dev \
	libncurses-dev \
	libncurses6 \
	libncursesw6 \
	libpng-dev \
	libssl-dev \
	libtool \
	libvecmath-java \
	lsb-release \
	make \
	ninja-build \
	openssh-client \
	openjdk-11-jdk \
	openjdk-11-jre \
	libopencv-dev \
	pkg-config \
	python3 \
	python3-dev \
	python3-pip \
	python3-venv \
	python3-apt \
	rsync \
	screen \
	shellcheck \
	stlink-tools \
	texinfo \
	tzdata \
	u-boot-tools \
	uncrustify \
	unzip \
	usbutils \
	util-linux \
	valgrind \
	vim-common \
	wget \
	xsltproc \
	zip \
	openocd \
	kconfig-frontends \
	zsh \
	zsh-autosuggestions \
	zsh-syntax-highlighting \
	&& if [[ "${TARGETARCH}" == "amd64" ]]; then \
		apt-get -y --quiet --no-install-recommends install g++-multilib gcc-multilib; \
	fi \
	&& apt-get -y autoremove \
	&& apt-get clean autoclean \
	&& rm -rf /var/lib/apt/lists/{apt,dpkg,cache,log} /tmp/* /var/tmp/*

# Gazebo Harmonic repository + dependencies required for PX4 gz_* SITL targets.
RUN mkdir -p /etc/apt/keyrings \
	&& wget -qO- https://packages.osrfoundation.org/gazebo.gpg \
	| gpg --dearmor -o /etc/apt/keyrings/gazebo-archive-keyring.gpg \
	&& echo "deb [signed-by=/etc/apt/keyrings/gazebo-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable noble main" \
	> /etc/apt/sources.list.d/gazebo-stable.list \
	&& apt-get update \
	&& apt-get -y --quiet --no-install-recommends install \
	gz-harmonic \
	libgz-transport13-dev \
	libgz-sim8-dev \
	libgz-sensors8-dev \
	libgz-plugin2-dev \
	libprotobuf-dev \
	protobuf-compiler \
	&& apt-get -y autoremove \
	&& apt-get clean autoclean \
	&& rm -rf /var/lib/apt/lists/{apt,dpkg,cache,log} /tmp/* /var/tmp/*

# Set java 11 as default.
RUN update-alternatives --set java "$(update-alternatives --list java | grep 'java-11')"

# gtest
RUN cd /usr/src/gtest \
	&& mkdir build && cd build \
	&& cmake .. && make -j"$(nproc)" \
	&& find . -name '*.a' -exec cp {} /usr/lib \; \
	&& cd .. && rm -rf build

# Python dependencies aligned with PX4-containers base image.
RUN python3 -m pip install --break-system-packages \
	argparse \
	argcomplete \
	cerberus \
	coverage \
	empy==3.3.4 \
	jinja2 \
	jsonschema \
	kconfiglib \
	lxml \
	"matplotlib>=3.0.*" \
	nunavut>=1.1.0 \
	numpy \
	packaging \
	pkgconfig \
	psutil \
	pynacl \
	pyros-genmsg \
	pyulog \
	pyyaml \
	requests \
	serial \
	six \
	toml \
	mavproxy \
	future \
	wheel

# manual ccache setup
RUN ln -sf /usr/bin/ccache /usr/lib/ccache/cc \
	&& ln -sf /usr/bin/ccache /usr/lib/ccache/c++ \
	&& ln -sf /usr/bin/ccache /usr/lib/ccache/arm-none-eabi-g++ \
	&& ln -sf /usr/bin/ccache /usr/lib/ccache/arm-none-eabi-gcc

# astyle v3.1
RUN wget -q https://downloads.sourceforge.net/project/astyle/astyle/astyle%203.1/astyle_3.1_linux.tar.gz -O /tmp/astyle.tar.gz \
	&& cd /tmp && tar zxf astyle.tar.gz && cd astyle/src \
	&& make -f ../build/gcc/Makefile -j"$(nproc)" && cp bin/astyle /usr/local/bin \
	&& rm -rf /tmp/*

# GNU Arm Embedded Toolchain 13.3.Rel1 with host arch selected automatically.
RUN case "${TARGETARCH}" in \
		amd64) TOOLCHAIN_HOST="x86_64" ;; \
		arm64) TOOLCHAIN_HOST="aarch64" ;; \
		*) echo "Unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
	esac \
	&& mkdir -p /opt/gcc && cd /opt/gcc \
	&& wget -qO- "https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-${TOOLCHAIN_HOST}-arm-none-eabi.tar.xz" \
	| tar -Jx --strip 1 \
	&& rm -rf /opt/gcc/share/doc

# `kconfig-frontends` and `bloaty` are installed via apt above to avoid
# clone-time failures in restricted network environments.

RUN useradd --shell /bin/bash -u 1001 -c "" -m user && usermod -a -G dialout user

RUN mkdir -p /tmp/.X11-unix /home/root \
	&& chmod 1777 /tmp/.X11-unix \
	&& chown -R root:root /tmp/.X11-unix \
	&& touch /root/.bashrc \
	&& ln -sf /root/.bashrc /home/root/.bashrc

# SITL UDP PORTS
EXPOSE 14556/udp
EXPOSE 14557/udp

CMD ["/bin/bash"]
