#!/usr/bin/env bash
#
# NexusOS — сборка кросс-компилятора i686-elf-gcc + binutils.
# Стандартный путь для OS-разработки: https://wiki.osdev.org/GCC_Cross-Compiler
#
# Запускать один раз. Собранный компилятор ложится в $HOME/opt/cross —
# не трогает системный gcc/binutils.
#
set -e

BINUTILS_VERSION=2.42
GCC_VERSION=13.2.0

PREFIX="$HOME/opt/cross"
TARGET=i686-elf
WORKDIR="$HOME/src/nexusos-toolchain"

echo "==> Проверка зависимостей сборки (нужны на Debian/Ubuntu)"
MISSING=""
for pkg in build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo; do
    dpkg -s "$pkg" >/dev/null 2>&1 || MISSING="$MISSING $pkg"
done
if [ -n "$MISSING" ]; then
    echo "Не хватает пакетов:$MISSING"
    echo "Установи: sudo apt install$MISSING"
    exit 1
fi

mkdir -p "$WORKDIR" "$PREFIX"
cd "$WORKDIR"

export PATH="$PREFIX/bin:$PATH"

echo "==> Скачиваю binutils $BINUTILS_VERSION"
if [ ! -f "binutils-$BINUTILS_VERSION.tar.gz" ]; then
    curl -LO "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"
fi
tar -xzf "binutils-$BINUTILS_VERSION.tar.gz"

echo "==> Собираю binutils"
mkdir -p build-binutils && cd build-binutils
../"binutils-$BINUTILS_VERSION"/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j"$(nproc)"
make install
cd "$WORKDIR"

echo "==> Скачиваю gcc $GCC_VERSION"
if [ ! -f "gcc-$GCC_VERSION.tar.gz" ]; then
    curl -LO "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
fi
tar -xzf "gcc-$GCC_VERSION.tar.gz"

echo "==> Собираю gcc (только C, без libgcc пока нет цели)"
mkdir -p build-gcc && cd build-gcc
../"gcc-$GCC_VERSION"/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make -j"$(nproc)" all-gcc
make -j"$(nproc)" all-target-libgcc
make install-gcc
make install-target-libgcc

echo ""
echo "==> Готово. Добавь в PATH:"
echo "    export PATH=\"$PREFIX/bin:\$PATH\""
echo ""
echo "Проверка: i686-elf-gcc --version"
