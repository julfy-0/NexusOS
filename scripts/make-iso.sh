#!/usr/bin/env bash
# NexusOS — сборка загрузочного .iso через GRUB (grub-mkrescue).
# Обёртка над `make iso`, для запуска не из корня проекта.
set -e
cd "$(dirname "$0")/.."
make iso
