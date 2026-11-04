# VoyaPS4 capability map

| Module | Responsibility | Depends on |
|---|---|---|
| transfer | HTTP(S), validated byte ranges, durable partial files, hashes | — |
| library | Catalog validation, persistent queue, retries and pause | transfer |
| console | DualShock UI, URL entry, local package installation | library |
| releases | Pinned package builds, version checks, verified update downloads | transfer, console |

Build order: transfer → library → console → releases.
The user authorized building and publishing the app in this chat. Console firmware is 13.52;
the loader and hardware compatibility still need console verification.
