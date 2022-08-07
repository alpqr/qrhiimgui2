New integration of Qt Quick and Dear ImGui. Requires Qt 6.5 (the dev branch at
the time of writing).

This now hides the Qt Quick threading model completely, and is a QQuickItem,
meaning proper input event processing (keyboard focus, stacking order for mouse
events, etc.). The Qt Quick scenegraph integration happens via QSGRenderNode,
which we now try to use in the most optimal way (i.e. so that it won't
auto-disable the renderer's opaque pass, but sadly we can only do this with Qt
6.5+), while getting proper stacking with other items in the scene. Unlike
earlier attempts, the item size and the window device pixel ratio are respected
as well. (NB scaling the item or rotating around any axis will lead to
incorrect scissoring)

Applications can subclass QRhiImguiItem, reimplement frame() to do ImGui stuff
(safely on the main thread, even if the Qt Quick rendering has its own render
thread), register the item via the Qt 6 facilities (QML_NAMED_ELEMENT,
qt_add_qml_module), then instantiate somewhere in the QML scene.

Keyboard input mapping is somewhat incomplete. Should be migrated anyway to
AddKeyEvent() later on, so won't touch that now.
