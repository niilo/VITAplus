# 12: sceAppUtilSystemParamGetString ignores bufSize

Status: open
Type: task
Label: ready-for-agent

## Context

`vita3k/modules/SceAppUtil/SceAppUtil.cpp:543`:

```cpp
EXPORT(int, sceAppUtilSystemParamGetString, unsigned int paramId, SceChar8 *buf, SceSize bufSize) {
    TRACY_FUNC(sceAppUtilSystemParamGetString, paramId, buf, bufSize);
    constexpr auto devname_len = SCE_SYSTEM_PARAM_USERNAME_MAXSIZE;
    char devname[devname_len];
    switch (paramId) {
    case SCE_SYSTEM_PARAM_ID_USER_NAME:
        if (gethostname(devname, devname_len)) {
            std::strncpy(devname, emuenv.io.user_name.c_str(), sizeof(devname));
        }
        std::strncpy(reinterpret_cast<char *>(buf), devname, sizeof(devname));
        break;
```

`bufSize` is never read. `sizeof(devname)` is `SCE_SYSTEM_PARAM_USERNAME_MAXSIZE`,
which is the size of the emulated device name, not the size of the guest's
buffer. A title that asks for a short name with a small `bufSize` gets
`SCE_SYSTEM_PARAM_USERNAME_MAXSIZE` bytes written into it.

`buf` is also not null-checked, and the `default:` arm returns
`SCE_APPUTIL_ERROR_PARAMETER` for every other `paramId`, so there is no second
case to compare against.

This is a guest memory overflow. The amount written past the caller's buffer
is small on a device with a short hostname and equal to the buffer size on one
with a long one.

EmuCoreV commit `82de10e5` change 6 bounds the copy with
`std::min(bufSize, SCE_SYSTEM_PARAM_USERNAME_MAXSIZE)`, null-checks both
arguments, and adds a last-resort fallback string so an unmapped hostname does
not produce an empty result.

## Plan

1. Bound the copy with `std::min(bufSize, devname_len - 1)` and terminate
   inside the bound.
2. Return `SCE_APPUTIL_ERROR_PARAMETER` when `buf` is null or `bufSize` is 0.
   Check `emuenv/include/emuenv/app_util.h` for the error code the file already
   uses.
3. Add a fallback for the case where `gethostname` fails and the configured
   user name is empty, so the call returns a usable string.
4. Read the whole file and check every other export that takes a buffer and a
   size for the same omission. `sceAppUtilSaveDataDataRemove` and
   `sceAppUtilSaveDataDataSave` at `:249` take an unvalidated `slot->id`, which
   is a second guest-input path into a host path; report it as a comment here
   rather than fixing it here.

## Comments