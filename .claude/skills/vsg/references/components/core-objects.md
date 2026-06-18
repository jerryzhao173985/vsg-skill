---
title: ref_ptr / Object / Inherit / observer_ptr (core object idioms)
description: The intrusive reference-counting object model that every VSG type uses — heap allocation via T::create(), strong ref_ptr<T> ownership, weak observer_ptr<T>, and new types declared via Inherit<Base,Derived>.
---

## Public include
Consumers normally pull everything through the barrel header and use the `vsg::` namespace:
```cpp
#include <vsg/all.h>   // pulls in all core headers below
```
The specific declaring headers (useful for reference) are `include/vsg/core/ref_ptr.h` (`ref_ptr.h:21`), `include/vsg/core/Object.h` (`Object.h:59`), `include/vsg/core/Inherit.h` (`Inherit.h:27`), `include/vsg/core/observer_ptr.h` (`observer_ptr.h:23`), and `include/vsg/core/Auxiliary.h` (`Auxiliary.h:25`). All symbols live in namespace `vsg` (`ref_ptr.h:15`).

## When to use
This is the foundational idiom every other VSG type assumes. Use `vsg::ref_ptr<T>` for strong (owning) handles to any `vsg::Object`-derived type, `vsg::observer_ptr<T>` for weak (non-owning) back-references that must not keep the object alive (`observer_ptr.h:20`), and a raw `T*` only for transient, non-owning access where the object is known to outlive the pointer (`ref_ptr.h:164`). Derive new scene-graph/object types with `vsg::Inherit<ParentClass, Subclass>` (`Inherit.h:27`), never directly from the base class and never with manual `new`/`delete`.

## Key API
- `T::create(args...)` -> `ref_ptr<T>` — the only sanctioned constructor; forwards args to the subclass ctor (`Inherit.h:35`). Base `vsg::Object::create()` likewise returns `ref_ptr<Object>` (`Object.h:67`).
- `T::create_if(bool, args...)` -> `ref_ptr<T>` — creates only when the flag is true, otherwise returns an empty `ref_ptr` (`Inherit.h:41`; base form `Object.h:69`).
- `ref_ptr<T>::get()` -> `T*` — raw pointer without transferring ownership (`ref_ptr.h:173`).
- `ref_ptr<T>::operator*` / `operator->` — deref/member access (`ref_ptr.h:169`, `ref_ptr.h:171`).
- `ref_ptr<T>::valid()` / `explicit operator bool()` — null test (`ref_ptr.h:160`, `ref_ptr.h:162`).
- `ref_ptr<T>::reset()` — drop the reference, set to null (`ref_ptr.h:72`).
- `ref_ptr<T>::cast<R>()` -> `ref_ptr<R>` — safe down/cross cast via the object's RTTI (`ref_ptr.h:192`).
- `vsg::cast<T>(object)` -> `T*` — free-function cast taking a `ref_ptr` or raw pointer (`Object.h:195`, `Object.h:201`).
- `Object::ref()` / `Object::unref()` — intrusive increment/decrement; `unref()` deletes when the count hits zero (`Object.h:122`, `Object.h:123`). You almost never call these directly — `ref_ptr` does.
- `Object::referenceCount()` -> `unsigned int` (`Object.h:128`).
- `Object::sizeofObject()`, `className()`, `type_info()`, `is_compatible()` — RTTI surface, overridden automatically by `Inherit` (`Object.h:81`-`Object.h:86`; overrides `Inherit.h:47`-`Inherit.h:50`).
- `Object::accept(Visitor&)`, `accept(ConstVisitor&)`, `accept(RecordTraversal&)` — visitor dispatch, also auto-wired by `Inherit` (`Object.h:109`-`Object.h:115`; `Inherit.h:70`-`Inherit.h:72`).
- `Object::setValue(key, value)` / `getValue(key, value)` — typed metadata stored via `vsg::Value<T>` (`Object.h:132`, `Object.h:139`).
- `Object::setObject(key, ref_ptr<Object>)` / `getObject(key)` / `getRefObject(key)` — attach/retrieve named child objects (`Object.h:143`, `Object.h:146`, `Object.h:160`); templated typed variants exist (`Object.h:152`, `Object.h:166`).
- `Object::getOrCreateAuxiliary()` / `getAuxiliary()` — lazily-allocated side table that backs metadata and weak pointers (`Object.h:177`, `Object.h:178`).
- `observer_ptr<T>::ref_ptr()` and `operator vsg::ref_ptr<R>()` — promote a weak handle to a strong one (locked, returns empty if the object is gone) (`observer_ptr.h:173`, `observer_ptr.h:177`).
- `observer_ptr<T>::valid()` — true only while the connected object still exists (`observer_ptr.h:168`).

## Best Practices

### Lifecycle & ownership
- Always allocate with `T::create(...)`, never `new T` — `create` returns a `ref_ptr` that already holds the first reference (`Inherit.h:35`). Storing the result in a `ref_ptr` (or `auto`) keeps it alive.
- Never call `delete` on a VSG object and never manage its lifetime manually — cleanup is automatic: the last `ref_ptr` to drop triggers `unref()` -> deletion (`Object.h:123`, `ref_ptr.h:69`). The destructor `~Object()` is `protected`, so stack allocation / manual delete won't compile (`Object.h:182`).
- Hold owning references in `ref_ptr<T>` members. A bare `T*` member does not keep the object alive and can dangle.
- `ref_ptr` is intentionally small (a single pointer) and faster than `std::shared_ptr` because the count lives inside the object (`ref_ptr.h:18`); do not wrap VSG objects in `std::shared_ptr`.

### ref_ptr vs observer_ptr vs raw pointer
- Use `observer_ptr<T>` to break ownership cycles or to reference an object you must not keep alive (e.g. a child referring back to its `Viewer`/`Window`) (`observer_ptr.h:20`).
- Before dereferencing an `observer_ptr`, promote it to a `ref_ptr` first; this is the thread-safe way to access the pointee (`observer_ptr.h:177`). Real example: `vsg::ref_ptr<vsg::Window> window = closeWindow.window;` (`examples/ui/vsginput/vsginput.cpp:263`).
- The implicit `ref_ptr<T>::operator T*()` is convenient but flagged dangerous in the header: assigning it to a raw pointer can dangle if the `ref_ptr` then dies (`ref_ptr.h:164`). Prefer `.get()` when you really want the raw pointer.

### Defining new types
- Derive via the CRTP helper: `class MyNode : public vsg::Inherit<vsg::Group, MyNode> {...}`. `Inherit` supplies `create()`, `create_if()`, ref-counting, `sizeofObject()`, `className()`, `type_info()`, `is_compatible()`, and `accept()` for free (`Inherit.h:27`-`Inherit.h:72`). Real examples: `vsg::Inherit<vsg::Object, Params>` (`examples/ui/vsgimgui_example/vsgimgui_example.cpp:15`), `vsg::Inherit<vsg::Group, CustomGroupNode>` (`examples/core/vsgvisitorcustomtype/VisitorCustomType.h:34`).
- The first template arg is the parent, the second is the new class itself (CRTP). Constructor args are forwarded through `Inherit`'s perfect-forwarding ctor (`Inherit.h:30`).

### Threading & casting
- Casting uses VSG's own RTTI (`is_compatible`/`type_info`), not necessarily `dynamic_cast` — `cast<T>()` returns `nullptr` on mismatch, so check the result (`Object.h:96`, `ref_ptr.h:192`).
- Reference counting is atomic (`std::atomic_uint`), so `ref`/`unref` across threads is safe, but accessing object *contents* across threads still needs your own synchronization (`Object.h:122`, `Object.h:190`). Promote `observer_ptr` -> `ref_ptr` to safely read across threads (`observer_ptr.h:181`).

## Composition examples

```cpp
#include <vsg/all.h>

// 1. Create with the factory; the ref_ptr owns the object.
auto group = vsg::Group::create();          // ref_ptr<vsg::Group> (Inherit.h:35)

// 2. Attach typed metadata — stored in the lazily-created Auxiliary.
auto object = vsg::Object::create();         // Object.h:67
object->setValue("name", "Name field contents"); // Object.h:132
object->setValue("size", 3.1f);
double t = 0.0;
object->getValue("time", t);                 // Object.h:139

// 3. Weak back-reference, promoted to strong before use (threading-safe).
vsg::observer_ptr<vsg::Viewer> weakViewer(viewer);   // observer_ptr.h:52
if (vsg::ref_ptr<vsg::Viewer> v = weakViewer)        // promote; empty if gone (observer_ptr.h:177)
{
    v->update();                              // safe: object held alive by v
}
// (distilled from examples/threading/vsgdynamicload/vsgdynamicload.cpp:279
//  and examples/ui/vsginput/vsginput.cpp:263, examples/core/vsgvalues/vsgvalues.cpp:28-33)
```

```cpp
#include <vsg/all.h>

// Defining a new scene-graph node type via the Inherit CRTP helper.
// Inherit supplies create(), ref-counting, RTTI and accept() automatically.
class CustomGroupNode : public vsg::Inherit<vsg::Group, CustomGroupNode>
{
public:
    CustomGroupNode() {}
    // ... custom members ...
};
// Usage: heap-allocated via the generated factory, owned by a ref_ptr.
auto node = CustomGroupNode::create();   // ref_ptr<CustomGroupNode>
// (faithful to examples/core/vsgvisitorcustomtype/VisitorCustomType.h:34)
```

## Source references
- `include/vsg/core/ref_ptr.h` — `ref_ptr<T>` strong smart pointer declaration.
- `include/vsg/core/Object.h` — `vsg::Object` base, intrusive ref-counting, RTTI, metadata API, `CopyOp`/`Duplicate`, free `cast`/`clone`.
- `include/vsg/core/Inherit.h` — `vsg::Inherit<Parent,Subclass>` CRTP helper providing `create()` and dispatch.
- `include/vsg/core/observer_ptr.h` — `vsg::observer_ptr<T>` weak smart pointer.
- `include/vsg/core/Auxiliary.h` — side table backing metadata and weak-pointer connectivity.
- `examples/core/vsgvalues/vsgvalues.cpp` — `setValue`/`getValue` metadata usage.
- `examples/core/vsgpointer/vsgpointer.cpp` — `ref_ptr`/`Object`/`Auxiliary` size & ownership demonstration.
- `examples/core/vsgvisitorcustomtype/VisitorCustomType.h` — `Inherit<Group, CustomGroupNode>` subclassing.
- `examples/threading/vsgdynamicload/vsgdynamicload.cpp` — `observer_ptr<Viewer>` weak handle across threads.
- `examples/ui/vsginput/vsginput.cpp` — promoting `observer_ptr` to `ref_ptr` before use.

## Common mistakes
- `new vsg::Group()` -> use `vsg::Group::create()`; manual `new` bypasses the factory and the destructor is protected so it won't compile cleanly anyway (`Object.h:182`, `Inherit.h:35`).
- `delete obj;` -> never delete; drop the last `ref_ptr` and cleanup happens automatically (`Object.h:123`).
- Storing an owning handle in a raw `T*` member -> store it in a `ref_ptr<T>` so the object stays alive (`ref_ptr.h:21`).
- Dereferencing an `observer_ptr` directly -> promote to `ref_ptr` first, then check validity (`observer_ptr.h:177`).
- `static_cast<Derived*>(obj.get())` blindly -> use `obj.cast<Derived>()` / `vsg::cast<Derived>(obj)` and check for `nullptr` (`ref_ptr.h:192`, `Object.h:195`).
- Deriving `class Foo : public vsg::Object` directly -> derive via `vsg::Inherit<vsg::Object, Foo>` so RTTI, `create()`, and `accept()` are wired (`Inherit.h:27`).

## Things to never invent
- There is no `T::make(...)`, `vsg::make_ref<T>()`, or `std::make_shared`-style helper — the factory is `T::create(...)` only (`Inherit.h:35`).
- `ref_ptr` has no `make_unique`/`weak()`/`lock()` member; weak handling lives in `observer_ptr` (and you promote via `ref_ptr()` / implicit conversion, `observer_ptr.h:173`).
- `observer_ptr` has no `expired()` method — test liveness with `valid()` or `explicit operator bool()` (`observer_ptr.h:168`, `observer_ptr.h:170`).
- `Object` exposes `ref()`/`unref()`/`unref_nodelete()`/`referenceCount()` but no `incrementReferenceCount`/`addRef`/`release` (`Object.h:122`-`Object.h:128`); `ref_ptr` has `release_nodelete()` (`ref_ptr.h:175`) but no plain `release()`.
- Do not assume `cast<T>()` uses `dynamic_cast` — under the default build it uses `is_compatible`/`static_cast` (`Object.h:95`), so it relies on the registered `type_info`, not C++ RTTI inheritance walking.
- There is no public `Object` constructor that takes a reference count, and `~Object()` is not public (`Object.h:182`).
