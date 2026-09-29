# Android build image for Vita3K, used with Apple's `container` CLI.
# The Android NDK ships Linux host tools for x86_64 only, so this image is
# amd64 and runs through Rosetta on Apple silicon.
# Use container/vita3k.sh instead of calling `container` by hand.
FROM ubuntu:24.04

ARG ANDROID_CMDLINE_TOOLS=13114758
ARG ANDROID_API_LEVEL=35
ARG ANDROID_BUILD_TOOLS=36.0.0
# Keep in step with ndkVersion in android/app/build.gradle.
ARG ANDROID_NDK_VERSION=29.0.14206865

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        openjdk-17-jdk-headless git curl ca-certificates unzip zip tar \
        cmake ninja-build pkg-config build-essential python3 perl \
        autoconf automake libtool ccache file \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    ANDROID_NDK_HOME=/opt/android-sdk/ndk/${ANDROID_NDK_VERSION}
RUN mkdir -p ${ANDROID_HOME}/cmdline-tools \
    && curl -fsSLo /tmp/tools.zip https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS}_latest.zip \
    && unzip -q /tmp/tools.zip -d ${ANDROID_HOME}/cmdline-tools \
    && mv ${ANDROID_HOME}/cmdline-tools/cmdline-tools ${ANDROID_HOME}/cmdline-tools/latest \
    && rm /tmp/tools.zip \
    && (yes | ${ANDROID_HOME}/cmdline-tools/latest/bin/sdkmanager --licenses > /dev/null || true) \
    && ${ANDROID_HOME}/cmdline-tools/latest/bin/sdkmanager --install \
        "platforms;android-${ANDROID_API_LEVEL}" \
        "build-tools;${ANDROID_BUILD_TOOLS}" \
        "ndk;${ANDROID_NDK_VERSION}"

ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# These folders are in the cache volume, which exists only at run time.
# container/build-android.sh creates them.
ENV VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg \
    VCPKG_DOWNLOADS=/cache/vcpkg-downloads \
    GRADLE_USER_HOME=/cache/gradle \
    CCACHE_DIR=/cache/ccache \
    CCACHE_MAXSIZE=20G
ENV PATH=${VCPKG_ROOT}:${ANDROID_HOME}/cmdline-tools/latest/bin:${PATH}

WORKDIR /src
