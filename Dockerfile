ARG MINECRAFT_VERSION
FROM alpine:3 AS base


# spigot build stage
FROM base AS build-spigot
ARG MINECRAFT_VERSION

WORKDIR /opt
RUN apk add --no-cache curl git openjdk11-jre
RUN curl -LO https://hub.spigotmc.org/jenkins/job/BuildTools/lastSuccessfulBuild/artifact/target/BuildTools.jar
RUN java -jar BuildTools.jar --rev "$MINECRAFT_VERSION"


# entrypoint build stage
FROM base AS build-entrypoint
RUN apk add --no-cache musl-dev gcc
COPY entrypoint-spigot.c /opt/entrypoint.c
RUN gcc -Werror -o /opt/entrypoint /opt/entrypoint.c


# base minecraft image build
FROM base as base-minecraft

RUN apk add --no-cache bash openjdk11-jre

RUN addgroup -g 565 -S minecraft && adduser -h /data -s /sbin/nologin -G minecraft -S -D -u 565 minecraft

VOLUME /data
WORKDIR /data

ENV MAX_MEMORY=2048M

EXPOSE 25565/tcp


# spigot image build
FROM base-minecraft as spigot
ARG MINECRAFT_VERSION

COPY --from=build-spigot "/opt/spigot-${MINECRAFT_VERSION}.jar" /opt/
RUN ln -s "/opt/spigot-${MINECRAFT_VERSION}.jar" /opt/spigot.jar

COPY --from=build-entrypoint /opt/entrypoint /usr/local/bin/

USER minecraft:minecraft
ENTRYPOINT ["/usr/local/bin/entrypoint", "/opt/spigot.jar"]


# paper build stage
FROM base AS build-paper
ARG MINECRAFT_VERSION

WORKDIR /opt
RUN apk add --no-cache curl jq openjdk11-jre
RUN curl -L "https://papermc.io/api/v1/paper/$MINECRAFT_VERSION/latest" | jq -r .build > paper-version.txt
RUN curl -LOJ "https://papermc.io/api/v1/paper/$MINECRAFT_VERSION/latest/download"
RUN java -jar paper-*.jar
RUN rm "cache/mojang_${MINECRAFT_VERSION}.jar"


# paper image build
FROM base-minecraft as paper
ARG MINECRAFT_VERSION

COPY --from=build-paper "/opt/paper-*.jar" /opt/paper-version.txt /opt/
COPY --from=build-paper "/opt/cache/patched_${MINECRAFT_VERSION}.jar" /opt/cache/
RUN ln -s /opt/paper-*.jar /opt/paper.jar

COPY entrypoint-paper.sh /usr/local/bin/entrypoint.sh

USER minecraft:minecraft
ENTRYPOINT ["/usr/local/bin/entrypoint.sh", "/opt/paper.jar"]
