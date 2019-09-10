#!/bin/sh

exec java -Xms512M "-Xmx${MAX_MEMORY}" "$@" -jar /opt/spigot.jar nogui
