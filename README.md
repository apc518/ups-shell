# UPS Shell

A basic POSIX shell written in C

![upssh-pic](https://user-images.githubusercontent.com/56745633/139185753-39987ef3-5c4c-4847-bf95-7235d57926a2.png)

### Features
- cd command to change directory, either relative or absolute
- history command to list all commands issued in a session
- mypwd to print current working directory
- looks up commands it doesnt know in PATH

### Usage
```sh
curl https://raw.githubusercontent.com/apc518/ups-shell/master/upssh.c > upssh.c
gcc upssh.c -o upssh
./upssh
```

This was written for a homework assignment in CS 475: Operating Systems, at the University of Puget Sound.
