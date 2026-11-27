# GLFW-3.4-Paritätsaudit

Ziel: alle **118 Nicht-Vulkan-Funktionen** von GLFW 3.4 haben eine Entsprechung
in `sgl`. Die sechs Vulkan-Funktionen (`glfwVulkanSupported`,
`glfwGetRequiredInstanceExtensions`, `glfwGetInstanceProcAddress`,
`glfwGetPhysicalDevicePresentationSupport`, `glfwCreateWindowSurface`,
`glfwInitVulkanLoader`) sind bewusst nicht enthalten.

## Initialisierung, Fehler, Plattform

| GLFW 3.4 | sgl |
|---|---|
| `glfwInit` | `sgl::Init()` |
| `glfwTerminate` | `sgl::Terminate()` |
| `glfwInitHint` | `sgl::SetInitHint()` |
| `glfwInitAllocator` | `sgl::InitOptions::allocator` |
| `glfwGetVersion` | `sgl::GetVersion()` |
| `glfwGetVersionString` | `sgl::GetVersionString()` |
| `glfwGetError` | `sgl::GetError()` |
| `glfwSetErrorCallback` | `sgl::SetErrorCallback()` |
| `glfwGetPlatform` | `sgl::GetPlatform()` |
| `glfwPlatformSupported` | `sgl::IsPlatformSupported()` |

## Monitore

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetMonitors` | `sgl::GetMonitors()` |
| `glfwGetPrimaryMonitor` | `sgl::GetPrimaryMonitor()` |
| `glfwGetMonitorPos` | `Monitor::getPosition()` |
| `glfwGetMonitorWorkarea` | `Monitor::getWorkarea()` |
| `glfwGetMonitorPhysicalSize` | `Monitor::getPhysicalSize()` |
| `glfwGetMonitorContentScale` | `Monitor::getContentScale()` |
| `glfwGetMonitorName` | `Monitor::getName()` |
| `glfwSetMonitorUserPointer` | `Monitor::setUserPointer()` |
| `glfwGetMonitorUserPointer` | `Monitor::getUserPointer()` |
| `glfwSetMonitorCallback` | `sgl::SetMonitorCallback()` |
| `glfwGetVideoModes` | `Monitor::getVideoModes()` |
| `glfwGetVideoMode` | `Monitor::getVideoMode()` |
| `glfwSetGamma` | `Monitor::setGamma()` |
| `glfwGetGammaRamp` | `Monitor::getGammaRamp()` |
| `glfwSetGammaRamp` | `Monitor::setGammaRamp()` |

## Fenster

| GLFW 3.4 | sgl |
|---|---|
| `glfwDefaultWindowHints` | `sgl::GetDefaultWindowHints()` |
| `glfwWindowHint` | `WindowHints`-Feld |
| `glfwWindowHintString` | `WindowHints`-Stringfeld |
| `glfwCreateWindow` | `Window::Create()` |
| `glfwDestroyWindow` | `Window`-Destruktor |
| `glfwWindowShouldClose` | `Window::isCloseRequested()` |
| `glfwSetWindowShouldClose` | `Window::setCloseRequested()` |
| `glfwGetWindowTitle` | `Window::getTitle()` |
| `glfwSetWindowTitle` | `Window::setTitle()` |
| `glfwSetWindowIcon` | `Window::setIcon()` |
| `glfwGetWindowIcon` | `Window::getIcon()` |
| `glfwSetWindowPos` | `Window::setPosition()` |
| `glfwGetWindowPos` | `Window::getPosition()` |
| `glfwGetWindowSize` | `Window::getSize()` |
| `glfwSetWindowSize` | `Window::setSize()` |
| `glfwSetWindowSizeLimits` | `Window::setSizeLimits()` |
| `glfwSetWindowAspectRatio` | `Window::setAspectRatio()` |
| `glfwGetFramebufferSize` | `Window::getFramebufferSize()` |
| `glfwGetWindowFrameSize` | `Window::getFrameSize()` |
| `glfwGetWindowContentScale` | `Window::getContentScale()` |
| `glfwGetWindowOpacity` | `Window::getOpacity()` |
| `glfwSetWindowOpacity` | `Window::setOpacity()` |
| `glfwIconifyWindow` | `Window::iconify()` |
| `glfwRestoreWindow` | `Window::restore()` |
| `glfwMaximizeWindow` | `Window::maximize()` |
| `glfwShowWindow` | `Window::show()` |
| `glfwHideWindow` | `Window::hide()` |
| `glfwFocusWindow` | `Window::focus()` |
| `glfwRequestWindowAttention` | `Window::requestAttention()` |
| `glfwGetWindowMonitor` | `Window::getMonitor()` |
| `glfwSetWindowMonitor` | `Window::setMonitor()` |
| `glfwGetWindowAttrib` | `Window::getAttrib()` / `isAttrib()` |
| `glfwSetWindowAttrib` | `Window::setAttrib()` |
| `glfwSetWindowUserPointer` | `Window::setUserPointer()` |
| `glfwGetWindowUserPointer` | `Window::getUserPointer()` |

## Fenster-Callbacks

| GLFW 3.4 | sgl |
|---|---|
| `glfwSetWindowPosCallback` | `Window::setPositionCallback()` |
| `glfwSetWindowSizeCallback` | `Window::setSizeCallback()` |
| `glfwSetWindowCloseCallback` | `Window::setCloseCallback()` |
| `glfwSetWindowRefreshCallback` | `Window::setRefreshCallback()` |
| `glfwSetWindowFocusCallback` | `Window::setFocusCallback()` |
| `glfwSetWindowIconifyCallback` | `Window::setIconifyCallback()` |
| `glfwSetWindowMaximizeCallback` | `Window::setMaximizeCallback()` |
| `glfwSetFramebufferSizeCallback` | `Window::setFramebufferSizeCallback()` |
| `glfwSetWindowContentScaleCallback` | `Window::setContentScaleCallback()` |

## Eingabe

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetInputMode` | `Window::getInputMode()` |
| `glfwSetInputMode` | `Window::setInputMode()` |
| `glfwRawMouseMotionSupported` | `Window::isRawMouseMotionSupported()` / `sgl::IsRawMouseMotionSupported()` |
| `glfwGetKeyName` | `Window::getKeyName()` / `sgl::GetKeyName()` |
| `glfwGetKeyScancode` | `Window::getKeyScancode()` / `sgl::GetKeyScancode()` |
| `glfwGetKey` | `Window::isKeyPressed()` |
| `glfwGetMouseButton` | `Window::isMouseButtonPressed()` |
| `glfwGetCursorPos` | `Window::getCursorPos()` |
| `glfwSetCursorPos` | `Window::setCursorPos()` |
| `glfwCreateCursor` | `Cursor::Create()` |
| `glfwCreateStandardCursor` | `Cursor::CreateStandard()` |
| `glfwDestroyCursor` | `Cursor`-Destruktor |
| `glfwSetCursor` | `Window::setCursor()` |
| `glfwSetKeyCallback` | `Window::setKeyCallback()` |
| `glfwSetCharCallback` | `Window::setCharCallback()` |
| `glfwSetCharModsCallback` | `Window::setCharModsCallback()` |
| `glfwSetMouseButtonCallback` | `Window::setMouseButtonCallback()` |
| `glfwSetCursorPosCallback` | `Window::setCursorPosCallback()` |
| `glfwSetCursorEnterCallback` | `Window::setCursorEnterCallback()` |
| `glfwSetScrollCallback` | `Window::setScrollCallback()` |
| `glfwSetDropCallback` | `Window::setDropCallback()` |

## Joysticks und Gamepads

| GLFW 3.4 | sgl |
|---|---|
| `glfwJoystickPresent` | `sgl::IsJoystickPresent()` |
| `glfwGetJoystickAxes` | `sgl::GetJoystickAxes()` |
| `glfwGetJoystickButtons` | `sgl::GetJoystickButtons()` |
| `glfwGetJoystickHats` | `sgl::GetJoystickHats()` |
| `glfwGetJoystickName` | `sgl::GetJoystickName()` |
| `glfwGetJoystickGUID` | `sgl::GetJoystickGuid()` |
| `glfwSetJoystickUserPointer` | `sgl::SetJoystickUserPointer()` |
| `glfwGetJoystickUserPointer` | `sgl::GetJoystickUserPointer()` |
| `glfwJoystickIsGamepad` | `sgl::IsGamepad()` |
| `glfwGetGamepadName` | `sgl::GetGamepadName()` |
| `glfwGetGamepadState` | `sgl::GetGamepadState()` |
| `glfwUpdateGamepadMappings` | `sgl::UpdateGamepadMappings()` |
| `glfwSetJoystickCallback` | `sgl::SetJoystickCallback()` |

## Zwischenablage, Zeit, Kontext, Ereignisse

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetClipboardString` | `Window::getClipboardString()` |
| `glfwSetClipboardString` | `Window::setClipboardString()` |
| `glfwGetTime` | `sgl::GetTime()` |
| `glfwSetTime` | `sgl::SetTime()` |
| `glfwGetTimerValue` | `sgl::GetTimerValue()` |
| `glfwGetTimerFrequency` | `sgl::GetTimerFrequency()` |
| `glfwMakeContextCurrent` | `Window::makeContextCurrent()` |
| `glfwGetCurrentContext` | `sgl::GetCurrentWindow()` |
| `glfwSwapBuffers` | `Window::swapBuffers()` |
| `glfwSwapInterval` | `Window::setSwapInterval()` |
| `glfwExtensionSupported` | `Window::isExtensionSupported()` / `sgl::IsExtensionSupported()` |
| `glfwGetProcAddress` | `sgl::GetProcAddress()` |
| `glfwPollEvents` | `sgl::PollEvents()` |
| `glfwWaitEvents` | `sgl::WaitEvents()` |
| `glfwWaitEventsTimeout` | `sgl::WaitEventsTimeout()` |
| `glfwPostEmptyEvent` | `sgl::PostEmptyEvent()` |

## Native Zugriffe

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetX11Display` | `sgl::native::x11::GetDisplay()` |
| `glfwGetX11Window` | `sgl::native::x11::GetWindow()` |
| `glfwGetGLXContext` | `sgl::native::x11::GetGlxContext()` |
| `glfwGetGLXWindow` | `sgl::native::x11::GetGlxWindow()` |
| `glfwGetX11Monitor` | `sgl::native::x11::GetMonitor()` |
| `glfwGetWaylandDisplay` | `sgl::native::wayland::GetDisplay()` |
| `glfwGetWaylandWindow` | `sgl::native::wayland::GetSurface()` |
| `glfwGetEGLDisplay` | `sgl::native::wayland::GetEglDisplay()` |
| `glfwGetEGLSurface` | `sgl::native::wayland::GetEglSurface()` |
| `glfwGetEGLContext` | `sgl::native::wayland::GetEglContext()` |
| `glfwGetWin32Window` | `sgl::native::win32::GetHwnd()` |
| `glfwGetWGLContext` | `sgl::native::win32::GetHglrc()` |
| `glfwGetWin32Monitor` | `sgl::native::win32::GetMonitor()` |
| `glfwGetCocoaWindow` | `sgl::native::cocoa::GetNsWindow()` |
| `glfwGetNSGLContext` | `sgl::native::cocoa::GetNsglContext()` |
| `glfwGetCocoaMonitor` | `sgl::native::cocoa::GetNsScreen()` |

## Achsen-Konvention

- Joystick- und Gamepad-Y-Achsen: **negativ = oben, positiv = unten**
  (GLFW/SDL-Konvention, auf allen Plattformen gleich).
- X-Achsen: positiv nach rechts.
- Trigger: `-1 … +1`.

Die Reihenfolge der Rohachsen und Rohbuttons ist wie bei GLFW
plattformspezifisch. Der Gamepad-Zustand (`GamepadState`) ist dagegen
plattformübergreifend normiert.

Kalibrierung:

- **Linux:** `EVIOCGABS` liefert `minimum`, `maximum`, `fuzz` und `flat`;
  normalisiert wird über `minimum`/`maximum`.
- **macOS:** logische Min/Max plus Auto-Kalibrierung anhand beobachteter Werte
  (wie GLFW).
- **Windows/XInput:** feste Bereiche und Deadzones des XInput-Treibers.

## Bekannte Einschränkungen

- macOS: maximal OpenGL 4.1; ein 4.6-Kontext schlägt fehl.
- Wayland: Pointer-Lock, Rohmausbewegung und `setPosition` sind
  protokollbedingt nicht verfügbar.
- X11: GLX und EGL werden unterstützt. Win32: bisher nur WGL (kein ANGLE/EGL).
- Windows/macOS: code-complete, in dieser Umgebung nicht kompilier- oder
  testbar.
- OSMesa: nur wenn `libosmesa` installiert ist.
- Gamepad-Mapping: Die SDL_GameControllerDB ist eingebettet (zlib). Geräte
  ohne passenden Eintrag gelten nicht als Gamepad. Eigene Mappings können
  über `sgl::UpdateGamepadMappings()` ergänzt werden.
