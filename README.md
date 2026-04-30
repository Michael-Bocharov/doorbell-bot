# DoorBell — Telegram Bot Integration

This project is an ESP32-S3-Zero based firmware that bridges a **generic intercom/doorbell system** to **Telegram**. It allows you to receive notifications on your phone when someone rings the doorbell and remotely open the door by replying with a simple command.

## 🚀 Features

- **Instant Notifications**: Sends a Telegram message immediately when the doorbell rings.
- **Remote Unlock**: Reply `open` or `/open` to the bot to activate the door relay.
- **Whitelist Security**: Only authorized users can open the door. The Admin can add/remove users via Telegram commands.
- **Visual Feedback**: Onboard WS2812 RGB LED indicates the current system status (Connecting, Online, Ringing, Error).
- **No Extra Backend**: Communicates directly with the Telegram Bot API over HTTPS. No MQTT broker, separate backend server, or custom mobile app required.
- **Debouncing**: Built-in 5-second debounce to prevent spamming notifications if the doorbell is pressed multiple times quickly.

## 🔌 Hardware Setup

You will need an **ESP32-S3-Zero** (or similar ESP32-S3 board) connected as follows:

| Component | ESP32-S3 Pin | Description |
|-----------|--------------|-------------|
| Ring detector | `GPIO 4` | Input from doorbell ring signal (pulled down internally). |
| Door relay | `GPIO 45` | Output to door lock relay (active high for 2 seconds). |
| WS2812 LED | `GPIO 21` | Onboard status indicator (varies by board, check your schematic). |
| BOOT button | `GPIO 0` | Long-press for 5 seconds to factory reset (erase NVS credentials). |

## 🚦 LED Status Indicators

| Color | Pattern | Meaning |
|-------|---------|---------|
| 🟡 Yellow | Blink | Connecting to WiFi |
| 🟢 Green | Solid | Online, polling Telegram for commands |
| ⚪️ White | Fast blink | Doorbell is ringing! |
| 🔴 Red | Blink | Error or Factory Resetting |

## 🛠️ Software Installation

This project is built using the [ESP-IDF framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/).

### 1. Create a Telegram Bot

1. Open Telegram and search for **@BotFather**.
2. Send `/newbot` and follow the prompts to create your bot.
3. Copy the **Bot Token** (it looks like `123456789:ABCdefGHIjklMNOpqrSTUvwxYZ`).
4. Create a new Telegram Group or use your personal chat with the bot.
5. Get your **Chat ID** (a numeric value, e.g., `123456789` or `-100123456789` for groups) using **@userinfobot** or **@RawDataBot**. *Note: The bot cannot send messages to its own username.*

### 2. Configure Firmware

```bash
cd firmware_esp32s3zero
# Source your ESP-IDF environment, e.g.:
. ~/esp/esp-idf/export.sh
idf.py menuconfig
```

Navigate to **DoorBell Configuration** and set:
- **WiFi SSID**
- **WiFi Password**
- **Telegram Bot Token**
- **Telegram Chat ID**
- **Telegram Admin User ID**: (Required for whitelist functionality). Only this numeric user ID will be able to add/remove other users. You can get your User ID from @userinfobot.

*(Alternatively, you can modify `firmware_esp32s3zero/main/Kconfig.projbuild` default values if you prefer, though menuconfig is recommended to keep secrets out of source control).*

### 3. Build & Flash

```bash
idf.py build
idf.py -p /dev/tty.usbmodem* flash monitor
```

## 🏗️ Architecture

- **`main.c`**: Entry point. Initializes NVS, establishes WiFi connection, and spawns the network-ready task. Also handles the BOOT button factory reset.
- **`telegram_bot.c/h`**: Implements the HTTPS client for the Telegram Bot API. It uses `sendMessage` to push notifications and long-polls `getUpdates` (every 2s with a 5s timeout) to receive commands, utilizing the ESP-IDF `esp_http_client` and the mbedTLS certificate bundle.
- **`doorbell_logic.c/h`**: Manages GPIOs. Polls GPIO 4 for the ring signal (with a 5s debounce) and pulses GPIO 45 for 2 seconds when an `open` command is received.
- **`led_status.c/h`**: Drives the onboard WS2812 RGB LED via the RMT peripheral to reflect system states.

## 💬 Telegram Commands

Once the device is online (Green LED), you can interact with it via Telegram:

**General Commands (Requires Authorization):**
| Command | Action |
|---------|--------|
| `open` or `/open` | Triggers the door relay for 2 seconds to unlock the door. |

**Admin Commands (Only accepted from Admin ID):**
| Command | Action |
|---------|--------|
| `/add <user_id>` | Adds a Telegram User ID to the whitelist so they can open the door. |
| `/remove <user_id>` | Removes a User ID from the whitelist. |
| `/list` | Shows the list of currently authorized User IDs. |

## 📝 License

This project is open-source. Feel free to modify and adapt it to your needs!

## ⚠️ Disclaimer

The author is not responsible for any damage, property loss, or violation of home rules, building codes, or local laws caused by the use of this software or hardware modifications. You are modifying building infrastructure at your own risk. Please ensure you have permission to interface with the intercom system in your building.
