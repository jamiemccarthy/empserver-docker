## How the Dockerfiles are used

Each of the three `Dockerfile`s builds a docker image with a
specific purpose:

* The `builder` compiles the empire source into a directory full
of binaries.

* The `setup` uses instructions in a `.yml` data file to generate
a new volume that serves as the beginning of a new game.

* The `server` runs a game on such a volume. (It also contains the
client.)

## How to build the docker images

You probably don't have to do these steps, you could just use the images
on my Docker Hub. But if you want to, here's how I built those images.

First, build an image that will build the binaries, and tag it to refer
to it later:

```
$ docker build -f docker/Dockerfile.builder -t empire4-builder .
```

You can tag it whatever you want, but I'm suggesting `empire4-builder`
here. I've pushed and will continue to push these builds to Docker Hub
as `jamiemccarthy/empire4-builder`.

Having built the image, run it to compile empire and install it into a
fresh `docker/built-usr-local/` directory on the host. Compilation will
take a few minutes.

```
$ mkdir -p docker/built-usr-local
$ git clean -fdX docker/built-usr-local/
$ docker run --rm --mount type=bind,source=$PWD/docker/built-usr-local,target=/usr/local empire4-builder
```

That directory is really all there is to the `empire4-server` image.
Now that it's populated with the empire binaries, you can build the
server image:

```
$ docker build -f docker/Dockerfile.server -t empire4-server .
```

Finally, you can build the setup image, which is used to write out
volumes used to start new games.

```
$ docker build -f docker/Dockerfile.setup -t empire4-setup .
```

## How to set up a game

Pick any name you like for your new empire game volume as long as it's
not already in use (here, `empire-volume-1`). Run the setup script:

```
$ docker volume rm -f empire-volume-1
$ docker run --rm --mount type=volume,source=empire-volume-1,target=/usr/local/var/empire jamiemccarthy/empire4-setup:latest
```

If you'd like to manually look at the files to verify you've got a new game
ready to go, you can:

```
$ docker run -it --rm --mount type=volume,source=empire-volume-1,target=/usr/local/var/empire alpine:3.6 sh
/ # cd /usr/local/var/empire
/usr/local/var/empire # ls
ann        contact    loan       nation     plane      reject     ship
bmap       game       lostitems  news       power      relat      tel
commodity  land       map        nuke       realms     sector     trade
/usr/local/var/empire # exit
```

## How to start your game running

```
docker run --rm -d -p 6665:6665 --name empire-game-1 --mount type=volume,source=empire-volume-1,target=/usr/local/var/empire jamiemccarthy/empire4-server:latest /usr/local/sbin/emp_server -d
```

The container will run an empire game on the given volume, responding on
the given port, until it crashes (apparently empire is historically crashy)
or you stop it with `docker stop empire-game-1`.

If you want to see its logs, you can check the volume while or after the
container is running:

```
$ docker run -it --rm --mount type=volume,source=empire-volume-1,target=/usr/local/var/empire alpine:3.6 sh
/ # tail /usr/local/var/empire/server.log
Thu Dec 21 18:27:59 2017 ------------------------------------------------------
Thu Dec 21 18:27:59 2017 Empire server (pid 1) started
Thu Dec 21 18:27:59 2017 Update schedule read
Thu Dec 21 18:27:59 2017 Next update at Thu Dec 21 18:38:00 2017
Thu Dec 21 18:27:59 2017 Listening on 0.0.0.0
Thu Dec 21 18:29:21 2017 Shutdown commencing (cleaning up threads.)
Thu Dec 21 18:29:21 2017 Server shutting down on signal 2
/ # exit
```

## TODO

* the TODO items in setup.rb
* instructions to set the port - needed when running multiple games
* put emp\_server as the ENTRYPOINT for empire4-server
* clone from github instead of having the code in `/app/.git/` in empire4-builder?
