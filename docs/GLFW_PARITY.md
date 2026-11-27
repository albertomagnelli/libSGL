# GLFW-3.4-Paritätsaudit

Ziel: alle **118 Nicht-Vulkan-Funktionen** von GLFW 3.4 haben eine Entsprechung
in `sgl`. Die sechs Vulkan-Funktionen (`glfwVulkanSupported`,
`glfwGetRequiredInstanceExtensions`, `glfwGetInstanceProcAddress`,
`glfwGetPhysicalDevicePresentationSupport`, `glfwCreateWindowSurface`,
`glfwInitVulkanLoader`) sind bewusst nicht enthalten.

## Initialisierung, Fehler, Plattform

| GLFW 3.4 | sgl |
|---|---|
| `glfwInit` | `sgl::init()` |
| `glfwTerminate` | `sgl::terminate()` |
| `glfwInitHint` | `sgl::initHint()` |
| `glfwInitAllocator` | `sgl::InitOptions::allocator` |
| `glfwGetVersion` | `sgl::getVersion()` |
| `glfwGetVersionString` | `sgl::getVersionString()` |
| `glfwGetError` | `sgl::getError()` |
| `glfwSetErrorCallback` | `sgl::setErrorCallback()` |
| `glfwGetPlatform` | `sgl::getPlatform()` |
| `glfwPlatformSupported` | `sgl::isPlatformSupported()` |

## Monitore

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetMonitors` | `sgl::getMonitors()` |
| `glfwGetPrimaryMonitor` | `sgl::getPrimaryMonitor()` |
| `glfwGetMonitorPos` | `Monitor::getPosition()` |
| `glfwGetMonitorWorkarea` | `Monitor::getWorkarea()` |
| `glfwGetMonitorPhysicalSize` | `Monitor::getPhysicalSize()` |
| `glfwGetMonitorContentScale` | `Monitor::getContentScale()` |
| `glfwGetMonitorName` | `Monitor::getName()` |
| `glfwSetMonitorUserPointer` | `Monitor::setUserPointer()` |
| `glfwGetMonitorUserPointer` | `Monitor::getUserPointer()` |
| `glfwSetMonitorCallback` | `sgl::setMonitorCallback()` |
| `glfwGetVideoModes` | `Monitor::getVideoModes()` |
| `glfwGetVideoMode` | `Monitor::getVideoMode()` |
| `glfwSetGamma` | `Monitor::setGamma()` |
| `glfwGetGammaRamp` | `Monitor::getGammaRamp()` |
| `glfwSetGammaRamp` | `Monitor::setGammaRamp()` |

## Fenster

| GLFW 3.4 | sgl |
|---|---|
| `glfwDefaultWindowHints` | `sgl::getDefaultWindowHints()` |
| `glfwWindowHint` | `WindowHints`-Feld |
| `glfwWindowHintString` | `WindowHints`-Stringfeld |
| `glfwCreateWindow` | `Window::create()` |
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
| `glfwRawMouseMotionSupported` | `Window::isRawMouseMotionSupported()` / `sgl::isRawMouseMotionSupported()` |
| `glfwGetKeyName` | `Window::getKeyName()` / `sgl::getKeyName()` |
| `glfwGetKeyScancode` | `Window::getKeyScancode()` / `sgl::getKeyScancode()` |
| `glfwGetKey` | `Window::isKeyPressed()` |
| `glfwGetMouseButton` | `Window::isMouseButtonPressed()` |
| `glfwGetCursorPos` | `Window::getCursorPos()` |
| `glfwSetCursorPos` | `Window::setCursorPos()` |
| `glfwCreateCursor` | `Cursor::create()` |
| `glfwCreateStandardCursor` | `Cursor::createStandard()` |
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
| `glfwJoystickPresent` | `sgl::isJoystickPresent()` |
| `glfwGetJoystickAxes` | `sgl::getJoystickAxes()` |
| `glfwGetJoystickButtons` | `sgl::getJoystickButtons()` |
| `glfwGetJoystickHats` | `sgl::getJoystickHats()` |
| `glfwGetJoystickName` | `sgl::getJoystickName()` |
| `glfwGetJoystickGUID` | `sgl::getJoystickGUID()` |
| `glfwSetJoystickUserPointer` | `sgl::setJoystickUserPointer()` |
| `glfwGetJoystickUserPointer` | `sgl::getJoystickUserPointer()` |
| `glfwJoystickIsGamepad` | `sgl::isGamepad()` |
| `glfwGetGamepadName` | `sgl::getGamepadName()` |
| `glfwGetGamepadState` | `sgl::getGamepadState()` |
| `glfwUpdateGamepadMappings` | `sgl::updateGamepadMappings()` |
| `glfwSetJoystickCallback` | `sgl::setJoystickCallback()` |

## Zwischenablage, Zeit, Kontext, Ereignisse

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetClipboardString` | `Window::getClipboardString()` |
| `glfwSetClipboardString` | `Window::setClipboardString()` |
| `glfwGetTime` | `sgl::getTime()` |
| `glfwSetTime` | `sgl::setTime()` |
| `glfwGetTimerValue` | `sgl::getTimerValue()` |
| `glfwGetTimerFrequency` | `sgl::getTimerFrequency()` |
| `glfwMakeContextCurrent` | `Window::makeContextCurrent()` |
| `glfwGetCurrentContext` | `sgl::getCurrentWindow()` |
| `glfwSwapBuffers` | `Window::swapBuffers()` |
| `glfwSwapInterval` | `Window::setSwapInterval()` |
| `glfwExtensionSupported` | `Window::isExtensionSupported()` / `sgl::isExtensionSupported()` |
| `glfwGetProcAddress` | `sgl::getProcAddress()` |
| `glfwPollEvents` | `sgl::pollEvents()` |
| `glfwWaitEvents` | `sgl::waitEvents()` |
| `glfwWaitEventsTimeout` | `sgl::waitEventsTimeout()` |
| `glfwPostEmptyEvent` | `sgl::postEmptyEvent()` |

## Native Zugriffe

| GLFW 3.4 | sgl |
|---|---|
| `glfwGetX11Display` | `sgl::native::x11::getDisplay()` |
| `glfwGetX11Window` | `sgl::native::x11::getWindow()` |
| `glfwGetGLXContext` | `sgl::native::x11::getGLXContext()` |
| `glfwGetGLXWindow` | `sgl::native::x11::getGLXWindow()` |
| `glfwGetX11Monitor` | `sgl::native::x11::getMonitor()` |
| `glfwGetWaylandDisplay` | `sgl::native::wayland::getDisplay()` |
| `glfwGetWaylandWindow` | `sgl::native::wayland::getSurface()` |
| `glfwGetEGLDisplay` | `sgl::native::wayland::getEGLDisplay()` |
| `glfwGetEGLSurface` | `sgl::native::wayland::getEGLSurface()` |
| `glfwGetEGLContext` | `sgl::native::wayland::getEGLContext()` |
| `glfwGetWin32Window` | `sgl::native::win32::getHWND()` |
| `glfwGetWGLContext` | `sgl::native::win32::getHGLRC()` |
| `glfwGetWin32Monitor` | `sgl::native::win32::getMonitor()` |
| `glfwGetCocoaWindow` | `sgl::native::cocoa::getNSWindow()` |
| `glfwGetNSGLContext` | `sgl::native::cocoa::getNSGLContext()` |
| `glfwGetCocoaMonitor` | `sgl::native::cocoa::getNSScreen()` |

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
  über `sgl::updateGamepadMappings()` ergänzt werden.
