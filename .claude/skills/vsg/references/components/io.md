---
title: vsg::read / vsg::read_cast / vsg::write / vsg::Options
description: Free functions that load and save VSG objects to/from file, stream or memory, with an Options object carrying search paths, ReaderWriters and a file cache.
---

## Public include
Consumers normally pull everything via the barrel header; the specific declaring headers are listed for reference.
```cpp
#include <vsg/all.h>        // barrel include (preferred)
// specific headers:
#include <vsg/io/read.h>    // vsg::read / vsg::read_cast
#include <vsg/io/write.h>   // vsg::write
#include <vsg/io/Options.h> // vsg::Options
```
All symbols live in namespace `vsg::`. The `read` declarations are in `include/vsg/io/read.h:20`, `write` in `include/vsg/io/write.h:19`, and `Options` in `include/vsg/io/Options.h:36`.

## When to use
Use `vsg::read` / `vsg::read_cast<T>` to load a scene graph node, image `Data`, shader, font or any serialized `vsg::Object` from a `.vsgt`/`.vsgb` file or, via a `ReaderWriter` such as `vsgXchange::all`, from 3rd-party formats; use `vsg::write` to serialize an object back out. These are the high-level convenience entry points layered over the `ReaderWriter` plugins listed in `Options::readerWriters` (`include/vsg/io/Options.h:57`) — when you need direct control over a single native format you can instead use the `vsg::VSG` ReaderWriter object (`examples/io/vsgio/vsgio.cpp:92`). `Options` (`include/vsg/io/Options.h:36`) is the shared bag of IO settings (paths, file cache, ReaderWriters) you pass to both `read` and `write`; it is not a per-file object — create one and reuse it.

## Key API
- `vsg::read(const Path& filename, ref_ptr<const Options> options = {})` -> `ref_ptr<Object>`; returns null on failure — `include/vsg/io/read.h:24`.
- `vsg::read(const Paths& filenames, ...)` -> `PathObjects` for batch reads — `include/vsg/io/read.h:27`.
- `vsg::read(std::istream& fin, ...)` and `vsg::read(const uint8_t* ptr, size_t size, ...)` for stream/memory sources — `include/vsg/io/read.h:30`, `include/vsg/io/read.h:33`.
- `vsg::read_cast<T>(const Path& filename, ref_ptr<const Options> options = {})` -> `ref_ptr<T>`; calls `read` then `dynamic_cast<T*>`, returning null if the read failed OR the type did not match — `include/vsg/io/read.h:37`.
- `vsg::write(ref_ptr<Object> object, const Path& filename, ref_ptr<const Options> options = {})` -> `bool` (true on success) — `include/vsg/io/write.h:23`.
- `vsg::Options::create(args...)` — variadic factory; each arg is forwarded to `add(...)`, so `Options::create(vsgXchange::all::create())` registers a ReaderWriter at construction — `include/vsg/io/Options.h:42`.
- `Options::add(ref_ptr<ReaderWriter>)` / `Options::add(const ReaderWriters&)` — append ReaderWriter plugins — `include/vsg/io/Options.h:53`.
- `Options::paths` (`Paths`) — directories searched for relative filenames — `include/vsg/io/Options.h:69`.
- `Options::fileCache` (`Path`) — directory used to cache converted/compiled assets — `include/vsg/io/Options.h:74`.
- `Options::readerWriters` (`ReaderWriters`) — the ordered list of plugins consulted — `include/vsg/io/Options.h:57`.
- `Options::sharedObjects` (`ref_ptr<SharedObjects>`) — enables de-duplication/sharing of loaded objects — `include/vsg/io/Options.h:56`.
- `Options::readOptions(CommandLine&)` -> `bool` — pull IO options from the command line — `include/vsg/io/Options.h:51`.
- `Options::checkFilenameHint` defaults to `CHECK_ORIGINAL_FILENAME_EXISTS_FIRST` — controls how `paths` interacts with the original filename — `include/vsg/io/Options.h:67`.
- `Options::extensionHint` (`Path`) — force a format when the filename has no usable extension — `include/vsg/io/Options.h:76`.

## Best Practices
- Always allocate via `vsg::Options::create(...)`, never `new`; it derives from `vsg::Object` through `Inherit<Object, Options>` and is held by `ref_ptr` — `include/vsg/io/Options.h:36`.
- ALWAYS null-check the result of `read`/`read_cast`. `read_cast<T>` returns null both when the file could not be read and when the loaded object is not a `T`, so a non-null check is your only signal of success — `include/vsg/io/read.h:40`, `examples/app/vsghelloworld/vsghelloworld.cpp:25`.
- Register 3rd-party format support by adding a `ReaderWriter` (commonly `vsgXchange::all::create()`) to the `Options`; without it only the native `.vsgt`/`.vsgb` formats are available — `examples/app/vsghelloworld/vsghelloworld.cpp:14`, `examples/app/vsgviewer/vsgviewer.cpp:68`.
- Build one `Options` near startup and pass it to every `read`/`write`; set `paths` from the environment with `vsg::getEnvPaths("VSG_FILE_PATH")` and `fileCache` with `vsg::getEnv("VSG_FILE_CACHE")` — `examples/app/vsghelloworld/vsghelloworld.cpp:15`, `examples/app/vsghelloworld/vsghelloworld.cpp:16`.
- The `options` parameter is `ref_ptr<const Options>` — `read`/`write` will not mutate it, so the same object is safe to share across calls and threads for reading — `include/vsg/io/read.h:24`.
- `read`/`write` only deserialize/serialize CPU-side objects; they do NOT create Vulkan resources. A loaded scene graph must still go through `viewer->compile()` before the frame loop renders it — `examples/app/vsghelloworld/vsghelloworld.cpp:74`.
- Native format choice is by extension: `.vsgt` is human-readable ASCII, `.vsgb` is binary; both are handled by the built-in `vsg::VSG` ReaderWriter with no extra plugin — `examples/io/vsgio/vsgio.cpp:92`, `examples/app/vsghelloworld/vsghelloworld.cpp:19`.
- Set `extensionHint` when reading from a stream or memory buffer that has no filename, so the right `ReaderWriter` is selected — `include/vsg/io/Options.h:76`.
- Set `options->sharedObjects = vsg::SharedObjects::create()` to share identical loaded sub-objects (textures, state) across multiple reads and reduce memory — `examples/app/vsgviewer/vsgviewer.cpp:62`.

## Composition examples
Distilled from `examples/app/vsghelloworld/vsghelloworld.cpp:13-26`:
```cpp
#include <vsg/all.h>
#include <vsgXchange/all.h>   // 3rd-party format ReaderWriters

// Build a reusable Options object: register vsgXchange, set cache + search paths.
auto options = vsg::Options::create(vsgXchange::all::create());
options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

// Typed read: returns null if the file is missing OR is not a vsg::Node.
vsg::ref_ptr<vsg::Node> scene = vsg::read_cast<vsg::Node>("models/teapot.vsgt", options);
if (!scene) return 0;        // MUST check before use
// ... add scene to a CommandGraph, then viewer->compile() before rendering.
```

Writing an object back to a native or 3rd-party format, distilled from `examples/io/vsgio/vsgio.cpp:107-111`:
```cpp
#include <vsg/all.h>

vsg::ref_ptr<vsg::Object> object = /* ... build or load ... */;

// Extension decides the format: .vsgt (ASCII), .vsgb (binary), or a
// vsgXchange-supported format if its ReaderWriter is in options.
if (!vsg::write(object, "output.vsgt"))     // returns false on failure
{
    // handle write error
}
```

## Source references
- `include/vsg/io/read.h` — `read` overloads and the `read_cast<T>` template declarations.
- `include/vsg/io/write.h` — the `write` declaration.
- `include/vsg/io/Options.h` — the `Options` class, its factory, members and `FindFileHint`/`InstanceNodeHint` enums.
- `examples/app/vsghelloworld/vsghelloworld.cpp` — canonical `Options::create` + `read_cast<vsg::Node>` + null-check usage.
- `examples/io/vsgio/vsgio.cpp` — `vsg::write(object, filename)` and direct `vsg::VSG` ReaderWriter usage.
- `examples/app/vsgviewer/vsgviewer.cpp` — `Options` with `sharedObjects`, `paths`, `fileCache`, and `add(vsgXchange::all::create())`.

## Common mistakes
- Using the raw `vsg::read` result without a cast when you need a node -> use `vsg::read_cast<vsg::Node>(...)` so you get a typed `ref_ptr<vsg::Node>`; `include/vsg/io/read.h:37`.
- Skipping the null-check after `read_cast` -> always test the returned `ref_ptr` before dereferencing; it is null on any failure; `examples/app/vsghelloworld/vsghelloworld.cpp:26`.
- Expecting `.obj`/`.gltf`/`.png` to load out of the box -> add a `ReaderWriter` (e.g. `vsgXchange::all::create()`) to the `Options` first; only `.vsgt`/`.vsgb` are built-in; `examples/app/vsgviewer/vsgviewer.cpp:68`.
- Allocating `Options` with `new` -> use `vsg::Options::create(...)`; the object is ref-counted; `include/vsg/io/Options.h:42`.
- Assuming a successful `read` gives a render-ready scene -> you still must `viewer->compile()` before the frame loop; `read` only builds CPU-side objects; `examples/app/vsghelloworld/vsghelloworld.cpp:74`.
- Ignoring the `bool` returned by `write` -> check it; a false return means the file was not written; `include/vsg/io/write.h:23`.

## Things to never invent
- There is no `vsg::load`, `vsg::open`, `vsg::Options::load` or `Options::loadFile`; the only entry points are the free functions `vsg::read`, `vsg::read_cast`, `vsg::write`.
- `read`/`read_cast` do NOT throw or return an error code on failure — they return a null `ref_ptr`. Do not write `try/catch` expecting an IO exception.
- `Options` has no `setPath`/`addPath`/`getPaths` accessors; `paths` is a public `Paths` member you assign directly (`include/vsg/io/Options.h:69`), and ReaderWriters are added via `add(...)` not `addReaderWriter`.
- `Options::add` takes a `ref_ptr<ReaderWriter>` or `ReaderWriters`, not a file path or a string (`include/vsg/io/Options.h:53`).
- There is no `vsg::write(object, std::ostream&)` free function in `write.h`; stream writing goes through the `vsg::VSG` ReaderWriter object's `write` method instead (`examples/io/vsgio/vsgio.cpp:117`).
- `read_cast` is a function template, not a member of `Options`; never write `options->read_cast<...>`.
