# NDS NiFi Test App

This is a proof of concept demo application showing how 2 Nintendo DS consoles can interact with each other using local wireless communication.

The code to allow NiFi on the NDS isn't mine, it originated from [CTurt/dsgmLib](https://github.com/CTurt/dsgmLib). I've just removed gamemaker wrapper and put it directly into a fork of the devkitpro dswifi library.

As a heads up for what this attempts to do is, run the DS in promiscuous mode and accept data from all WiFi available channels which isn't ideal.

## Prerequisites

1. Clone [jpenny1993/dswifi](https://github.com/jpenny1993/dswifi)
1. Open a terminal like msys2 in the root of the repo
1. Run the `make` command to build the dswifi library
1. Run the `make install` command to replace the stock dswifi library provided with devkitpro

## Build

1. Open a terminal in the root of the repo
1. Run the `make` command to build the .nds file

## Deploy

Copy the .nds file from the root of the repo onto your NDS flashcart

## Run

Launch the .nds application on both NDS consoles, tap the touch screen with your stylus and the co-ordinates of the touch event should be sent to the other NDS.
