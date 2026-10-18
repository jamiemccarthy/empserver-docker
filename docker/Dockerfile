# Wolfpack Empire 4.4.1 server.
#
# glibc, not musl: the LWP thread package needs makecontext(3), which musl
# does not provide.  Without LWP the regression suite in tests/ cannot run.
#
#   docker build -f docker/Dockerfile --target server -t empserver:4.4.1 .
#   docker build -f docker/Dockerfile --target tools  -t empserver:tools .
#
# Two images come out of this file:
#   server  Empire's own programs, the C libraries they load, and nothing
#           else: no shell, no coreutils, no package manager.  What a game
#           container runs.
#   tools   a small Debian image with a shell, for playempire's short-lived
#           helper containers, which write config files into a game's volume
#           and read files back out.  They run with no network and one volume.

FROM debian:trixie-slim AS build

# libc6-dev is only a Recommends of gcc, so --no-install-recommends drops it and
# the build fails at "C compiler cannot create executables".  readline/ncurses
# are for the bundled terminal client, which the build makes but the server
# image doesn't ship.  zlib is linked statically into empire-backup.
RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc libc6-dev make perl groff-base libreadline-dev libncurses-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . /src

# The files the dist tarball generates are committed (see .gitignore), but COPY
# does not guarantee they land newer than their prerequisites, and Make.mk has
# remake rules that fire when they don't.  The autoconf ones would need
# autotools installed; the .tarball-version one (Make.mk:352) appends "-dirty"
# to the version the server reports.  Stamp them in dependency order.
RUN cd /src \
    && touch aclocal.m4 \
    && touch configure stamp-h.in config.h.in \
    && touch .tarball-version \
    && touch .dirty-stamp

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

# Assembles, under /rootfs, every file the server image holds.
FROM build AS rootfs

# The pre-update backup, and the image's /bin/sh.  See the comment at the top
# of empire-backup.c: emp_server runs its hook with system(3), which always
# starts /bin/sh, so /bin/sh is this program, and it runs only the backup.
RUN gcc -O2 -Wall -Wextra -static -o /usr/local/sbin/empire-backup \
      /src/docker/empire-backup.c -lz

# Empire's programs and the shared libraries they load, found with ldd.  The
# terminal client and empdump are left out: nothing in a game container uses
# them, and an empdump export holds every password in clear text.
RUN set -eu; \
    mkdir -p /rootfs/bin /rootfs/etc /rootfs/usr/local/sbin /rootfs/usr/local/share \
             /rootfs/usr/local/etc /rootfs/usr/local/var/empire /rootfs/backup; \
    programs="emp_server files fairland empsched pconfig empire-backup"; \
    for p in $programs; do cp -a "/usr/local/sbin/$p" /rootfs/usr/local/sbin/; done; \
    for p in $programs; do ldd "/usr/local/sbin/$p" || true; done \
      | awk '$2 == "=>" && $3 ~ /^\// { print $3 } $1 ~ /^\// { print $1 }' | sort -u \
      | xargs -I{} cp -L --parents {} /rootfs; \
    cp -a /usr/local/share/empire /rootfs/usr/local/share/; \
    cp -a /usr/local/etc/empire /rootfs/usr/local/etc/; \
    ln -s /usr/local/sbin/empire-backup /rootfs/bin/sh; \
    echo 'empire:x:10001:10001::/nonexistent:/usr/sbin/nologin' > /rootfs/etc/passwd; \
    echo 'empire:x:10001:' > /rootfs/etc/group; \
    chown 10001:10001 /rootfs/usr/local/var/empire /rootfs/backup

FROM debian:trixie-slim AS tools

# The helpers mount a game's volume at /volume.  A new named volume takes the
# owner of the directory it is mounted on, so /volume is owned by the same
# uid the server runs as, and the files a helper writes are the server's.
RUN groupadd --gid 10001 empire \
    && useradd --uid 10001 --gid empire --no-create-home --home-dir /nonexistent \
         --shell /usr/sbin/nologin empire \
    && mkdir /volume \
    && chown empire:empire /volume
USER empire

FROM scratch AS server

# Copied with the owners set in the rootfs stage.
COPY --from=rootfs /rootfs /

# Everything runs as an unprivileged user with a fixed uid, so whatever runs
# inside the container can't use root's rights.  The server needs none: it
# listens above 1024, never changes user, and after chdir() to the data
# directory writes nothing anywhere else (nor do `files` and `fairland`).
#
# The directories it writes are owned by that user.  An empty named volume
# mounted on one takes the directory's owner the first time, so a new game's
# volumes start out writable with no extra step.
#   /usr/local/var/empire  game data
#   /backup                where empire-backup writes archives
# The config directory stays root-owned: the server only reads it.
#
# A host directory mounted as the data directory keeps the host's owner.
# Docker Desktop and Rancher Desktop on macOS let the container write to it
# anyway; on a Linux host, `chown -R 10001:10001` it first, or run with
# `--user 10001:10001` after making it writable to that uid.
USER empire

# Game data.  The server assumes exclusive access to this directory; nothing
# else may read or write it while emp_server is running.  See doc/backups.
VOLUME ["/usr/local/var/empire"]
EXPOSE 6665

# There is no entrypoint script, because there is no shell to run one.  A new
# world is made by running `files -f` and then `fairland` in their own
# short-lived containers before the server first starts (playempire's
# GameProvisioner does this).
#
# -d keeps the process in the foreground.  It also sets the internal-error
# handler to abort(), so -E must follow it to put that back.
CMD ["/usr/local/sbin/emp_server", "-d", "-E", "nothing"]
