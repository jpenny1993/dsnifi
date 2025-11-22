# NDS NiFi Test App

This is a proof of concept demo application showing how 2+ Nintendo DS consoles can interact with each other using local wireless communication.

The code to allow NiFi on the NDS originated from [CTurt/dsgmLib](https://github.com/CTurt/dsgmLib). I've refactored it into a clean event-driven library and integrated it into a fork of the devkitpro dswifi library.

## What is NiFi?

NiFi (Near Field Communication) enables DS-to-DS multiplayer without requiring a WiFi access point. The DS runs in promiscuous WiFi mode to send/receive packets directly between devices on the same channel.

## Library Design Philosophy

The NiFi library uses **circular packet buffers** by design:
- Fixed memory usage (critical on NDS with only 4MB RAM)
- Old unprocessed packets are overwritten when buffers fill
- Optimal for real-time games where stale data should be discarded
- Similar to UDP: fast and lossy, prioritizing recent events over old ones
- You may see "Overwriting packet" warnings in high-traffic scenarios - this is expected

For most real-time multiplayer games (racing, action, platformers), dropping old position updates is better than queuing them and introducing lag.

## Prerequisites

1. Install [devkitpro](https://devkitpro.org/wiki/Getting_Started) v3.0.3 (latest)
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

## Using the NiFi Library in Your Game

The library provides an event-driven API with 9 callback hooks:

### Basic Setup
```c
// 1. Register your event handlers
NiFi_OnRoomAnnounced(OnRoomAnnounced);
NiFi_OnJoinAccepted(OnJoinAccepted);
NiFi_OnClientConnected(OnClientConnected);
// ... register other handlers as needed

// 2. Initialize NiFi
NiFi_Init(wifiChannel, timerId, "GAME");

// 3. Create or join a room
NiFi_CreateRoom();  // Host
// or
NiFi_ScanRooms();   // Client (auto-calls OnRoomAnnounced when found)
```

### Event Hooks

| Hook | When Called | Use Case |
|------|-------------|----------|
| `OnRoomAnnounced` | Room discovered during scan | Show available rooms, auto-join |
| `OnJoinAccepted` | Successfully joined room | Initialize game state |
| `OnJoinDeclined` | Join rejected (room full) | Show error message |
| `OnClientConnected` | Player joins room | Spawn player avatar |
| `OnClientDisconnected` | Player leaves room | Remove player avatar |
| `OnDisconnected` | You are kicked/disconnected | Return to main menu |
| `OnHostMigration` | New host selected | Update UI, pause game |
| `OnPositionUpdated` | Position broadcast received | Update player position |
| `OnGamePacket` | Custom packet received | Chat, items, game events |

### Sending Custom Game Events
```c
// Example: Send a chat message
NiFiPacket packet;
NiFi_SetPacket(&packet, "CHAT_MSG");
strcpy(packet.data[0], playerName);
strcpy(packet.data[1], "Hello!");
NiFi_SendBroadcast(&packet, NULL);

// Example: Handle in OnGamePacket
void OnGamePacket(NiFiPacket packet) {
    if (strcmp(packet.command, "CHAT_MSG") == 0) {
        printf("%s: %s\n", packet.data[0], packet.data[1]);
    }
}
```

### Real-Time Position Sync
```c
// Send position (called from main loop)
Position pos = {touchX, touchY, 0};
NiFi_BroadcastPosition(pos);

// Receive position (automatic via OnPositionUpdated hook)
void OnPositionUpdated(Position pos, u8 clientIndex, NiFiClient client) {
    players[clientIndex].x = pos.x;
    players[clientIndex].y = pos.y;
}
```

### Important Notes
- All event handlers run in **interrupt context** (from hardware timer)
- Keep handler code fast and simple to avoid blocking network updates
- Access to `clients[]` array for all connected player info
- `localClient` pointer for your own client data
- Packet data: up to 6 parameters, 32 chars each
