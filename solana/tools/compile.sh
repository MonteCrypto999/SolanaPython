#!/usr/bin/env bash
# Wrapper for pika_compile

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/pika_compile" "$@"
