# Crack the Safe

A safe-cracking game for the **Waveshare ESP32-S3-Knob-Touch-LCD-1.8**: a
360Ã—360 round touchscreen inside a detent knob, with a DRV2605 haptic motor.

You find the combination by feel. Turn the knob right until it **clicks** in
your hand, then reverse right there to lock that number in. Find the second
number turning left, the third turning right, and the vault door swings open
on a pile of glowing gold.

Turn past a number, or reverse anywhere else, and you lose: red and blue police
lights flash across the dial, then a new game starts with fresh numbers and the
dial back at 0.

## How to play

Beat the clock. The centre of the dial counts down the time left -- from your
first turn of the dial, not from when the code is dealt:

| Level | Time | Prize |
|---|---|---|
| 1 | 5:00 | Coins |
| 2 | 2:30 | Silver |
| 3 | 1:00 | Gold |
| 4 | 0:30 | Cash |
| 5 | 0:20 | Gemstones |
| 6 | 0:20 | Pearls and jewellery |
| 7 | 0:20 | A crown |
| 8 | 0:20 | A diamond |

The level table is `main/levels.c`; levels 6-8 also have false clicks.

Open the safe to go up a level; after level 8 you start again at level 1. The time halves
until level 4, then stays at 20 seconds; from level 5 the buzz starts closer to the number
instead (8 detents out, down to 4 at level 8), so you have to feel for it. The countdown turns
red for the last ten seconds. Running out of time, like turning past a number, is a loss: the
screen shows **Time's Up** (or **Wrong**), then the same level starts again with fresh
numbers.

| Step | Do this | You feel |
|---|---|---|
| 1 | Turn **right**. As you near the number the knob buzzes, stronger each detent | A steady buzz that grows, full on the number |
| 2 | Reverse at full strength: number 1 is locked | Double click |
| 3 | Turn **left** toward number 2, reverse at full strength | Growing buzz, full, double click |
| 4 | Turn **right** to number 3 and **stop** on it (or reverse): the safe opens | Growing buzz, full, then a rising rumble |

- Each number you hit also gives a quiet click through the 3.5 mm jack. It is a line-level
  output, so plug in headphones or a powered speaker to hear it -- or pair Bluetooth
  headphones (below).
- The buzz switches on 10 detents out and **holds**, getting stronger with every detent closer,
  then goes to full strength on the number. Turning the wrong way, it fades. It stops by itself
  if you leave the knob alone for 4 seconds.
- **Turn slowly.** Below about 4 detents a second you always feel the full buzz and click on the
  number. The faster you turn, the more likely you land on it with no feedback at all -- and
  carry on past it, which loses.
- Turn past the number (felt or not), or reverse anywhere else once committed, and you lose:
  the buzz stops dead, red and blue police lights swap across the dial for two seconds, and a
  siren wails in time with them. Then a **new game** starts: fresh numbers, the dial back at 0.
- The dial has 40 positions. The combination is 3 numbers, each at least 5
  positions from the one before it.
- **Touch and hold** the screen for a second, on any game screen, for the menu: **New Game**
  (back to level 1, playing straight away), **Settings**, or **Back** to carry on. The clock
  stops while the menu is open.
- **Settings â†’ Volume** sets how loud the click and the siren are: **Off**, then 1 to 10
  (3 to start with). Touch **-** or **+**, or turn the knob while Settings is open; each step
  plays a click at the new level. It applies to the 3.5 mm jack and to Bluetooth headphones,
  and the board remembers it when switched off.
- The battery icon above **Level** shows how much charge is left, and turns red when it is
  low (15% or less). With USB plugged in a green bolt appears and the bars fill up over
  and over. The board cannot see the battery while it is on the charger, so it shows the
  same thing whether the battery is still charging or already full.
- Turn the knob or tap the title screen to see how to play, then touch **Start** to begin.
  The knob does nothing on the instructions screen. After the vault opens, wait for
  **Turn to Play Again** before starting the next round.

### Bluetooth headphones

Touch **Headphones** on the instructions screen, or touch and hold during a game and pick
**Settings â†’ Headphones**. Put your headphones in pairing mode; the board searches by itself
and lists what it finds (turn the knob or swipe to scroll). Tap yours to connect, then
**Done**. The Headphones button turns green while they are connected.

AirPods: put both in the case, open the lid, and hold the button on the back of the case
until the light flashes white. If they still do not show up, another Apple device has
probably claimed them: turn Bluetooth off on your iPad, Mac or Watch, or reset the
AirPods (hold the case button about 15 seconds, through flashing amber, until it flashes
white -- you will need to pair them with your phone again afterwards).

- While Bluetooth headphones are connected the click plays through them and the 3.5 mm jack
  goes quiet. The board cannot tell whether anything is plugged into the jack (there is no
  plug-detect line), so connecting is what switches the sound over.
- The board remembers the last headphones (always at the top of the list) and reconnects to
  them when they are switched on. To forget them, touch the **(i)** on their row, then
  **Forget Device**.
- Bluetooth audio arrives roughly 150-300 ms late. The buzz in the knob does not, so trust
  your fingers -- like a real safecracker.
- Bluetooth needs the co-processor firmware in `coprocessor/` (see
  [docs/flashing.md](docs/flashing.md)). With the factory firmware still on that chip, the
  screen says **Bluetooth is not available**. The board and the co-processor must run matching
  firmware (version 3): they talk at 921600 baud, fast enough to stream the vault's sounds
  to the headphones as they play. Flash both together; with an older co-processor the
  screen says **Bluetooth is not available**.

## Modes, records and the guard

Touch and hold → **Mode** picks how rounds are played:

| Mode | What it is |
|---|---|
| **Classic** | The levels (`main/levels.c`). Progress is saved: Start carries on from the level you reached; **New Game** goes back to 1. From level 6 a **false click** waits a few detents before each number -- reverse on it and you lose. |
| **Practice** | No clock. The centre shows the detents still to go; going past the number (or reversing early) only buzzes -- back up and try again. |
| **Daily** | Today's code, the same on every knob (from the date over Wi-Fi). 60 s, false clicks. Best time and tries are kept for the day. |
| **Challenge** | A code a friend sets on the settings page; it is used up once cracked. |

**Guard** (on the Mode screen, or the settings page): every 12-25 s of play a
watchman comes by -- soft footsteps in the knob and "Footsteps...", then
"FREEZE!" for 2.5 s. Turn the dial then and you are **Caught**. With the
microphone on he also hears you: the board's PDM mic (GPIO45/46) compares the
sound against the room's own level.

**Records.** After each safe: the time of each number and the total
(speedcubing splits), "New Record!" on a best time for the level. The
settings page shows progress, wins and losses (and how: time, guard, which
number), best time per level, ao5 / ao12, the last 12 solves with splits and
a graph, and the Daily. Everything lives in NVS (`main/stats.c`).

**Battery.** A minute without a touch or a turn dims the screen; after 5
minutes (settings page: 2-30 or never) the knob goes into deep sleep and
wakes when the dial is turned. On USB it only dims.

## Under the Knob Launcher

The game also runs under the
[Knob Launcher](https://github.com/EvgeniChak/knob-launcher), which downloads
it from this repository's releases into one of its game slots.

- **Back to the launcher:** touch and hold for the menu → **Launcher**, or keep
  holding anywhere for 4 seconds (a ring fills up; let go to stay). A reset or
  power cycle also lands in the launcher; waking from deep sleep goes straight
  back to the game.
- **Wi-Fi and the settings page** (stats, Challenge) work as before, on the
  same saved network. **Updates** are the launcher's job there: the page and
  the knob's Wi-Fi screen say so and hide Check / Install / upload, because
  the other app slot belongs to another game.
- **Volume** is shared with the launcher's Settings; brightness comes from it.
- Releases must stay under the launcher's 6.4 MB slot -- the release workflow
  checks.

## Wi-Fi and updates

The knob updates itself from this repository's GitHub releases.

**First setup.** With no network saved the knob opens an open access point,
`Knob-XXXX`. Join it from a phone, open http://192.168.4.1, pick your 2.4 GHz
network and enter the password. From then on the settings page is at
http://knob.local (or the IP shown on the knob).

**On the knob.** Touch and hold (on any screen, the title included) → **Settings → Wi-Fi & Update**: network,
address, firmware version, **Check / Install**, and **Setup Wi-Fi** to reopen
the access point. When a newer release turns up while the title screen is
showing, the knob offers it: **Install** or **Later**.

**Settings page.** Network, version, check and install, and upload of your
own `crack_the_safe.bin` (for testing a local build without a release).

**Publishing a release.**

```bash
git tag v1.2.0
git push origin v1.2.0
```

`.github/workflows/release.yml` builds with ESP-IDF 5.5.2 and publishes the
release with `version.json` (how the knob learns the newest version -- not
the GitHub API, whose 60-requests-an-hour limit is shared by everything behind
one router), `crack_the_safe.bin` (what the knob downloads) and
`crack_the_safe_full_16MB.bin` (bootloader + partitions + app, for a first
flash over USB: `esptool write-flash 0 crack_the_safe_full_16MB.bin`). The
knob compares the tag with its own version (`git describe` for local builds)
and only offers newer ones.

**Safety.** The flash has two app slots (`partitions.csv`). A new image boots
on probation and is confirmed once the game is up; if it crashes before that,
the bootloader goes back to the previous one.

**Size.** Each slot is 7.94 MB. The door frames, prizes and police lights
are 6.7 MB as raw pixels, so they are stored LZ4-compressed (2.6 MB) and
unpacked into PSRAM when drawn. After every EEZ Studio export (which writes
them raw again) run:

```bash
pip install lz4
python tools/compress_images.py
```

The build stops with that hint if it finds an uncompressed image.

## Building and flashing

The board has **two microcontrollers** and this project has firmware for both.
**[docs/flashing.md](docs/flashing.md) is the full step-by-step guide**: the
cable, how to tell which chip is connected, backing up the factory firmware,
and troubleshooting.

| Chip | Runs | Firmware | Shows up as (Linux) |
|---|---|---|---|
| ESP32-S3 | the game | project root | `/dev/ttyACM0` -- USB ID `303a:1001` Espressif |
| ESP32-U4WDH | Bluetooth headphones | `coprocessor/` | `/dev/ttyUSB0` -- USB ID `1a86:7523` CH340 |

There is one USB-C port, and **which way up the plug is decides which chip it
reaches**. To switch chips, unplug the cable from the board, turn the plug
over and plug it back in. Check which one answered before flashing:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null      # ACM = the S3, USB = the co-processor
esptool.py --port /dev/ttyACM0 chip_id        # "Chip is ESP32-S3"
```

You need ESP-IDF 5.4 or newer (tested on 5.5.2), and a USB-C cable that
carries data.

**The game (ESP32-S3)** -- plug in so `/dev/ttyACM0` appears:

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32s3     # first build on a fresh clone
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

**Bluetooth (ESP32-U4WDH)** -- turn the plug over so `/dev/ttyUSB0` appears.
This replaces Waveshare's factory firmware on that chip, so back it up first
(once; the image is specific to your board):

```bash
mkdir -p firmware_backup
esptool.py --port /dev/ttyUSB0 -b 921600 read_flash 0 ALL firmware_backup/esp32_u4wdh_factory_4MB.bin
cd coprocessor
idf.py set-target esp32       # first build on a fresh clone
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

The S3 has no Classic Bluetooth, which headphones need, so the second chip
does that part: it finds and pairs headphones and plays the game's sounds to
them. The S3 talks to it over a UART (S3 GPIO38 -> ESP32 IO18, ESP32 IO23 ->
S3 GPIO48); the protocol is described at the top of `coprocessor/main/main.c`.
It also holds the DAC's soft-mute line, which is why the 3.5 mm jack goes quiet
while headphones are connected.

## Changing the screens

The GUI is authored in **EEZ Studio** (0.29 or newer):
`GUI/CrackTheSafe.eez-project` is the source of truth.

1. Open `GUI/CrackTheSafe.eez-project` in EEZ Studio.
2. Pick the **Dark** theme in the theme dropdown. The firmware runs dark.
3. Make your changes and press **Build** (Ctrl+B).
4. `idf.py build`.

Build exports into `main/ui/`. That folder holds only generated files: delete
it whenever you like and export again. Never edit files in it by hand; the next
export overwrites them. With `main/ui/` missing, the firmware still builds, as
a placeholder that asks for an export.

The hand-written C that the screens call lives beside it in `main/`:

| File | What it does |
|---|---|
| `game.c` | The rules. No LVGL, no hardware. |
| `app.c` | Knob and tap into the game, haptics, screen changes, the unlock timeline. |
| `sound.c` | The click and the siren, the volume setting (saved in NVS), and whether sound goes to the jack or the headphones. |
| `ui_game.c` | What the screens show: text, style states and the hidden flag, from the game state. |
| `actions.c` | The actions the screens call: `Tap`, `Start`, `Menu`, and the Menu, Settings (Volume among them) and Headphones buttons. |
| `bt_link.c` | Bluetooth headphones: the line protocol to the co-processor. |
| `main.c` | Boot. |

The C code sets text, picks style **states** and shows or hides widgets. It
never positions, sizes or restyles one. Every look a widget can take is
authored as a state in its style: `CHECKED` for a locked-in number, `DISABLED`
for "wrong" (red). So what EEZ Studio's canvas shows is what the panel shows.
The dial's rotation is set from C the same way, as a live value; the canvas
shows the dial at rest on 0.

### The vault door

The door opening is a 16-frame sprite (`GUI/ASSETS/art/door_00.png` â€¦
`door_15.png`), rendered from a generated vault-door image with a perspective
swing about its hinge. Each frame is its own image widget on `PageUnlocked`,
placed where that frame sits on screen. The firmware shows one frame at a
time. The canvas shows the last frame, the door standing open, because that
is where the screen settles.

The gold glow is an additive-blend image that appears once the door has
opened far enough to see inside. It then steps through three brightness
states of its style, so it breathes without the firmware ever setting an
opacity.

## Hardware

All board support is in `components/knob_board/`. `src/board_pins.h` has
every pin, each cited to Waveshare's demo code or the schematic.

| Part | Interface |
|---|---|
| ST77916 360Ã—360 LCD | QSPI: CS 14, CLK 13, D0â€“D3 15â€“18, RST 21; backlight PWM on 47 |
| CST816 touch | I2C0 (SDA 11, SCL 12) at 0x15, INT 9, RST 10 |
| DRV2605L haptics | I2C0 at 0x5A, driving an **LRA** open loop (~199 Hz), LRA library 6 |
| Knob | Alps bidirectional detector switch, *not* quadrature: A on 8 (clockwise), B on 7 |
| PCM5100A DAC | I2S1: BCK 39, WS 40, DOUT 41; GPIO0 routes the DAC to the S3. Line-level out of the 3.5 mm jack -- **no amplifier, no speaker**. Soft mute is on the co-processor's IO32 |
| ESP32-U4WDH co-processor | UART1: TX 38, RX 48, 921600 baud. Classic Bluetooth for headphones (`coprocessor/`) |
| Battery sense | ADC1 channel 0 on GPIO1, half the supply rail (10k/10k). No charger status line, so USB power is read as "charging" (above 4.45 V) and the level is read off the cell voltage on battery |

The knob's direction, the motor type, the DRV2605's waveform library and the
LRA drive frequency can be changed in `idf.py menuconfig` â†’ **Knob board**.
Waveshare's demo drives the motor as an ERM; it is an LRA (the schematic
labels its nets `LRA_P`/`LRA_N`), and as an ERM it barely moves.

## Repository layout

```
GUI/CrackTheSafe.eez-project   the screens (source of truth)
GUI/ASSETS/                    fonts, and the vault art the project embeds
main/                          the game and the glue to the UI
main/ui/                       EEZ Studio's export -- disposable
components/knob_board/         display, touch, knob, haptics, battery, LVGL, co-processor UART
coprocessor/                   ESP32 co-processor firmware: Bluetooth headphones
docs/flashing.md               flashing both chips, step by step
```

## License

MIT, see [LICENSE](LICENSE). Based on [trailcurrentoss/YouTubeCodeSamples](https://github.com/trailcurrentoss/YouTubeCodeSamples/tree/main/crack_the_safe_waveshare_rotary_encoder_game). Roboto is under the Apache License 2.0 and
Font Awesome Free under the SIL OFL 1.1.
