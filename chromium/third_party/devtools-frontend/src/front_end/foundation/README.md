# DevTools foundation

DevTools foundation is the core business logic of DevTools: mainly core/ and models/.
The difference between "foundation" and the rest of DevTools is
that the code is targeted not just for the browser, but also the Node.js runtime.
As such, allowed use of APIs is restricted to what is available in both runtimes.

A `DevToolsUniverse` is a concrete, encapsulated instance of "foundation" scoped
to a single root CDP target. It is valid to create multiple `DevToolsUniverse`
instances simultaneously.

## Architecture Shortcomings & Code Health Projects

The following is a prioritized list of architectural shortcomings and refactoring projects aimed at reducing coupling, eliminating global state, and improving the modularity of the DevTools foundation.

### Priority 1: Move `models/text_utils` to `core/text_utils` [DONE]
*   **Problem**: 22 files in `front_end/core/sdk` depend directly on `front_end/models/text_utils` for basic text structures (layering violation).
*   **Rationale**: `text_utils` contains fundamental, non-business-logic utilities (`TextRange`, `TextCursor`, `ContentData`, `ContentProvider`) that represent the base data structures of the editor, debugger, and network panels. It only depends on `core/common` and `core/platform`. Moving it to `core` is a natural fit and resolves the majority of `core` -> `models` layering violations.
*   **Action**:
    1. Move the directory `front_end/models/text_utils` to `front_end/core/text_utils`.
    2. Update all imports across the codebase.

### Priority 2: Break Circular Dependencies in Bindings (`ResourceMapping` <-> Bindings)
*   **Problem**: Bidirectional dependencies between `ResourceMapping` and `DebuggerWorkspaceBinding`/`CSSWorkspaceBinding` prevent isolated testing.
*   **Rationale**: Decoupling `ResourceMapping` from concrete bindings allows better testing and modularity.
*   **Action**: Introduce abstract listener interfaces in `ResourceMapping` (Dependency Inversion). `DebuggerWorkspaceBinding` and `CSSWorkspaceBinding` should implement these interfaces and register themselves, allowing `ResourceMapping` to invoke them without depending on concrete classes.

### Priority 3: Move `IgnoreListManager` from `workspace` to `bindings`
*   **Problem**: `models/workspace` depends on `core/sdk` via `IgnoreListManager.ts`.
*   **Rationale**: Ignore-listing is conceptually a debugger-binding concern, not a basic workspace/file-system concern.
*   **Action**:
    1. Remove the convenience helper `UISourceCode.isIgnoreListed()` (and related methods in `UILocation`) from `models/workspace`.
    2. Update callers to query `IgnoreListManager` directly.
    3. Move `IgnoreListManager.ts` to `models/bindings`.
    4. This removes the value-level dependency of `workspace` on `sdk`.

### Priority 4: Decouple `Workspace.Project` from `SDK.Target`
*   **Problem**: `Workspace.Project` exposes `target()`, coupling the VFS to runtime targets.
*   **Rationale**: A workspace should be able to exist without knowing about browser targets.
*   **Action**:
    1. Remove `target()` from the `Project` interface and `ProjectStore` in `front_end/models/workspace/WorkspaceImpl.ts`.
    2. In the `bindings` module, maintain a private `WeakMap<Workspace.Project, SDK.Target.Target>` to associate projects with targets.
    3. Update `NetworkProject.targetForUISourceCode(uiSourceCode)` to look up the target from this `WeakMap`.

### Priority 5: Decouple `core/sdk` from `models/formatter` (AST Scope Fallback)
*   **Problem**: `SDK/SourceMap.ts` depends on `models/formatter` for fallback scope calculation.
*   **Rationale**: `SDK` should not depend on high-level formatting/parsing worker pools.
*   **Action**:
    1. Remove the fallback scope calculation logic from `SourceMap.ts` and expose a clean API (e.g., `setScopesInfo(...)`).
    2. Move the AST fallback scope calculation logic to `front_end/models/bindings` (e.g., in `DebuggerWorkspaceBinding`), which coordinates script and source map loading.
    3. Have the bindings layer calculate the fallback scopes and attach them to the `SourceMap` object.

### Priority 6: Refactor Mapping Engines & Inner Helpers in Bindings
*   **Problem**: Engines and helpers query global `DebuggerWorkspaceBinding.instance()` directly.
*   **Rationale**: High coupling hinders testing and isolation.
*   **Action**: Eliminate direct global singleton queries. Update helpers to access parent mapping engines or pass references via constructors.

### Priority 7: Universe-scoped Embedder Host and HostConfig
*   **Problem**: `InspectorFrontendHostInstance` and `Root.Runtime.hostConfig` are treated as global mutable singletons. Core classes (`Connections`, `IsolatedFileSystemManager`, `FileManager`) and UI components access them directly, leading to global state pollution and state leaks in tests.
*   **Rationale**: The host interface and its configuration represent the boundary between DevTools and the Chromium browser. To support multiple isolated `DevToolsUniverse` instances, both the host interface and host configuration must be scoped to the universe context.
*   **Action**:
    1. Store the scoped `InspectorFrontendHostAPI` (wrapped in `ScopedInspectorFrontendHost`) in the `DevToolsUniverse` context.
    2. Refactor core classes (`Connections`, `IsolatedFileSystemManager`, `FileManager`, `UserMetrics`) to receive the scoped host via constructor injection.
    3. Introduce a scoped `HostConfigProvider` service in the `Universe` context to manage `HostConfig` fetching and caching.
    4. Move polling and configuration update logic from `HostConfigTracker` in `AidaClient.ts` to `HostConfigProvider`.
    5. Provide a compatibility Proxy for `Root.Runtime.hostConfig` to allow gradual migration of the 200+ call sites.
    6. Remove the global side-effect initialization in `InspectorFrontendHost.ts`.

### Priority 8: View-Level Dependency Injection (Universe-ification)
*   **Problem**: UI panels query global `.instance()` for bindings, causing state leaks in tests.
*   **Rationale**: DevTools is transitioning to a scoped "Universe" architecture where dependencies are injected. UI panels should receive their dependencies rather than querying globals.
*   **Action**:
    1. Expose bindings on `Universe` and inject them via the view loading pipeline.
    2. Leverage the view loading pipeline (`-meta.ts` files) to pass these bindings to panel constructors upon initialization, avoiding global calls in UI panels.

### Priority 9: Decouple `persistence` from `bindings`
*   **Problem**: `models/persistence` depends on `models/bindings` to check if a target is Node.js.
*   **Rationale**: `persistence` (which maps network files to local files) should ideally only depend on `workspace` and not on the complex debugger/bindings layer.
*   **Action**: Introduce a capability/property on `Workspace.Project` (e.g., `isNodeProject`) to avoid direct target queries.

### Priority 10: Move `SourceFrameIssuesManager` to `bindings`
*   **Problem**: `models/issues_manager` depends on `workspace` and `bindings` only for placing issue badges.
*   **Rationale**: Showing badges on source frames is a binding/presentation concern, similar to showing console messages. `issues_manager` itself should be a clean, independent leaf module.
*   **Action**: Move `SourceFrameIssuesManager.ts` to `models/bindings/` and remove `workspace` and `bindings` dependencies from `issues_manager/BUILD.gn`.

### Priority 11: Decouple UI via Interfaces / Revealers
*   **Problem**: General UI components (like `Linkifier`) depend on heavy concrete bindings.
*   **Rationale**: UI components should not be tightly coupled to complex mapping logic.
*   **Action**: Use light interfaces or `Common.Revealer`.

### Priority 12: Decouple Core Modules from `i18n` (UI Localization)
*   **Problem**: Core infra/parser modules import `i18n` for localized strings.
*   **Rationale**: Business logic and core infrastructure should not be tied to UI localization libraries.
*   **Action**:
    1. Refactor parsers to return structured error data and let UI handle localization.
    2. Update registries to use string IDs/keys.
