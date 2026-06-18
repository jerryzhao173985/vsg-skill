---
title: The object & memory model
description: How every VSG object is allocated, typed, reference-counted, and traversed — the ref_ptr / Inherit / Visitor substrate beneath the whole library.
---

## What this covers
- The intrusive reference-counting object model: `vsg::Object` base, `ref_ptr<T>` (strong) and `observer_ptr<T>` (weak), the `T::create()` factory and `Inherit<Base, Derived>` CRTP wiring.
- Why VSG never uses `new`/`delete`, raw owning pointers, or `std::shared_ptr` for scene objects.
- Custom RTTI (`type_info`/`is_compatible`/`cast<T>`) and the `Visitor`/`ConstVisitor` double-dispatch (`accept`/`apply`) traversal model.
- The `Auxiliary` side-table that backs rarely-used metadata and weak connectivity, and the `Allocator` singleton backing every allocation.

## Mental model
Every persistent VSG class derives from `vsg::Object`, which carries one `std::atomic_uint _referenceCount` and one `Auxiliary*` pointer (`include/vsg/core/Object.h:190-192`). Reference counting is *intrusive*: the count lives inside the object, not in a separate control block as `std::shared_ptr` uses. `ref()` does a relaxed atomic increment; `unref()` does a `seq_cst` decrement and, when the count reaches zero, calls `_attemptDelete()` (`include/vsg/core/Object.h:122-126`). `_attemptDelete()` calls `delete this` only if there is no `Auxiliary` or the `Auxiliary` agrees the object may die (`src/vsg/core/Object.cpp:84-100`). The destructor is `protected` (`include/vsg/core/Object.h:182`), so you *cannot* stack-allocate a VSG object or `delete` one yourself — lifetime is owned entirely by the ref count.

`ref_ptr<T>` is the strong handle that drives that count. Its constructors/assignment call `_ptr->ref()` when taking ownership and its destructor calls `_ptr->unref()` (`include/vsg/core/ref_ptr.h:50-70`). It is pointer-sized (one `T*`), which is why it is "half size and faster" than `std::shared_ptr` (`include/vsg/core/ref_ptr.h:18-19`). The canonical way to mint an object is the static `create()` factory, which news the object and immediately wraps it in a `ref_ptr` so the count is managed from birth (`include/vsg/core/Inherit.h:34-38`). Because the raw object starts at count 0, the *first* `ref_ptr` to adopt it brings it to 1; letting the last `ref_ptr` drop frees it.

`Inherit<ParentClass, Subclass>` is the CRTP glue every concrete class uses (`class Group : public Inherit<Node, Group>`). In one template it synthesises `create()`/`create_if()`, `sizeofObject()`, `className()`, the custom RTTI (`type_info()`, `is_compatible()`), `compare()`, and the three `accept()` overloads (`include/vsg/core/Inherit.h:34-72`). This is why authors never hand-write any of that boilerplate — deriving from `Inherit` *is* the contract.

RTTI is hand-rolled, not (by default) `dynamic_cast`. `is_compatible()` walks the inheritance chain comparing `typeid` against each level (`include/vsg/core/Inherit.h:50`), and `Object::cast<T>()` uses it to do a checked `static_cast` (`include/vsg/core/Object.h:95-99`). `ref_ptr<T>::cast<R>()` forwards to it (`include/vsg/core/ref_ptr.h:192-193`). So `obj->cast<MatrixTransform>()` returns `nullptr` on a type mismatch — like `dynamic_cast` but cheaper and not requiring RTTI to be enabled.

Traversal is double dispatch. A `Visitor` declares a virtual `apply(T&)` per known type (`include/vsg/core/Visitor.h:193-217`). `node->accept(visitor)` resolves on the node's dynamic type via `Inherit`, which calls `visitor.apply(static_cast<Subclass&>(*this))` (`include/vsg/core/Inherit.h:70-72`) — so the *exact* `apply` overload fires without the visitor switching on type. Default `apply(T&)` overloads chain up to the base type's overload (`src/vsg/core/Visitor.cpp:25-43`), so a visitor that overrides only `apply(Group&)` still sees every group. Recursion is explicit: `apply` must call `object.traverse(*this)` to descend, otherwise traversal stops at that node (`examples/core/vsgvisitor/vsgvisitor.cpp:58-63`).

`Auxiliary` is a lazily-created side-table for the rarely-used 5%: user metadata (`userObjects` map) and the weak-pointer back-channel. Most objects never allocate one, keeping `Object` lean. It is created on demand by `getOrCreateAuxiliary()` (`src/vsg/core/Object.cpp:225-234`) and is itself ref-counted, holding a back-pointer `_connectedObject` guarded by a mutex (`include/vsg/core/Auxiliary.h:103-106`). `observer_ptr<T>` (weak) holds a `ref_ptr<Auxiliary>` rather than a `ref_ptr<T>`, so it keeps the *Auxiliary* alive but not the object; promoting it to `ref_ptr<T>` locks the mutex and checks `getConnectedObject()` is still non-null (`include/vsg/core/observer_ptr.h:176-186`).

All of this memory comes from the `Allocator` singleton. `Object::operator new` routes through `vsg::allocate(count, ALLOCATOR_AFFINITY_OBJECTS)` and `operator delete` through `vsg::deallocate` (`src/vsg/core/Object.cpp:236-244`), which forward to `Allocator::instance()` — by default an `IntrusiveAllocator` pooling memory per affinity bucket (`src/vsg/core/Allocator.cpp:24-42`).

## Key types
- `vsg::Object` — universal base: atomic ref count, optional `Auxiliary*`, protected dtor, RTTI/accept/clone/compare virtuals (`include/vsg/core/Object.h:59`).
- `vsg::ref_ptr<T>` — strong, pointer-sized intrusive owning handle; ref/unref on copy/move/destroy (`include/vsg/core/ref_ptr.h:20`).
- `vsg::observer_ptr<T>` — weak handle; holds `ref_ptr<Auxiliary>`, promotes to `ref_ptr<T>` under lock (`include/vsg/core/observer_ptr.h:23`).
- `vsg::Inherit<ParentClass, Subclass>` — CRTP base supplying `create()`, RTTI, `sizeofObject()`, `compare()`, `accept()` (`include/vsg/core/Inherit.h:26`).
- `vsg::Auxiliary` — lazily-created metadata side-table + weak back-pointer; ref-counted, mutex-guarded (`include/vsg/core/Auxiliary.h:25`).
- `vsg::Visitor` / `vsg::ConstVisitor` — base for double-dispatch traversal via per-type `apply(T&)` (`include/vsg/core/Visitor.h:178`).
- `vsg::Allocator` — abstract singleton (default `IntrusiveAllocator`) backing every `Object::operator new` (`include/vsg/core/Allocator.h:41`).

## How it works (implementation-grounded)
- **Birth.** `Inherit<Base,Derived>::create(args...)` runs `new Subclass(args...)` (which calls `Object::operator new` → `vsg::allocate(size, ALLOCATOR_AFFINITY_OBJECTS)`, `src/vsg/core/Object.cpp:236-239`) and wraps the result in `ref_ptr<Subclass>` (`include/vsg/core/Inherit.h:34-38`). The new object has `_referenceCount == 0` (`src/vsg/core/Object.cpp:25-29`); the returned `ref_ptr`'s constructor refs it to 1 (`include/vsg/core/ref_ptr.h:50-54`).
- **Sharing.** Copying a `ref_ptr` refs the target; assigning refs the new target *before* unref-ing the old one, so self-/ancestor-assignment can't free the object mid-swap (`include/vsg/core/ref_ptr.h:94-108`). Moving steals the pointer without touching the count (`include/vsg/core/ref_ptr.h:36-41`).
- **Death.** The last `unref()` decrements to 0 and calls `_attemptDelete()` (`include/vsg/core/Object.h:123-126`). With no `Auxiliary`, it `delete this` immediately; with one, it asks `signalConnectedObjectToBeDeleted()`, which under a mutex re-checks the ref count (an `observer_ptr` may have just promoted) and either vetoes or nulls `_connectedObject` and allows the delete (`src/vsg/core/Object.cpp:84-100`, `src/vsg/core/Auxiliary.cpp:54-69`). `~Object()` unrefs the `Auxiliary` (`src/vsg/core/Object.cpp:74-82`).
- **Type queries.** `is_compatible(typeid(T))` short-circuits up the chain (`include/vsg/core/Inherit.h:50`); `cast<T>()` returns a checked pointer or `nullptr` (`include/vsg/core/Object.h:95-99`). `compare()` first compares `type_index`, then `Inherit::compare` memcmp's the bytes the subclass adds over its parent (`include/vsg/core/Inherit.h:52-68`).
- **Traversal.** `accept(visitor)` on a concrete node dispatches to `visitor.apply(static_cast<Subclass&>(*this))` (`include/vsg/core/Inherit.h:70-72`); the visitor's overload then calls `node.traverse(*this)` to recurse, or not (`examples/core/vsgvisitor/vsgvisitor.cpp:58-69`). `vsg::visit<MyVisitor>(scene)` is sugar that default-constructs the visitor, calls `accept`, and returns it for querying (`examples/core/vsgvisitor/vsgvisitor.cpp:76`, `include/vsg/core/visit.h:42`).
- **Metadata.** `setValue/setObject` call `getOrCreateAuxiliary()` (allocating the side-table on first use) and store into `userObjects`; getters return `nullptr` when no `Auxiliary` exists, never allocating (`src/vsg/core/Object.cpp:172-207`, `225-234`).

## Rules that follow
- Always create objects with `T::create(...)`, never `new T` or stack/`std::make_shared` — only `create()` correctly seeds the intrusive count and routes through the allocator (`include/vsg/core/Inherit.h:34-38`, `src/vsg/core/Object.cpp:236-239`).
- Hold VSG objects in `ref_ptr<T>` (or a VSG container of them) for the duration you need them; the object is freed the instant the last `ref_ptr` drops (`include/vsg/core/ref_ptr.h:67-70`, `include/vsg/core/Object.h:123-126`).
- Never `delete` a VSG object or declare one on the stack: the destructor is `protected` (`include/vsg/core/Object.h:182`).
- Write every new concrete class as `class Foo : public vsg::Inherit<Base, Foo>` and give it a `protected`/`= default` destructor; do not hand-write `create`, `accept`, or RTTI (`include/vsg/core/Inherit.h:26-73`, `examples/core/vsgvisitorcustomtype/VisitorCustomType.h:34-50`).
- Use `obj->cast<T>()` / `vsg::ref_ptr<T>::cast<T>()` (not C++ `dynamic_cast`) for VSG downcasts; check the result for `nullptr` (`include/vsg/core/Object.h:95-99`).
- In a visitor, call `object.traverse(*this)` inside `apply(...)` to descend; omitting it deliberately prunes the subtree (`examples/core/vsgvisitor/vsgvisitor.cpp:58-63`).
- Promote an `observer_ptr<T>` to a `ref_ptr<T>` before dereferencing it — the conversion locks the `Auxiliary` mutex and may yield null if the object died (`include/vsg/core/observer_ptr.h:176-186`, `examples/ui/vsginput/vsginput.cpp:262-263`).
- To break ownership cycles (e.g. a child referring back to a parent/viewer), use `observer_ptr<T>`, not `ref_ptr<T>` (`examples/threading/vsgdynamicviews/vsgdynamicviews.cpp:22`).

## Common mistakes
- `new Group()` / `std::make_shared<Group>()` → `vsg::Group::create()`; raw `new` skips the allocator and intrusive count, `shared_ptr` adds a second control block VSG ignores (`include/vsg/core/Inherit.h:34-38`).
- Stack-allocating `vsg::Group g;` → won't compile; the destructor is protected — always heap via `create()` (`include/vsg/core/Object.h:182`).
- Storing a node in a bare `Foo*` member that owns it → use `ref_ptr<Foo>`; a raw pointer doesn't ref and the object can be freed under you (`include/vsg/core/ref_ptr.h:50-54`).
- Caching an object in a `ref_ptr` "to be safe" when you only observe it (parent/back-references) → use `observer_ptr` to avoid a reference cycle that leaks (`include/vsg/core/observer_ptr.h:23`).
- `dynamic_cast<MatrixTransform*>(node)` → `node->cast<MatrixTransform>()`; VSG's custom RTTI works even with `dynamic_cast` disabled (`include/vsg/core/Object.h:95-99`).
- Writing a visitor `apply` and forgetting `traverse(*this)`, then wondering why children aren't visited → recursion is opt-in, not automatic (`examples/core/vsgvisitor/vsgvisitor.cpp:58-63`).
- Dereferencing `observer_ptr::get()` directly across threads → promote to `ref_ptr` first; `get()` can dangle (`include/vsg/core/observer_ptr.h:188-189`).

## Source references
- `include/vsg/core/Object.h` — base object: ref count, protected dtor, RTTI, accept/clone/compare, Auxiliary access
- `include/vsg/core/ref_ptr.h` — strong intrusive pointer (ref/unref, cast, assignment ordering)
- `include/vsg/core/Inherit.h` — CRTP wiring of create/RTTI/accept/compare
- `include/vsg/core/observer_ptr.h` — weak pointer backed by Auxiliary
- `include/vsg/core/Auxiliary.h` — metadata side-table + weak back-pointer
- `include/vsg/core/Allocator.h` — allocator singleton + affinity buckets
- `include/vsg/core/Visitor.h` — Visitor base + per-type apply overloads
- `include/vsg/core/visit.h` — `vsg::visit<>` convenience templates
- `src/vsg/core/Object.cpp` — `_attemptDelete`, `getOrCreateAuxiliary`, `operator new/delete`, metadata
- `src/vsg/core/Auxiliary.cpp` — ref/unref, `signalConnectedObjectToBeDeleted`
- `src/vsg/core/Allocator.cpp` — `instance()`, `vsg::allocate`/`deallocate`
- `src/vsg/core/Visitor.cpp` — default `apply` chaining to base type
- `examples/core/vsgvisitor/vsgvisitor.cpp` — ConstVisitor + traverse + `vsg::visit<>`
- `examples/core/vsgvisitorcustomtype/VisitorCustomType.h` — custom `Inherit` node/visitor types
- `examples/core/vsgpointer/vsgpointer.cpp` — ref_ptr vs shared_ptr sizing, create()
- `examples/ui/vsginput/vsginput.cpp` — observer_ptr → ref_ptr promotion
- `examples/threading/vsgdynamicviews/vsgdynamicviews.cpp` — observer_ptr to break cycles
