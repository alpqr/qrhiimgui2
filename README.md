New integration of Qt Quick and Dear ImGui. Requires Qt 6.4 or newer.

This now hides the Qt Quick threading model completely, and is a
QQuickItem, meaning proper input event processing (keyboard focus,
stacking order for mouse events, etc.).

Applications can subclass QRhiImguiItem, reimplement frame() to do
ImGui stuff (safely on the main thread, even if the Qt Quick rendering
has its own render thread), register the item via the Qt 6 facilities
(QML_NAMED_ELEMENT, qt_add_qml_module), then instantiate somewhere in
the QML scene. (however, while this is a true QQuickItem, it is still
expected to be sized to cover the whole window; input and clipping may
be incorrect otherwise)

Keyboard input mapping is somewhat incomplete. Should be migrated
anyway to AddKeyEvent() so won't touch that now.
