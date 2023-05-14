# Clipboard Share

Clipboard Share is a cross platform clipboard sharing deamon. Clipboars Share daemons can find each other in the same local network, and share clipboard content automatically when you hit `Ctrl+C` or `Command+C`.

## Warning

The current implementation of Clipboard Share is immature. Namely, the authentication and encryption is weak. It is recommended to use Clipboard Share in a trusted network environment. 

## Requirements on Linux
Make sure you are using Xorg and have `xclip` installed.

## Usage

### Build (Linux & MacOS)
```
git clone https://github.com/555isfaiz/clipboard_share.git
cd clipboard_share
make
```
### Run
```
clipboardshare -t <auth token> &
```
For more options, run
```
clipboardshare -h
```

## Future Plan

+ Stronger encryption
+ Fix Windows version
+ Wayland support