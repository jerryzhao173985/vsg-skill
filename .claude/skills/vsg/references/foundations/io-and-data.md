---
title: Data, IO & serialization (and maths/precision)
description: How VSG represents CPU/GPU data (Data/Array/Value), loads and saves objects via vsg::read/write dispatching through ReaderWriters on Options, and why scene maths are double precision.
---

## What this covers
- The `vsg::Data` hierarchy — `Value<T>`, `Array<T>` (and `Array2D`/`Array3D`) as the single abstraction for both CPU values and GPU-bound buffers/images.
- File IO: `vsg::read` / `vsg::read_cast` / `vsg::write` and how they dispatch through `ReaderWriter`s held on an `Options` object, falling back to native VSG formats.
- Native serialization: `.vsgt` (ascii) / `.vsgb` (binary) driven by each object's `read(Input&)` / `write(Output&)` visitor methods, versus 3rd-party formats provided by vsgXchange.
- `Options` as the IO context: search `paths`, `fileCache`, `readerWriters`, `sharedObjects`, coordinate conventions.
- The maths layer: `vec`/`mat` value types, `float` vs `double` precision (`dvec3`/`dmat4`), and why scene transforms are double.

## Mental model
Everything that moves bytes in VSG funnels through one base class, `vsg::Data` (`include/vsg/core/Data.h:114`). `Data` is `Object` plus a `Properties` block (Vulkan `VkFormat`, stride, mip levels, block size, origin, `dataVariance`) (`include/vsg/core/Data.h:120-140`) and a pure-virtual interface for "where are the bytes, how many, how big" (`dataPointer()`, `valueCount()`, `valueSize()`, `dataSize()`) (`include/vsg/core/Data.h:173-191`). Concrete subclasses are `Value<T>` (a single `T`) and `Array<T>` (a contiguous, strided run of `T`) (`include/vsg/core/Data.h:112-113`). The same `Array<vec3>` you fill with vertices on the CPU is the object you hand to a `BufferInfo`/`Image` for upload — there is no separate "GPU buffer type" at this layer. That is the central idea: one representation for CPU-side authoring and GPU-side transfer, with `Properties.format`/`stride` describing how Vulkan should interpret it.

`Data` tracks modification with a `ModifiedCount`: `dirty()` increments it (`include/vsg/core/Data.h:206`) and `getModifiedCount()` lets the transfer machinery detect changes (`include/vsg/core/Data.h:209-218`). `dataVariance` (`STATIC_DATA`, `DYNAMIC_DATA`, …) tells the runtime whether the data is uploaded once or re-transferred each frame (`include/vsg/core/Data.h:59-65`); `dynamic()` is just `dataVariance >= DYNAMIC_DATA` (`include/vsg/core/Data.h:171`). So "modify a vertex array every frame" means: create the `Array` with `DYNAMIC_DATA`, mutate it, call `dirty()`.

IO is layered on top. `vsg::read(filename, options)` is a free function (`include/vsg/io/read.h:24`) that does extension-based dispatch: if `options` carries `readerWriters`, each is tried in turn; otherwise the extension selects a built-in `ReaderWriter` (`VSG` for `.vsgt`/`.vsgb`/`.vsga`, `spirv` for `.spv`, `glsl`, `txt`, `json`) (`src/vsg/io/read.cpp:31-74`). `read_cast<T>` is a thin `dynamic_cast` wrapper over `read` (`include/vsg/io/read.h:36-41`). `vsg::write` mirrors this: try `options->readerWriters` first, then fall back to native VSG/spirv/glsl by extension (`src/vsg/io/write.cpp:31-60`). The crucial consequence: **VSG core only knows native formats**; loading a `.gltf`/`.png`/`.obj` requires you to register vsgXchange's `ReaderWriter` on the `Options` (`options->add(vsgXchange::all::create())`) — otherwise `read` returns null for those extensions.

Native serialization (`.vsgt`/`.vsgb`) is a visitor protocol, not reflection. Every serializable type implements `read(Input&)` and `write(Output&) const`; the format chooses ascii vs binary by writing/reading a `#vsga`/`#vsgb` token header and instantiating `AsciiInput`/`BinaryInput` or `AsciiOutput`/`BinaryOutput` (`src/vsg/io/VSG.cpp:55-63`, `src/vsg/io/VSG.cpp:175-190`). `Input`/`Output` carry an `ObjectFactory` (to construct objects by `className`) and a version so old files keep loading (`include/vsg/io/Input.h:46`, `include/vsg/io/Input.h:251-254`). Object identity/sharing is preserved through an `ObjectIDMap` (`include/vsg/io/Input.h:243-246`).

The maths layer is plain value types (`t_vec3`, `t_mat4`, …) templated on the scalar, with named aliases for each precision: `vec3 = t_vec3<float>`, `dvec3 = t_vec3<double>` (`include/vsg/maths/vec3.h:157-158`), `mat4 = t_mat4<float>`, `dmat4 = t_mat4<double>` (`include/vsg/maths/mat4.h:122-123`). Scene transforms are double: `MatrixTransform::matrix` is a `dmat4` (`include/vsg/nodes/MatrixTransform.h:30`) so that large world coordinates (planet-scale, geospatial) don't lose precision before the GPU receives the final, relative-to-eye `float` matrices.

## Key types
- `vsg::Data` — abstract base for all CPU/GPU data; carries `Properties` (format/stride/variance) and the `dataPointer`/`valueCount` interface (`include/vsg/core/Data.h:114`).
- `vsg::Value<T>` — a single value of `T` (e.g. `floatValue`, `dvec3Value`, `mat4Value`); also backs `Object::setValue`/`getValue` metadata (`include/vsg/core/Value.h:39`, `include/vsg/core/Value.h:171-193`).
- `vsg::Array<T>` — strided contiguous run of `T` (e.g. `vec3Array`, `floatArray`, `ubyteArray`); used for vertices, indices, image data (`include/vsg/core/Array.h:35`).
- `vsg::Options` — the IO context: `paths`, `fileCache`, `readerWriters`, `sharedObjects`, coordinate conventions (`include/vsg/io/Options.h:36`).
- `vsg::ReaderWriter` — pluggable format handler; `read`/`write` by filename, stream, or memory; advertises `Features` (`include/vsg/io/ReaderWriter.h:34`).
- `vsg::Input` / `vsg::Output` — visitor objects passed to each object's `read`/`write` for native (de)serialization (`include/vsg/io/Input.h:43`, `include/vsg/io/Output.h`).
- `vsg::vec3` / `vsg::dvec3` / `vsg::mat4` / `vsg::dmat4` — float vs double maths value types (`include/vsg/maths/vec3.h:157`, `include/vsg/maths/mat4.h:122`).

## How it works (implementation-grounded)
1. **Construct data.** `Array<T>::create(...)` allocates through the VSG allocator (or `new[]`/`malloc` per `Properties.allocatorType`) and calls `dirty()` (`include/vsg/core/Array.h:60-63`, `include/vsg/core/Array.h:352-362`). `Array` stride defaults to `sizeof(value_type)` but can differ for interleaved data; iteration is via `stride_iterator` so a strided view still indexes correctly (`include/vsg/core/Array.h:39-40`, `include/vsg/core/Data.h:67-108`). An `Array` can also be a view into another `Data`'s storage via `assign(storage, offset, stride, …)` (`include/vsg/core/Array.h:265-286`).
2. **Read a file.** `vsg::read(filename, options)` checks `options->readerWriters` first (each tried until one returns non-null), else dispatches on the lower-cased extension to a built-in `ReaderWriter` (`src/vsg/io/read.cpp:31-74`). If `options->sharedObjects` is set and the filename is suitable, the load is de-duplicated and memoized so repeated reads of the same asset return the shared instance (`src/vsg/io/read.cpp:76-115`). `read(Paths&, …)` parallelizes across `options->operationThreads` when more than one file is requested (`src/vsg/io/read.cpp:131-173`).
3. **Native decode.** For `.vsgt`/`.vsgb`, `VSG::read` opens the stream, reads the `#vsga`/`#vsgb` token, and builds an `AsciiInput` or `BinaryInput` bound to the `ObjectFactory` (`src/vsg/io/VSG.cpp:94-118`). Each object's `read(Input&)` pulls named properties: e.g. `Array<T>::read` reads `"size"`, optional `"storage"`/`"offset"`, then the raw `"data"` block (`include/vsg/core/Array.h:154-194`); `Value<T>::read` reads a single `"value"` (`include/vsg/core/Value.h:84-92`). `Input::read(propertyName, args...)` matches the property name then dispatches by type, with `vec`/`mat` overloads flattening to scalar runs (`include/vsg/io/Input.h:78-116`, `include/vsg/io/Input.h:189-196`).
4. **Write a file.** `vsg::write(object, filename, options)` tries `options->readerWriters`, then native VSG by extension (`src/vsg/io/write.cpp:21-60`). `VSG::write` picks ascii vs binary from the extension/`extensionHint`, writes the header token, and walks the object graph calling each `write(Output&) const` (`src/vsg/io/VSG.cpp:159-190`, `src/vsg/io/VSG.cpp:201-234`). `Array<T>::write` emits `"size"` then the `"data"` block, or the storage reference if it is a view (`include/vsg/core/Array.h:196-212`).
5. **Versioning.** `Input`/`Output` expose `version_greater_equal(major,minor,patch)`; serialization code branches on it (e.g. `Value` writes `"value"` for ≥0.6.1 else legacy `"Value"`) so newer code reads older files (`include/vsg/core/Value.h:87-100`, `include/vsg/io/Input.h:253-254`).
6. **Maths precision.** `Options` defaults `sceneCoordinateConvention` to `Z_UP` (`include/vsg/io/Options.h:80`) and maps known formats (`.gltf`/`.glb` → `Y_UP`) so loaders can re-orient (`src/vsg/io/Options.cpp:29-33`). Because `MatrixTransform::matrix` is `dmat4` (`include/vsg/nodes/MatrixTransform.h:30`), accumulated world transforms stay double until the record traversal reduces them relative to the camera.

## Rules that follow
- To load any non-native format (glTF, OBJ, PNG, KTX, …) you MUST register a `ReaderWriter` on `Options` — typically `options->add(vsgXchange::all::create())` — before calling `vsg::read` (`src/vsg/io/read.cpp:31-74`; example `examples/app/vsgviewer/vsgviewer.cpp:68`).
- Use `vsg::read_cast<T>(filename, options)` when you know the expected type; it is `read` + `dynamic_cast`, so a wrong/failed type yields a null `ref_ptr` you must check (`include/vsg/io/read.h:36-41`).
- Set `options->paths` (often from `vsg::getEnvPaths("VSG_FILE_PATH")`) so relative asset filenames resolve; the reader uses these search paths (`include/vsg/io/Options.h:69`; example `examples/app/vsgviewer/vsgviewer.cpp:64`).
- After mutating a `Data`'s contents in place, call `dirty()` so the transfer/`ModifiedCount` machinery re-uploads it; mutation alone does not flag a change (`include/vsg/core/Data.h:206-218`).
- If data must change after upload, set `Properties.dataVariance` to `DYNAMIC_DATA` (or the post-record variant); leaving it `STATIC_DATA` means VSG may unref/skip re-transfer (`include/vsg/core/Data.h:59-65`).
- Use double-precision types (`dvec3`, `dmat4`) for scene-graph positions/transforms and reserve `float` (`vec3`, `mat4`) for GPU-facing vertex/uniform data (`include/vsg/nodes/MatrixTransform.h:30`, `include/vsg/maths/mat4.h:122-123`).
- Share an `Options` instance with `sharedObjects` set when loading the same asset repeatedly to get de-duplication for free (`src/vsg/io/read.cpp:76-115`).

## Common mistakes
- "`vsg::read` can load any 3D format out of the box" — wrong; core only handles `.vsgt`/`.vsgb`/`.spv`/`.glsl`/`.txt`/`.json`. Register vsgXchange for everything else (`src/vsg/io/read.cpp:42-73`).
- "Edit an `Array`'s elements and the GPU updates" — no; you must `dirty()` it and it must be `DYNAMIC_DATA` to be re-transferred (`include/vsg/core/Data.h:206`, `include/vsg/core/Data.h:59-65`).
- "Store world positions as `vec3`/`mat4`" — float loses precision at large coordinates; scene transforms are `dmat4` by design (`include/vsg/nodes/MatrixTransform.h:30`).
- "`read` returning null means the file is corrupt" — it can also mean no `ReaderWriter` matched the extension; a genuine read failure returns a `ReadError` object, not null (`include/vsg/io/ReaderWriter.h:24-31`, `src/vsg/io/read.cpp:69-73`).
- "Use a separate buffer class for GPU data" — VSG uses the same `Array<T>`/`Value<T>` for CPU authoring and GPU upload; `Properties.format`/`stride` describe the Vulkan layout (`include/vsg/core/Data.h:120-140`).
- "Pass a fresh `Options` each call" — you then lose `paths`, registered readers, and `sharedObjects` caching; build one `Options` and reuse it (`include/vsg/io/Options.h:36`, `src/vsg/io/Options.cpp:39-62`).

## Source references
- `include/vsg/core/Data.h` — `Data` base, `Properties`, `DataVariance`, `ModifiedCount`, `stride_iterator`
- `include/vsg/core/Array.h` — `Array<T>`, allocation, strided storage, native `read`/`write`, type aliases
- `include/vsg/core/Value.h` — `Value<T>`, `Object::setValue`/`getValue`, `vsg::value<>` helper, type aliases
- `include/vsg/io/read.h` — `vsg::read` / `vsg::read_cast` declarations
- `include/vsg/io/write.h` — `vsg::write` declaration
- `include/vsg/io/Options.h` — `Options` (paths, fileCache, readerWriters, sharedObjects, coordinate conventions)
- `include/vsg/io/ReaderWriter.h` — `ReaderWriter`, `ReadError`, `Features`, `CompositeReaderWriter`
- `include/vsg/io/Input.h` / `include/vsg/io/Output.h` — visitor (de)serialization interface and versioning
- `include/vsg/maths/vec3.h` / `include/vsg/maths/mat4.h` — `vec`/`mat` value types and float/double aliases
- `include/vsg/nodes/MatrixTransform.h` — `dmat4 matrix` (scene transforms are double)
- `src/vsg/io/read.cpp` — `vsg::read` dispatch, sharedObjects de-dup, threaded multi-file read
- `src/vsg/io/write.cpp` — `vsg::write` dispatch and native fallback
- `src/vsg/io/VSG.cpp` — `.vsgt`/`.vsgb` header tokens, ascii/binary Input/Output selection
- `src/vsg/io/Options.cpp` — Options construction, copy, coordinate-convention defaults, command-line opts
- `examples/io/vsgio/vsgio.cpp` — building an object graph, `setValue`/`setObject`, `vsg::write`, `VSG::read`
- `examples/app/vsgviewer/vsgviewer.cpp` — `options->paths` from env, `options->add(vsgXchange::all::create())`, `read_cast`
