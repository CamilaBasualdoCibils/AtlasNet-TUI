# AtlasNet TUI

A full-screen FTXUI map connected exclusively through the AtlasNet Control Panel
client SDK. The `AtlasNet/` submodule contains the gateway, protocol and reusable
SDK. No Node or simulation objects are linked into the TUI.

## Build

Use a C++23 compiler and CMake 3.24+. With vcpkg:

```sh
git submodule update --init --recursive
cmake -S . -B build/tui -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/tui --target atlasnet-tui
./build/tui/atlasnet-tui --connect ws://127.0.0.1:8080
```

The TUI compiles and links the same standalone client library source, so it does not
configure Valkey, Node, simulation, or graphical example dependencies. FTXUI is
found through CMake/vcpkg, with a pinned FetchContent fallback.
The TUI build requires Boost.Beast, nlohmann-json, and FTXUI. uWebSockets and the
server transport tests belong to the AtlasNet Node build.

Build the node separately using AtlasNet's normal presets/dependencies. Both
Control Panel libraries compile by default; uWebSockets is a normal vcpkg dependency. A minimal node build is:

```sh
cmake -S AtlasNet -B build/node -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DATLASNET_EXAMPLES=OFF \
  -DATLASNET_TESTS=OFF -DATLASNET_DEV=OFF -DATLASNET_CARTOGRAPH=OFF
cmake --build build/node --target AtlasNetNode AtlasNetShardWorker
```

Start a gateway joining an existing cluster:

```sh
./build/node/AtlasNetNode --capabilities ControlPanel \
  --bootstrap 127.0.0.1:43000 \
  --control-panel-address 127.0.0.1 --control-panel-port 8080
```

The gateway can also be combined with other capabilities. ControllerEligible is
not required. Node YAML uses:

```yaml
node:
  capabilities: [ControlPanel]
  control_panel: {address: 127.0.0.1, port: 8080}
  bootstrap: {endpoints: ["127.0.0.1:43000"]}
```

The stack tool in `AtlasNet/tools/vscode-local-stack` includes the capability
checkbox and a listener port base. Each generated listener adds the global node
index to the base. Explicit YAML listeners are preserved and collisions checked.

## Controls

- Left drag pans immediately; the wheel zooms around the cursor.
- Hover over the map to see the region ID and its assigned shard. Hover over an
  entity to see its entity ID, current shard, position, and client ID when applicable.
- Tab/Space or mouse clicks toggle the three layer checkboxes.
- `r` reconnects and reloads metadata; `q` or Escape exits.
- Resizing recalculates the visible bounds.

Orange boundaries use the exact convex region geometry, clipped to the view.
Bold green `△` markers represent client entities. Other entities appear as blue
points. Client markers take precedence when entities share a terminal cell.
The visibility checkboxes sit above the map and wrap on narrow terminals; the
map spans the full available width.
Simulation bounds are green and disabled initially. Entity subscriptions cover a
40% margin around the viewport and update at most every 120 ms. A bounds change
keeps the subscription ID and replaces its revision with an atomic snapshot.
Disabling the entity layer unsubscribes; reenabling creates one subscription.
Sequence gaps trigger snapshot reconciliation on that same subscription.

## Tests

```sh
ctest --test-dir build/tui --output-on-failure
# In an AtlasNet build with ATLASNET_TESTS=ON:
cmake --build build/node --target AtlasNet_tests_ControlPanelTransport \
  AtlasNet_tests_ControlPanelGeometry AtlasNetControlPanelTestWorld \
  AtlasNetNode AtlasNetShardWorker
ctest --test-dir build/node -R 'ControlPanel' --output-on-failure
```

The cluster test starts an isolated temporary Valkey process, two simulation
nodes with a headless test module, and a gateway with only ControlPanel enabled.
It uses real cluster RPC, worker processes, WebSockets and registered entities.
It stops all test services afterward. Network tests require local socket access.
The headless test world is not used by the application.
Passing the TUI executable as a fourth argument to `tests/ControlPanel/Cluster.py`
also exercises the actual terminal application in a PTY, including hover details,
pan, zoom, full-width layout, and disabling/restoring the entity subscription.

## Initial limits

AtlasNet currently reserves/ignores WorldID in its spatial query system. The
gateway accepts world `0` and rejects other values explicitly. The map uses
Cartesian coordinates, honors XY/XZ/YZ for 2D partitions, and shows the z=0 XY
slice of 3D region geometry with entity positions projected onto XY. The hidden
entity axis uses a finite slab of ±1e30.

Heuristic geometry and simulation bounds refresh while connected. The camera
fits the simulation bounds when it first receives them.
Entity positions stream continuously. WebSockets are currently plain `ws://`;
the listener defaults to loopback. Rebuild nodes, workers, and modules together
when updating the internal spatial-interest format. The management protocol is
versioned separately.

See [the protocol notes](AtlasNet/docs/ControlPanel.md) for wire messages,
threading, snapshot barriers and batching details.
