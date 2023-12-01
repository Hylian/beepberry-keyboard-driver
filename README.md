# Beepy Keyboard Driver for Radxa Zero

Fork with modified Makefile and Device Tree Overlay for Radxa Zero.

This driver has been validated on Armbian 23.11 Bookworm for Radxa Zero.

## Installation

### Dependencies
```
apt install linux-headers-current-meson64
```

### Build
```
make
```

### Install
```
make install
```

### Uninstall

*Warning*: If you've made additional modifications to `user_overlays` or 
`extraargs` in `/boot/armbianEnv.txt`, this may result in unexpected behavior.

```
make uninstall
```

