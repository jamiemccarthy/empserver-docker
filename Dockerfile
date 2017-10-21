FROM debian:buster

ADD . /app
WORKDIR /app
RUN apt-get update                                  \
  && apt-get install -y autoconf gcc git groff make \
    ncurses-devel readline-devel                    \
  && rm -rf /var/lib/apt/lists/*
RUN ./bootstrap && ./configure && make && make install
# Consider starting the server with scripts/keepitup
RUN files -f && fairland 5 60 && emp_server
