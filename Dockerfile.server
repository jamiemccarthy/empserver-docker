FROM alpine:3.6

ADD . /app
WORKDIR /app
RUN apk add --no-cache autoconf automake gcc make musl-dev git groff      \
  && ./bootstrap && ./configure && make && make install                   \
  && apk del autoconf automake gcc make musl-dev git groff

# Consider starting the server with /app/scripts/keepitup
#RUN files -f && fairland 5 60 && emp_server
