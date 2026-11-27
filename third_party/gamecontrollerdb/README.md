# SDL_GameControllerDB

Vendored snapshot of the community game controller mapping database.

- Upstream: https://github.com/mdqinc/SDL_GameControllerDB
- Commit: `01af257a606ebec9b932f274129c383873bbda67`
- Date: 2026-11-27
- License: zlib (see `LICENSE.txt`)
- File: `gamecontrollerdb.txt`

`gamecontrollerdb.txt` is the source for the embedded table in
`src/common/GamepadMappingsGenerated.cpp`. The generated file is committed so
that building `sgl` does not require Python or network access.

To update the database, replace `gamecontrollerdb.txt`, update the commit
above, and regenerate the embedded C++ string literal.
