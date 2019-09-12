# spigot build stage
FROM alpine:latest AS build-spigot

ARG SPIGOT_VERSION

WORKDIR /opt
RUN apk add --no-cache curl git openjdk11-jre
RUN curl -LO https://hub.spigotmc.org/jenkins/job/BuildTools/lastSuccessfulBuild/artifact/target/BuildTools.jar
RUN java -jar BuildTools.jar --rev "$SPIGOT_VERSION"
RUN mv "spigot-${SPIGOT_VERSION}.jar" spigot.jar


# entrypoint build stage
FROM alpine:latest AS build-entrypoint
RUN apk add --no-cache musl-dev gcc
COPY entrypoint.c /opt/
RUN gcc -Werror -o /opt/entrypoint /opt/entrypoint.c


# final image build
FROM alpine:3

RUN apk add --no-cache openjdk11-jre
COPY --from=build-spigot /opt/spigot.jar /opt/

COPY --from=build-entrypoint /opt/entrypoint /usr/local/bin/

RUN addgroup -g 565 -S minecraft && adduser -h /data -s /sbin/nologin -G minecraft -S -D -u 565 minecraft
USER minecraft:minecraft

VOLUME /data
WORKDIR /data

ENV MAX_MEMORY=2048M
ENTRYPOINT ["/usr/local/bin/entrypoint", "/opt/spigot.jar"]

EXPOSE 25565/tcp
