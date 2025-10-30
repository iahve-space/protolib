FROM ubuntu:24.04

# Минимально необходимое окружение
RUN apt-get update && apt-get install -y --no-install-recommends \
    jq ca-certificates curl git bash sudo \
    build-essential ninja-build cmake pkg-config \
    gcc g++ gcovr lcov python3 python3-pip python3-venv pipx \
    clang-tidy-18 clang-18 llvm-18-dev doxygen graphviz \
 && rm -rf /var/lib/apt/lists/*

# Установка Conan 2.x через pipx (изолированно)
RUN pipx install "conan==2.*" && pipx ensurepath
RUN ln -sf /root/.local/bin/conan /usr/local/bin/conan

# Символьная ссылка для удобства

RUN ln -sf /usr/bin/clang-tidy-18 /usr/bin/clang-tidy

# Node.js + npm (LTS) for semantic-release/commitlint/etc.
RUN set -eux; \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash -; \
    apt-get update; \
    apt-get install -y --no-install-recommends nodejs; \
    rm -rf /var/lib/apt/lists/*; \
    node -v && npm -v

# Опциональный дев-юзер; не падаем если UID уже занят
ARG UID=1000
ARG GID=1000
RUN set -eux; \
    if ! getent passwd "${UID}" >/dev/null; then \
      if ! getent group "${GID}" >/dev/null; then groupadd -g "${GID}" dev; fi; \
      useradd -m -u "${UID}" -g "${GID}" -s /bin/bash dev; \
      echo "dev ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/dev; \
    fi; \
    chmod 0440 /etc/sudoers.d/*

# Рабочая директория для проектов
WORKDIR /workspace

# Оставляем контейнер "живым"
CMD ["bash", "-lc", "sleep infinity"]
#docker buildx build --platform linux/amd64,linux/arm64 -t ilia19911/ilia:latest -f docker/Dockerfile . --push