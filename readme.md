# Doremote
The main purpose of this project is to create a Windows DLL providing C functions to communicate with 
the music notation software *Dorico* over its WebSocket interface (Dorico Remote Control API). 
The current version is primarily a proof of concept and implements only a small portion of the API.

## Demo Applications
To test the functions of the DLL, this project also provides a simple GUI application and a command-line program which
allow for sending commands to Dorico. The following videos show short sessions with both demo applications.

### GUI Demo

https://github.com/user-attachments/assets/956531d7-46a8-4887-8d5a-a264f89134dd

### Command-line Demo

https://github.com/user-attachments/assets/0fc92180-ef3d-4536-8440-778139b644f1

### Doremote Deck
<img align="right" width="150" src="https://github.com/user-attachments/assets/4b52009f-38fd-4cab-8f29-2c30d7dfd0fb" />

Another application of the library is the Stream Deck plugin [Doremote Deck](https://github.com/mgieseki/doremote-deck). 
It provides actions for sending commands to Dorico which can be assigned to Stream Deck buttons. The commands are not
hard-coded but must be entered into corresponding text fields of the property inspector. This way you can assign any of the
Dorico commands or command sequences to as many Stream Deck buttons as you like and create your own collections and layouts.

## Build Requirements
The core library is developed in C++20 using [Boost.Beast](https://www.boost.org/doc/libs/latest/libs/beast/doc/html/index.html)
to implement the WebSocket communication. Thus, a recent version of [Boost](https://www.boost.org) must be accessible by the
build environment. Since the coroutine implementation of Boost.Beast does not currently seem to support MinGW (GCC or Clang),
I recommend using the free Community Edition of MS Visual Studio.

If you'd also like to build the demo applications, Qt (Community Edition) and [replxx](https://github.com/AmokHuginnsson/replxx)
are required as well.

## Disclaimer
Dorico is a registered trademark of Steinberg Media Technologies GmbH in the European Union,
United States of America, and other countries, used with permission.
This project is not affiliated with Steinberg Media Technologies GmbH in any way.
