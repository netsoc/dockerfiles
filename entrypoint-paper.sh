#!/bin/sh
set -e

[ -L /data/cache ] || ln -s /opt/cache /data/cache

jar="$1"
shift
exec java -Xms512M "-Xmx${MAX_MEMORY}" "$@" -jar "$jar" nogui
