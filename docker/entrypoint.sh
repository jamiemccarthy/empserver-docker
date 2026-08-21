#!/bin/sh
# Create the world on first boot, then exec the server.
#
# Everything here is idempotent: if /usr/local/var/empire/sector already
# exists, the volume holds a live game and we leave it alone.
#
# Country creation is NOT done here.  newcap_script is left in the config
# directory for the gateway to run over the protocol as POGO, so that country
# passwords are generated outside the image and never land in a build layer.
set -e

data=/usr/local/var/empire
etc=/usr/local/etc/empire

case "$1" in
*emp_server) ;;
*) exec "$@" ;;                 # utility invocations skip world creation
esac

if [ ! -e "$data/sector" ]; then
    echo "entrypoint: no world in $data, creating one"

    # fairland NC SC [NI] [IS]: number of islands must be a multiple of the
    # number of continents.
    : "${EMPIRE_CONTINENTS:=6}"
    : "${EMPIRE_CONTINENT_SIZE:=30}"
    : "${EMPIRE_ISLANDS:=12}"
    : "${EMPIRE_ISLAND_SIZE:=2}"

    if [ ! -e "$etc/econfig" ]; then
        echo "entrypoint: no $etc/econfig, refusing to guess" >&2
        exit 1
    fi

    /usr/local/sbin/files -f
    # shellcheck disable=SC2086
    /usr/local/sbin/fairland -q \
        ${EMPIRE_SEED:+-R $EMPIRE_SEED} \
        -s "$etc/newcap_script" \
        "$EMPIRE_CONTINENTS" "$EMPIRE_CONTINENT_SIZE" \
        "$EMPIRE_ISLANDS" "$EMPIRE_ISLAND_SIZE"
    echo "entrypoint: world created; $etc/newcap_script is ready to run"
fi

exec "$@"
