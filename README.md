# Midnight screensaver

A bouncing-logo screensaver that works like the classic DVD player screensaver:

- black screen, one logo moving diagonally at a constant speed
- bounces off every edge, with no easing and no randomness in the path
- switches to a new, clearly different color on every bounce
- an exact corner hit is rare, as in the original

The logo is a slanted **MIDNIGHT** wordmark over a flat disc, laid out like the DVD logo.

## Install and set as your screensaver (Linux / X11, xscreensaver)

    git clone -b claude/elegant-mccarthy-iv2ib2 https://github.com/jaska666/betas.git
    cd betas

    # install xscreensaver, a C compiler and the X11 headers:
    sudo pacman -S --needed xscreensaver base-devel libx11                 # Arch / Manjaro
    sudo dnf install xscreensaver gcc make libX11-devel                    # Fedora
    sudo apt install xscreensaver build-essential libx11-dev               # Debian / Ubuntu

    ./install.sh

`install.sh` builds the hack, installs it next to the other xscreensaver hacks
(or to `~/.local/bin`), makes it the selected program in `~/.xscreensaver`
(mode `one`), backs up your old config, and restarts the daemon.

## Try it without installing

    make && ./midnight          # windowed; press q or Esc to quit

Options: `-speed N` (pixels per frame on a 640 px wide screen, default 1),
`-fps N` (default 60), `-root`, `-window-id ID`.
