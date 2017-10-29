FROM alpine:3.6

ADD . /app
WORKDIR /app
RUN apk add --no-cache autoconf automake gcc make musl-dev git groff
CMD ./bootstrap && ./configure && make && make install
