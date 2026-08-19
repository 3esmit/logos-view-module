# `LogosView*.in` — the view-plugin templates

These four files are `configure_file` templates instantiated per module by
`logos_replica_factory()` (this directory's `LogosViewModule.cmake`) and by
`logos_module(REP_FILE ...)` in logos-module-builder:

| template | produces | linked into |
|---|---|---|
| `LogosViewPluginBase.{h,cpp}.in` | `<Rep>ViewPluginBase` | the module plugin |
| `LogosViewReplicaFactory.{h,cpp}.in` | `<Rep>ReplicaFactoryPlugin` | `<name>_replica_factory` |

They are pure Qt: `Q_OBJECT`, `Q_PLUGIN_METADATA`, `repc`-generated
`*SimpleSource`/`*Replica` types, `qmlRegisterUncreatableMetaObject`. Nothing
outside a Qt view plugin can use them.

## The rule

**There is exactly one copy of each, and it is this one.** No consumer keeps a
local copy — not even a test fixture. `LogosModule.cmake` in
logos-module-builder hard-errors when `LOGOS_VIEW_TEMPLATE_DIR` is unset rather
than falling back to a sibling copy, and so does `tests/rep-file-plugin` here:
a silent fallback to a second copy is the failure this layout exists to remove.

They are also **siblings of `LogosViewModule.cmake` by requirement** — that
function resolves them through `CMAKE_CURRENT_FUNCTION_LIST_DIR`. Move the
`.cmake` without them and `configure_file()` fails.

## Why this repo

This repo owns the `ui_qml` / `interface: universal` authoring flavour end to
end: the `.rep`-driven generator, `LogosViewModule.cmake`, the
`LogosUiPluginContext` header, and these templates. A view module is a distinct
authoring flavour rather than a variant of a core module, so changing how views
are built is a one-repo change.

It also *can* be everyone's copy. This repo is a leaf — its only flake input is
`logos-nix` — so any consumer can depend on it without a cycle. That is the
property the previous home lacked.

### What moved, and why

These templates used to live in **logos-plugin-qt** (`cmake/`), and before that
next to `LogosModule.cmake` in **logos-module-builder**, with each move driven
by which repo the then-current set of consumers could all reach:

* next to `LogosModule.cmake`: the natural home, since that is the code that
  instantiates them — but the `rep-file-plugin` fixture in logos-plugin-qt also
  instantiates them and cannot reach logos-module-builder (the dependency runs
  logos-module-builder → logos-plugin-qt, one way), so the fixture ended up
  holding a byte-identical second copy with nothing comparing the two;
* logos-plugin-qt: fixed that, because logos-module-builder can read it and the
  fixture lives there — but it made the Qt *plugin-loading backend* the owner of
  a view-plugin *authoring* concern.

logos-plugin-qt now handles exclusively what makes a cdylib module loadable by
`logos-module-loader-qt`. Its `cmake/` directory is gone, this repo took the
templates and the `rep-file-plugin` fixture, and logos-module-builder reads them
from here. The set of consumers did not change; only the repo all of them can
reach did.

`LogosModule.cmake` did **not** come along — that file is
logos-module-builder's build-system contract (`LOGOS_API_STYLE`,
`LOGOS_MODULE_GO_STATIC_LIBS`, `generated_code/`) and stays there. Only the
Qt-specific view templates it instantiates are published from this side.

## How consumers get them

* nix: `packages.<system>.logos-view-templates` — the four `.in` files flat in
  one directory, which is the shape `LOGOS_VIEW_TEMPLATE_DIR` wants.
  `packages.<system>.cmake-module` publishes the same bytes under
  `share/cmake/LogosViewModule/`, next to the `.cmake` that resolves them as
  siblings. Same content, two addressing schemes, one source.
* logos-module-builder passes that directory into every `ui_qml` module build —
  as `-DLOGOS_VIEW_TEMPLATE_DIR=` and as an exported environment variable, so
  both `nix build` and a hand-run `cmake` in a module dev shell resolve it.
* CMake: a consumer resolves `LOGOS_VIEW_TEMPLATE_DIR` (cache variable first,
  then environment) and hard-errors when it is unset.

## The interface declared inside the templates

`LogosViewPlugin` and `LogosViewReplicaFactory` are also declared, separately,
by logos-view-module-runtime — the host that loads these plugins. That
duplication is deliberate and cannot be collapsed: a module plugin must build
against Qt alone, without the host runtime on its include path. The two
declarations meet at runtime via the IID string, where a mismatch is silent.

That pair is enforced instead of documented: logos-module-builder's
`view-interface-abi` check (it is the one repo that can see both sides)
compares them and fails on any difference. It reads three separate things,
because three separate strings have to line up:

* `#define <Name>_iid` and the ordered pure-virtual list — the abstract shape;
* the argument of `Q_DECLARE_INTERFACE`, **resolved** through the `#define`s,
  since that is what `qobject_cast` compares and it need not be the macro;
* on the **concrete** classes in these templates — the ones carrying
  `Q_OBJECT`, `Q_PLUGIN_METADATA` and `Q_INTERFACES` — the exported IID and
  the declared interface list.

That last group is the runtime binding, and for a while it was outside the
check's window: bumping `Q_PLUGIN_METADATA(IID ...)` to `/2.0`, or deleting
`Q_INTERFACES`, left the guard green while breaking a real view.

Locally, `../tests/rep-file-plugin` covers the same two mutations from the other
direction — it builds a plugin from these templates, asserts the **exact** IID
in the resulting binary (not a substring of it), then `QPluginLoader`s it and
performs the same `qobject_cast` the host does.
