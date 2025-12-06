# Build Report — Workspace (generated)

Date: 2025-12-06
OS: macOS

Overview
--------
All projects in the workspace were built with PlatformIO and static checks were run. The builds succeeded for every station after a few targeted fixes.

Build summary (firmware size and RAM usage as reported by PlatformIO)
----------------------------------------------------------------------
- Station1-Core2: Flash ~966,809 bytes (30.7%), RAM ~48,644 bytes (1.1%)
- Station1-StickC: Flash ~915,869 bytes (46.6%), RAM ~47,132 bytes (14.4%)
- Station2-Echo: Flash ~909,681 bytes (46.3%), RAM ~47,104 bytes (14.4%)
- Station2-Matrix: Flash ~887,369 bytes (45.1%), RAM ~47,120 bytes (14.4%)
- Station2-Core2: Flash ~885,793 bytes (28.2%), RAM ~46,504 bytes (1.0%)
- Station3-Paper: Flash ~887,581 bytes (28.2%), RAM ~47,144 bytes (14.4%)
- Station4-Paper: Flash ~888,365 bytes (28.2%), RAM ~47,192 bytes (14.4%)
- Station4-Matrix: Flash ~885,645 bytes (45.0%), RAM ~47,072 bytes (14.4%)
- Reset-Basic: built earlier with no reported errors

Static analysis
---------------
PlatformIO "platformio check" (cppcheck) was run for the projects. No cppcheck warnings/errors were reported in the collected outputs.

Source edits made
-----------------
During the build-and-fix iteration the following changes were applied to fix compile-time errors detected during builds:

1) Station2-Matrix/include/ShowcaseProtocol.h
   - Added protocol struct and helpers: ShowcaseMessage struct, calculateChecksum/verifyChecksum/setChecksum, and createMessage helper. This resolved missing symbol errors (createMessage, verifyChecksum).

2) Station3-Paper/src/main.cpp
   - Fixed M5.Touch API usage: replaced t.isPressed with t.isPressed() to call the method.

3) Station4-Paper/src/main.cpp
   - Same M5.Touch API fix: replaced t.isPressed with t.isPressed().

Next actions available
----------------------
- Archive firmware binaries for each project into a deliverables folder.
- Prepare a git patch/PR with the edits.
- Add runtime simulation tests that call onDataRecv handlers with crafted messages to exercise flows locally.
- Flash selected firmwares to hardware (requires devices connected).

Tell me which to perform next, or say "do all" to proceed with archiving artifacts, creating a patch, and adding a simulation harness.