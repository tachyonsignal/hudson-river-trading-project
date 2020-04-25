FROM "debian:stretch"
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install make && apt-get -y install build-essential
WORKDIR /tmp
COPY . .
RUN make test && ./test
