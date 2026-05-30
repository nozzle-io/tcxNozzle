# tcxNozzle

A [TrussC](https://github.com/TrussC-org/TrussC) addon for **cross-process GPU
texture sharing**, built on top of [nozzle](https://github.com/nozzle-io/nozzle).

Share textures between separate applications on the same machine — a Syphon
(macOS) / Spout (Windows) style workflow, exposed through a small TrussC-flavored
API (`NozzleSender` / `NozzleReceiver`).

> **Status:** work in progress. Verified working on **macOS** (Metal/IOSurface),
> including a full **GPU zero-copy round trip** (`send(Texture&)` →
> `receive(Texture&)`, no CPU readback either end), and on **Windows** (D3D11).
> nozzle itself is early-stage and uses **its own protocol — it is *not*
> wire-compatible with Syphon or Spout**, so a nozzle sender is only visible to
> other nozzle/tcxNozzle apps.

---

## Install

This addon vendors nozzle as a git **submodule**, so clone recursively:

```bash
git clone --recursive https://github.com/tettou771/tcxNozzle
# already cloned without --recursive?
git submodule update --init --recursive
```

Then list it in your project's `addons.make`:

```
tcxNozzle
```

A root `CMakeLists.txt` is provided (it pulls in the nozzle submodule and links
the platform frameworks), so TrussC builds it automatically.

---

## Usage

### Sender

```cpp
#include <tcxNozzle.h>

NozzleSender sender;
sender.setup("myApp");          // name other apps discover you by

// GPU zero-copy: blits straight into nozzle's shared texture
sender.send(texture);
sender.send(fbo.getTexture());

// CPU path (reads pixels back first)
sender.send(fbo);
sender.send(pixels);
sender.send(data, width, height, /*channels=*/4);
```

### Receiver

```cpp
#include <tcxNozzle.h>

// discover what's currently being shared
for (auto& s : NozzleReceiver::listSenders()) {
    logNotice() << s.getName() << " (" << s.getApplicationName() << ")";
}

NozzleReceiver receiver;
receiver.connect("myApp");

Texture tex;
if (receiver.receive(tex) && receiver.isFrameNew()) {  // GPU zero-copy when possible
    tex.draw(0, 0);
}
```

See [`example-sender`](example-sender) and [`example-receiver`](example-receiver)
for runnable apps — launch both, and the receiver picks up the sender's frames.

---

## API

| Class | Key methods |
|-------|-------------|
| `NozzleSender`   | `setup(name, w?, h?)`, `send(Texture&)`, `send(Fbo&)`, `send(Pixels&)`, `send(data, w, h, ch)`, `close()`, `isSetup()`, `getName()`, `getWidth()`, `getHeight()`, `getFrameCount()` |
| `NozzleReceiver` | `listSenders()` *(static)*, `connect(name)`, `connect(NozzleSenderInfo&)`, `disconnect()`, `isConnected()`, `receive(Texture&)`, `receive(Pixels&)`, `isFrameNew()`, `getSenderName()`, `getWidth()`, `getHeight()`, `getFormat()` |
| `NozzleSenderInfo` | `getName()`, `getApplicationName()`, `getId()`, `getBackend()` (public fields with getters, per the TrussC device-info convention) |

`send(Texture&)` and `receive(Texture&)` use a GPU-to-GPU blit into/out of
nozzle's shared texture on Metal/D3D11 — no CPU readback. When a native blit
isn't available (or the format doesn't map cleanly) they fall back to a CPU
pixel copy automatically. `send(Fbo&)` / `send(Pixels&)` / `receive(Pixels&)`
are always the CPU path.

---

## Examples

| Example | What it shows |
|---------|---------------|
| [`example-sender`](example-sender)     | Draws an animated pattern into an Fbo and publishes it every frame. |
| [`example-receiver`](example-receiver) | Discovers senders, connects to one, and draws the received texture. |

Run `example-sender` first, then `example-receiver`.

## Tests

[`tests`](tests) is a small headless harness (`tests/test_nozzle.cpp`) covering
sender/receiver defaults, setup, raw-pixel send, discovery, and a cross-process
round trip. Build & run it like any TrussC project:

```bash
cd tests
trusscli run     # builds the host app, then runs the headless tests
```

It also builds `gpu_roundtrip` — a small windowed app that draws a 4-quadrant
pattern, sends it via the GPU path, receives it via the GPU path, reads it back
and asserts the colors/orientation match. Run `tests/bin/gpu_roundtrip` (needs a
display / GPU) to verify GPU zero-copy end to end; it prints `[GPU-VERIFY] PASS`.

---

## License

MIT — see [LICENSE](LICENSE).

Third-party dependencies:

- [nozzle](https://github.com/nozzle-io/nozzle) — MIT
- [plog](https://github.com/SergiusTheBest/plog) (via nozzle) — MIT
