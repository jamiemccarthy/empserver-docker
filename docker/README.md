### Here's what I do to build the necessary images, but you don't have to

# Build the empire4-builder image
docker build -f docker/Dockerfile.builder .
(generates jamiemccarthy/empire4-builder)

# Run the empire4-builder image to compile the source into target binaries and files, in a local directory
docker run --rm --mount type=bind,source=$PWD/docker/built-usr-local,target=/usr/local jamiemccarthy/empire4-builder:latest

# Then, build the empire4-server image, containing all the built files
docker build -f docker/Dockerfile.server .
(generates jamiemccarthy/empire4-server)

# Last, build the empire4-setup image, used to set up new games
docker build -f docker/Dockerfile.setup .
(generates jamiemccarthy/empire4-setup)

### Here's how to set up a game

# This runs the setup script in the setup image, creating a new game with the parameters you specify.
# Use any unique volume name in place of "empire_volume_1" (that volume will be destroyed if it exists!)
# (TODO: the TODO items in docker/setup.rb)
docker volume rm -f empire_volume_1
docker run -it --rm --mount type=volume,source=empire_volume_1,target=/usr/local/var/empire jamiemccarthy/empire4-setup:latest

# To check out the contents of a volume
docker run -it --rm --mount type=volume,source=empire_volume_1,target=/usr/local/var/empire alpine:3.6 sh

# This uses the volume you just set up to start a game.
# Use any unique game name for the container, in place of "empire_game_1"
# (TODO: instructions to set the port?)
# TODO: have Dockerfile.server start the empire server as its ENTRYPOINT
# (but Dockerfile.setup then shouldn't be based FROM it I guess, no biggie)
docker run --rm -p 6665:6665 --name empire_game_1 --mount type=volume,source=empire_volume_1,target=/usr/local/var/empire jamiemccarthy/empire4-server:latest

