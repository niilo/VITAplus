# 17: A controller port is reserved before the open and never returned

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/ctrl/src/ctrl.cpp:85-96`:

```cpp
new_controller.port = reserve_port(state);
if (new_controller.port == -1) { // Port not available
    break;
}
const GamepadPtr controller(SDL_OpenGamepad(gamepad_id), SDL_CloseGamepad);
if (controller == nullptr) {
    continue;
}
auto controller_name = SDL_GetGamepadName(controller.get());
if (controller_name == nullptr) {
```

`reserve_port` marks the port used (`ctrl.cpp:31`). If `SDL_OpenGamepad` returns
null, or the gamepad has no name, the `continue` leaves the port marked used
with no controller in `state.controllers`. Nothing returns it: the removal loop
at `ctrl.cpp:62` only releases ports of controllers that were added.

So each such gamepad permanently consumes one of the four ports. There are only
four. After four failures no controller works for the rest of the session, with
no message.

Android reports several devices like this. The `uinput-` and `gf_` names are
filtered at `ctrl.cpp:80`, but other unnamed virtual gamepads are not.

Vita3K-Plus commit `ad93d9c7`.

## Plan

1. Move `reserve_port` to after the open succeeds and the name is accepted, so
   a failure returns the port.
2. Move the Android name filter ahead of the open, as the reference does, so
   the device is rejected before it can consume anything.
3. Add a per-device log line. Without it this failure is invisible: the game
   reports no controller and the emulator reports none either.
4. Test by attaching several unnamed gamepads, which Android can do with
   `tools/android/device.sh`.

## Comments