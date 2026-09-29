# Linux development image for Vita3K, used with Apple's `container` CLI.
# Fedora is used because it ships Qt 6.11, which the Qt frontend needs.
# Use container/vita3k.sh instead of calling `container` by hand.
FROM fedora:44

RUN dnf -y install --setopt=install_weak_deps=False \
        git cmake ninja-build clang lld clang-tools-extra ccache \
        pkgconf-pkg-config python3 which findutils file \
        boost-devel boost-static openssl-devel gtk3-devel libstdc++-static \
        qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtsvg-devel \
        qt6-qtmultimedia-devel qt6-qttools-devel \
        libX11-devel libXext-devel libXrandr-devel libXcursor-devel \
        libXfixes-devel libXi-devel libXScrnSaver-devel libXtst-devel \
        libxkbcommon-devel wayland-devel wayland-protocols-devel libdecor-devel \
        alsa-lib-devel pulseaudio-libs-devel pipewire-devel jack-audio-connection-kit-devel \
        mesa-libGL-devel mesa-libEGL-devel mesa-libgbm-devel libdrm-devel \
        dbus-devel ibus-devel systemd-devel liburing-devel \
    && dnf clean all

ENV CCACHE_DIR=/ccache \
    CCACHE_MAXSIZE=20G \
    CCACHE_BASEDIR=/src
WORKDIR /src
