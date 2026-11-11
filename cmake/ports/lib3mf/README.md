# lib3mf overlay port

The upstream vcpkg port of lib3mf does not list Emscripten in its `supports` expression, so it
cannot be built for the browser target of the WebAssembly core (AP-6.7a, ADR-0014). This overlay
adds Emscripten to that line. Keep the files in sync with the vcpkg baseline in `.vcpkg` and
remove the overlay once the upstream port supports Emscripten.
