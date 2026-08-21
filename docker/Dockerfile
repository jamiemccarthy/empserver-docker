# Wolfpack Empire 4.4.1 server.
#
# glibc, not musl: the LWP thread package needs makecontext(3), which musl
# does not provide.  Without LWP the regression suite in tests/ cannot run.
#
#   docker build -f docker/Dockerfile -t empserver .

FROM debian:trixie-slim AS build

# libc6-dev is only a Recommends of gcc, so --no-install-recommends drops it and
# the build fails at "C compiler cannot create executables".  readline/ncurses
# are for the bundled terminal client, which is handy for poking at the server.
RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc libc6-dev make perl groff-base libreadline-dev libncurses-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . /src

# configure, aclocal.m4 and config.h.in are committed (see .gitignore).  COPY
# does not guarantee they land newer than configure.ac, and Make.mk has
# autoconf remake rules that would then fire.  Stamp them in dependency order.
RUN cd /src && touch aclocal.m4 && touch configure stamp-h.in config.h.in

WORKDIR /build
# Make.mk has no DESTDIR support, so install straight into the build stage's
# /usr/local (empty on debian-slim) and copy that whole tree into the runtime
# stage.  install-html adds the 254-page info manual as HTML.
RUN /src/configure --without-pthread --prefix=/usr/local \
    && make -j"$(nproc)" \
    && make install \
    && make install-html

# Optional: run the regression suite at build time with
#   docker build --target check ...
FROM build AS check
RUN make check

FROM debian:trixie-slim AS server

# emp_server itself needs nothing but libc; these are for the bundled `empire`
# terminal client, which is worth having inside the container for debugging.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libreadline8 libtinfo6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local /usr/local
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh

# Game data.  The server assumes exclusive access to this directory; nothing
# else may read or write it while emp_server is running.  See doc/backups.
VOLUME ["/usr/local/var/empire"]
EXPOSE 6665

# -d keeps the process in the foreground.  It also sets the internal-error
# handler to abort(), so -E must follow it to put that back.
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["/usr/local/sbin/emp_server", "-d", "-E", "nothing"]
