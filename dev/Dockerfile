FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        gdb \
        git \
        iproute2 \
        libboost-all-dev \
        libgtest-dev \
        ninja-build \
        pkg-config \
        clang-tidy \
        clang-format \
        clang \
        sudo \
    && rm -rf /var/lib/apt/lists/*

# Non-root user expected by the Dev Containers extension
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# ubuntu:24.04 already ships a default user/group at uid/gid 1000
RUN if getent passwd $USER_UID > /dev/null; then userdel -r "$(getent passwd $USER_UID | cut -d: -f1)"; fi \
    && if getent group $USER_GID > /dev/null; then groupdel "$(getent group $USER_GID | cut -d: -f1)"; fi \
    && groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID --create-home --shell /bin/bash $USERNAME \
    && echo "$USERNAME ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME

WORKDIR /workspace/rillnet

CMD ["/bin/bash"]