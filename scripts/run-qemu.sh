#!/usr/bin/env bash
# NexusOS — запуск в QEMU. -serial stdio выводит COM1-лог прямо
# в терминал, что удобно для отладки, если экран QEMU не показывает
# всё (например, паника случилась до инициализации VGA).
set -e
cd "$(dirname "$0")/.."
make run
