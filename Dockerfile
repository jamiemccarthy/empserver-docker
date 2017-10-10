FROM debian:buster

ADD . /app
WORKDIR /app
RUN apt-get update \
  && apt-get install -y autoconf gcc git groff make \
  && rm -rf /var/lib/apt/lists/*
RUN ./bootstrap
RUN ./configure && make && make install
