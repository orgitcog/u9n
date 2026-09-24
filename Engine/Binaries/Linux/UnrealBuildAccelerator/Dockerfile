FROM ubuntu:24.04
WORKDIR /app
RUN apt-get update && \
    apt-get install --yes --no-install-recommends binutils && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
COPY * /app/
RUN chmod +x ./UbaAgent ./UbaCacheService ./UbaCli ./UbaStorageProxy
ENTRYPOINT ["/app/UbaCli"]