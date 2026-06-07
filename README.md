# tcxNozzle

A [TrussC](https://github.com/TrussC-org/TrussC) addon for **cross-process GPU
texture sharing**, built on top of [nozzle](https://github.com/nozzle-io/nozzle).

Share textures between separate applications on the same machine — a Syphon
(macOS) / Spout (Windows) style workflow, exposed through a small TrussC-flavored
API (`NozzleSender` / `NozzleReceiver`).

> **Status:** work in progress. The addon builds on macOS, Windows and Linux.
> On **macOS (Metal)** and **Windows (D3D11)** the sender blits straight into
> nozzle's shared texture on the GPU; the receiver currently reads frames back
> through a CPU copy. nozzle itself is early-stage and uses **its own protocol —
> it is *not* wire-compatible with Syphon or Spout**, so a nozzle sender is only
> visible to other nozzle/tcxNozzle apps.

---

## Install

This addon vendors nozzle as a git **submodule**, so clone recursively:

```bash
git clone --recursive https://github.com/nozzle-io/tcxNozzle
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

// GPU blit into nozzle's shared texture on Metal/D3D11 (CPU fallback otherwise)
sender.send(texture);
sender.send(fbo);

// CPU path (uploads raw pixels)
sender.send(pixels);
sender.send(data, width, height, /*channels=*/4);
```

### Receiver

```cpp
#include <tcxNozzle.h>

// discover what's currently being shared
for (auto& s : NozzleReceiver::findSenders()) {
    logNotice() << s.name << " (" << s.application_name << ")";
}

NozzleReceiver receiver;
receiver.connect("myApp");

Texture tex;
if (receiver.receive(tex) && receiver.isFrameNew()) {
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
| `NozzleReceiver` | `findSenders()` *(static)*, `connect(name)`, `connect(NozzleSenderInfo&)`, `disconnect()`, `isConnected()`, `receive(Texture&)`, `receive(Pixels&)`, `isFrameNew()`, `getSenderName()`, `getWidth()`, `getHeight()`, `getFormat()` |
| `NozzleSenderInfo` | public fields: `name`, `application_name`, `format`, `semantic_format` |

`send(Texture&)` / `send(Fbo&)` blit into nozzle's shared texture on Metal/D3D11
and fall back to a CPU pixel copy when a native blit isn't available.
`send(Pixels&)` / `send(data, ...)` are always the CPU path. The receiver reads
frames back through a CPU copy (`receive(Texture&)` uploads them into the
texture for you).

---

## Examples

| Example | What it shows |
|---------|---------------|
| [`example-sender`](example-sender)     | Draws an animated pattern into an Fbo and publishes it every frame. |
| [`example-receiver`](example-receiver) | Discovers senders, connects to one, and draws the received texture. |

Run `example-sender` first, then `example-receiver`.

---

## License

MIT — see [LICENSE](LICENSE).

Third-party dependencies:

- [nozzle](https://github.com/nozzle-io/nozzle) — MIT
- [plog](https://github.com/SergiusTheBest/plog) (via nozzle) — MIT
