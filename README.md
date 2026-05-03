# tcxNozzle

> This codebase is currently in its AI-slob prototyping phase: the code runs on momentum, vibes, and plausible intent.
> Proper debugging will be introduced once demand graduates from hypothetical to measurable.

[Nozzle](https://github.com/nozzle-io/nozzle) GPU texture sharing addon for [TrussC](https://github.com/TrussC-org/TrussC).

Share textures between TrussC applications via nozzle's cross-process GPU sharing (Metal/IOSurface on macOS, D3D11 on Windows, DMA-BUF/OpenGL on Linux).

## Disclaimer / Notice

This library is currently a work in progress and contains many incomplete features and unverified implementations.
Although it may appear usable at first glance, it may not function correctly.

Please use it with the understanding that no guarantees are made regarding its behavior, and perform debugging, validation, and review as needed.
If you encounter problems, please do not become angry; instead, contributions in the form of Issues or Pull Requests would be greatly appreciated.

## Usage

### Sender

```cpp
#include "tcxNozzle.h"

tcx::NozzleSender sender;
sender.setup("myApp", width, height);

// Send from Pixels
sender.send(pixels);

// Send from FBO
sender.send(fbo);
```

### Receiver

```cpp
#include "tcxNozzle.h"

// Discover senders
auto senders = tcx::NozzleReceiver::findSenders();

// Connect
tcx::NozzleReceiver receiver;
receiver.connect("myApp");

// Receive into Texture
tc::Texture tex;
if (receiver.receive(tex)) {
    tex.draw(0, 0);
}
```

## Build

Add to your TrussC project's `addons.make`:
```
tcxNozzle
```

Or use as a CMake subdirectory. nozzle is included as a submodule.

## API

| Class | Key Methods |
|-------|-------------|
| `tcx::NozzleSender` | `setup()`, `send(Pixels&)`, `send(Fbo&)`, `send(data, w, h)` |
| `tcx::NozzleReceiver` | `findSenders()`, `connect()`, `receive(Pixels&)`, `receive(Texture&)` |

## License

MIT

Third-party dependencies:

- [nozzle](https://github.com/nozzle-io/nozzle) — MIT
