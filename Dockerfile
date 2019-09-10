FROM alpine:3

ARG SPIGOT_VERSION

RUN apk add --no-cache curl git openjdk11-jre && \
    cd /opt && \
    curl -LO https://hub.spigotmc.org/jenkins/job/BuildTools/lastSuccessfulBuild/artifact/target/BuildTools.jar && \
    java -jar BuildTools.jar --rev "$SPIGOT_VERSION" && \
    rm -rf BuildData/ BuildTools.jar BuildTools.log.txt Bukkit/ CraftBukkit/ Spigot/ apache-maven-3.6.0/ work/ && \
    apk del --no-cache git
RUN ln -s "/opt/spigot-${SPIGOT_VERSION}.jar" /opt/spigot.jar
COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN addgroup -g 565 -S minecraft && adduser -h /data -s /sbin/nologin -G minecraft -S -D -u 565 minecraft
USER minecraft:minecraft

VOLUME /data
WORKDIR /data

ENV MAX_MEMORY=2048M
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

EXPOSE 25565/tcp
