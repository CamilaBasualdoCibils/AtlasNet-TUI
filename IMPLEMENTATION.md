# Implementation and validation report

The workspace now builds an ControlPanel node capability, a standalone C++ client SDK, and the FTXUI terminal map. The visibility controls sit
above a map that spans the terminal width and wrap on narrower terminals.

## AtlasNet submodule

The implementation reuses Controller intent RPC, `RPCMethod`/`HashRPC`,
`DescribeRegion` and partition ownership, the Controller's authoritative domain,
`NodeDirectory`, ShardManager spatial/entity queries, published transforms, and
`Interest::Manager` migration epochs/probes/snapshot framing. The gateway performs
peer discovery before entering the channel send lock and caches registry addresses.
Spatial interests now optionally track coalesced position changes and wait for
old as well as new destinations when reconciling changed bounds.

The server uses uWebSockets; the independently linkable client uses asynchronous
Boost.Beast. The SDK exposes the `AtlasNet::ControlPanelClient` target directly in the existing CMakeLists.txt. Server sources compile in Node, which links uWebSockets directly. No Node implementation
is linked into the TUI or SDK. See [protocol documentation](AtlasNet/docs/ControlPanel.md).

The existing stack extension now offers ControlPanel as an independent checkbox,
a listener port base, generated per-node ports, YAML overrides and TCP collision
validation. Its package version is 0.3.5.

Changed and added files (paths relative to `AtlasNet/`):

- [CMakeLists.txt](AtlasNet/CMakeLists.txt)
- Updated the existing CMakeLists.txt and cmake/FetchDeps.cmake directly.
- [docs/ControlPanel.md](AtlasNet/docs/ControlPanel.md)
- [include/AtlasNet/ControlPanel/ControlPanelClient.hpp](AtlasNet/include/AtlasNet/ControlPanel/ControlPanelClient.hpp)
- [include/AtlasNet/ControlPanel/ControlPanelServer.hpp](AtlasNet/include/AtlasNet/ControlPanel/ControlPanelServer.hpp)
- [include/AtlasNet/ControlPanel/Geometry.hpp](AtlasNet/include/AtlasNet/ControlPanel/Geometry.hpp)
- [include/AtlasNet/ControlPanel/Protocol.hpp](AtlasNet/include/AtlasNet/ControlPanel/Protocol.hpp)
- [include/AtlasNet/Core/SpatialInterest/InterestManager.hpp](AtlasNet/include/AtlasNet/Core/SpatialInterest/InterestManager.hpp)
- [include/AtlasNet/Core/SpatialInterest/SpatialInterest.hpp](AtlasNet/include/AtlasNet/Core/SpatialInterest/SpatialInterest.hpp)
- [include/AtlasNet/Node/AtlasNetNode.hpp](AtlasNet/include/AtlasNet/Node/AtlasNetNode.hpp)
- [include/AtlasNet/Node/ControlPanelGateway.hpp](AtlasNet/include/AtlasNet/Node/ControlPanelGateway.hpp)
- [include/AtlasNet/Node/DB/DebugMirror.hpp](AtlasNet/include/AtlasNet/Node/DB/DebugMirror.hpp)
- [include/AtlasNet/Node/NodeCapability.hpp](AtlasNet/include/AtlasNet/Node/NodeCapability.hpp)
- [include/AtlasNet/Node/NodeConfiguration.hpp](AtlasNet/include/AtlasNet/Node/NodeConfiguration.hpp)
- [include/AtlasNet/Node/RPC/ControlPanel.hpp](AtlasNet/include/AtlasNet/Node/RPC/ControlPanel.hpp)
- [include/AtlasNet/Node/Shard/ShardManager.hpp](AtlasNet/include/AtlasNet/Node/Shard/ShardManager.hpp)
- [src/ControlPanel/Client/ControlPanelClient.cpp](AtlasNet/src/ControlPanel/Client/ControlPanelClient.cpp)
- [src/Node/ControlPanelServer.cpp](AtlasNet/src/Node/ControlPanelServer.cpp)
- [src/Core/Configuration/Configuration.cpp](AtlasNet/src/Core/Configuration/Configuration.cpp)
- [src/Core/SpatialInterest/InterestManager.cpp](AtlasNet/src/Core/SpatialInterest/InterestManager.cpp)
- [src/Node/AtlasNetNode.cpp](AtlasNet/src/Node/AtlasNetNode.cpp)
- [src/Node/ControlPanelGateway.cpp](AtlasNet/src/Node/ControlPanelGateway.cpp)
- [src/Node/Controller/Controller.cpp](AtlasNet/src/Node/Controller/Controller.cpp)
- [src/Node/Shard/ShardManager.cpp](AtlasNet/src/Node/Shard/ShardManager.cpp)
- [src/Standalone/Configuration.cpp](AtlasNet/src/Standalone/Configuration.cpp)
- [tests/ControlPanel/CMakeLists.txt](AtlasNet/tests/ControlPanel/CMakeLists.txt)
- [tests/ControlPanel/Cluster.py](AtlasNet/tests/ControlPanel/Cluster.py)
- [tests/ControlPanel/Geometry.cpp](AtlasNet/tests/ControlPanel/Geometry.cpp)
- [tests/ControlPanel/TestWorld.cpp](AtlasNet/tests/ControlPanel/TestWorld.cpp)
- [tests/ControlPanel/WebSocket.cpp](AtlasNet/tests/ControlPanel/WebSocket.cpp)
- [tests/Unit/DistributedSpatial/Interest.cpp](AtlasNet/tests/Unit/DistributedSpatial/Interest.cpp)
- [tests/Unit/Node/CMakeLists.txt](AtlasNet/tests/Unit/Node/CMakeLists.txt)
- [tools/vscode-local-stack/README.md](AtlasNet/tools/vscode-local-stack/README.md)
- [tools/vscode-local-stack/media/panel.html](AtlasNet/tools/vscode-local-stack/media/panel.html)
- [tools/vscode-local-stack/media/panel.js](AtlasNet/tools/vscode-local-stack/media/panel.js)
- [tools/vscode-local-stack/package.json](AtlasNet/tools/vscode-local-stack/package.json)
- [tools/vscode-local-stack/stack.js](AtlasNet/tools/vscode-local-stack/stack.js)
- [tools/vscode-local-stack/test/stack.test.js](AtlasNet/tools/vscode-local-stack/test/stack.test.js)
- [vcpkg.json](AtlasNet/vcpkg.json)

## Superproject

The TUI has a Map tab, functional layer checkboxes, exact clipped convex boundaries,
entity density markers, controller bounds, immediate drag/cursor-centered wheel
camera movement, viewport-margin subscriptions, revision/sequence reconciliation,
connection status and reconnect handling. All cluster data comes through the SDK.

Changed and added files:

- [.gitignore](.gitignore)
- [CMakeLists.txt](CMakeLists.txt)
- [README.md](README.md)
- [IMPLEMENTATION.md](IMPLEMENTATION.md)
- [src/MapModel.hpp](src/MapModel.hpp)
- [src/MapModel.cpp](src/MapModel.cpp)
- [src/main.cpp](src/main.cpp)
- [tests/MapTests.cpp](tests/MapTests.cpp)
- [vcpkg.json](vcpkg.json)

The user initialized the submodule and checked out the intended AtlasNet revision
`8a2244151d0643cedcb37820964d14aba4f22b09` before implementation. Git commits have
not been created; changes remain in the two working trees.

## Verification

| Check | Result |
| --- | --- |
| AtlasNet Node and worker, default Control Panel client and Node server sources | Built successfully |
| TUI and separate client SDK | Built successfully |
| Existing spatial suite plus new position/migration/barrier tests | 69 passed |
| Control Panel CTest selection | 7 passed, including real cluster |
| TUI CTest selection | Map tests passed; server transport tests remain in AtlasNet |
| Stack extension tests | 19 passed; 2 optional GDB/perf tests skipped |
| Real terminal application against real cluster | Passed |
| Git whitespace checks in both repositories | Passed |

The cluster test ran isolated Valkey storage, two shard nodes and a third node
with only ControlPanel enabled. It retrieved real unbounded heuristic regions and
authoritative simulation bounds, tracked four moving entities over two shard
nodes, checked stable IDs and ordered incremental updates, moved the subscription
outside and back into the simulation without changing its ID, then checked
unsubscribe and malformed JSON. The optional PTY run connected the actual TUI,
verified real entity data and the full-width layout, sent wheel and drag events,
disabled/restored the entity layer, and exited cleanly. Every test process was
stopped afterward.

Build/test logs are available in the local `build/` directory, including
`control-panel-test.log`, `tui-test.log`, `spatial-test.log`, `stack-test.log`,
`cluster-tui-test.log`, and `node-off-build.log`. Dependency sources used for local
validation are also confined to `build/` or existing read-only dependency caches;
they are not vendored into either source tree.

## Initial limits

- AtlasNet reserves WorldID in its spatial queries. The gateway accepts world 0
  and explicitly rejects other values.
- The TUI renders Cartesian data and XY/XZ/YZ 2D partitions. For XYZ partitions,
  it draws a z=0 slice and projects entity positions onto XY; the hidden entity
  axis has a finite ±1e30 subscription slab.
- Metadata loads on connection; reconnect with `r` to refresh heuristic geometry
  and simulation bounds. Entity positions stream continuously.
- The initial transport is plain `ws://`, with a loopback listener default and
  no authentication. All nodes/workers/modules must be rebuilt together because
  the opt-in position flag extends the existing internal binary interest contract.

[Build and run instructions](README.md).

## Saved SimpleGame stack test

The saved `/home/camila/Documents/AtlasNet/.vscode/atlasnet-stack.json` was used
without modifying its settings: two nodes, KMeans2D, four desired shards, 15-second
recomputation, SteamNetSock ports 5919/5920 and Control Panel port 8080.
Three windowed `SimpleGameClient` processes and the real `SimpleGameClientStress`
program ran together. All 100 stress clients connected and received confirmations
(50 per ingress), and all three regular clients sent movement commands and
received remote player updates. The Control Panel stream contained 103 entities
and the TUI drew orange heuristic boundaries and blue entity locations. Layer
visibility, pan, cursor zoom, and refreshing metadata after recomputation passed.
Every test process was stopped afterward.

The final build defines the client library directly in the existing CMakeLists.txt.
Server sources compile under `src/Node`, with uWebSockets linked directly to Node.
There is no server library target, no Control Panel build switch, and no added
CMake helper files. An existing Cartograph build target was disabled in the local
build cache because that checkout's legacy Cartograph sources had been removed.

Final evidence: [results](build/simplegame-stack-20261117T222204Z/result.json),
[build structure](build/simplegame-stack-20261117T222204Z/build-structure.json),
[terminal capture](build/simplegame-stack-20261117T222204Z/tui-live.png).
