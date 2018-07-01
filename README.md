# festivald

A festival speech synthesis server using systemd sockets.

## Getting started

### Dependencies

- A C++ compiler
- meson
- festivald needs the festival speech synthesis system and speech-tools.
- In case you want to run festivald as a daemon, you will also need either
  systemd (service and socket files are given) or to write an init script for
  your init system.

### Install

Clone the git repository and enter the cloned directory:

    git clone https://github.com/zeehio/festivald
    cd festivald

Configure the project. We will build it in the `build` directory:

    meson build .

Checkout the configure options

    meson configure build

To do a quick test you will probably want to do the installation on a local prefix. Note
that besides the `prefix` you will need to set `systemdsystemunitdir`:

    meson configure build -Dprefix=$PWD/install -Dsystemdsystemunitdir=$PWD/install/lib/systemd/system

Compile the project and install it:

    ninja -C build install

### Try it

Go to the installation directory and from a terminal use:

    cd install/bin
    ./festival --socket $PWD/festivald.socket

From another terminal use:

    cd install/bin
    ./festival_client --socket $PWD/festivald.socket

And then you can try to give Festival commands to the client:

    (SayText "Hello world")

## License

Licensed under the same license than festival, a MIT-like license.

