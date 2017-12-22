FROM alpine:3.6

RUN apk add --no-cache autoconf automake gcc make musl-dev git groff
COPY .git /app/.git/
WORKDIR /app
ENTRYPOINT git checkout -fq v4.4.0-docker && ./bootstrap && ./configure && make && make install
