# PIF Manual

The manual is built with [Doxygen](https://www.doxygen.nl) from two sources:

- `docs/manual/`: hand-written pages (Markdown `.md` or Doxygen `.dox`).
- `include/`, `gpl/include/`: the API reference, from the comments in the headers.

## Building

Install Doxygen (1.9.6 or later), then run from anywhere in the repository:

```bash
tools/gen_docs.sh          # build docs/build/html
tools/gen_docs.sh --open   # build and open it in the browser
```

On Windows, from cmd.exe or PowerShell:

```bat
tools\gen_docs.cmd
tools\gen_docs.cmd --open
```

Set `DOXYGEN=/path/to/doxygen` when it is not on the PATH. `gen_docs.cmd` also looks in
`%ProgramFiles%\doxygen\bin`, where the Windows installer puts it. Warnings go to `docs/build/warnings.log`.
`docs/build/` is ignored by git.

## Publishing

The manual is not committed. `.github/workflows/docs.yml` builds it with Doxygen 1.18.0 on every push to
`main` and publishes it at https://slowlybarefoot.github.io/pif/. A pull request only builds it, and the
warnings show up as annotations of the run. In the repository settings, Pages > Source must be set to
"GitHub Actions" once.

## Layout

| Path | Contents |
| --- | --- |
| `docs/Doxyfile` | Doxygen settings (only those that differ from the defaults) |
| `docs/manual/mainpage.md` | Front page and table of contents |
| `docs/manual/*.md` | Manual pages |
| `docs/manual/images/` | Images for `![...](name.png)` or `@image` |
| `docs/manual/examples/` | Code for `@include` and `@snippet` (the `test/` tree is searched too) |

## Adding a page

1. Create `docs/manual/<name>.md` and give it a page id on its title line:

   ```markdown
   Ring Buffer {#ring_buffer}
   ===========

   @tableofcontents

   ## Overview
   ...
   ```

2. Link it from its parent page with `@subpage ring_buffer`, in `mainpage.md` or in another page. A page
   that no page links with `@subpage` is listed at the top level.

A page that describes what a module parses, such as a language or a template syntax, is written in the
header of that module instead, as a `/** @page id Title ... */` block, so it is changed together with the
code. The BASIC language (`basic_language` in interpreter/pif_basic.h) and the SRML syntax (`srml_syntax`
in markup/pif_srml.h) are done this way.

Inside a page:

- A function name followed by `()`, such as `pifRingBuffer_PutData()`, links to its reference. So does a type
  name such as `PifRingBuffer`.
- `@ref ring_buffer "text"` links to another page, `@ref ring_buffer_overview` to a heading with
  `{#ring_buffer_overview}`.
- `@code{.c}` ... `@endcode` shows code. `@snippet test/core/test_pif_ring_buffer.c name` shows the lines
  between two `//! [name]` markers in that file.
- `@note`, `@warning` and `@see` make a highlighted paragraph.

## Commenting the headers

A function is documented by the block right above its declaration:

```c
/**
 * @brief Sets up a ring buffer object of the caller's with a buffer of size bytes from the heap.
 *
 * Details, when the brief is not enough.
 *
 * @param p_owner Ring buffer to set up.
 * @param size Size of the buffer in bytes. One byte less can be held.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size);
```

Do not add `@fn name`: Doxygen takes the block for a function of that exact signature, and a bare name
matches none, so the whole block is dropped. The block above the declaration needs no `@fn`.

Other conventions:

- Every header starts with a `@file` block after the SPDX line, which describes the module on its file page.
- Structures use `@class StPifXxx` (or `@struct`). Enums and callback typedefs get a `@brief` block above them.
- A field, an enum value or a macro gets a trailing `///<` comment. A plain `//` comment is not picked up.
- A run of related macros, such as the fields of a register, is put in a `/** @name Title ... @{ */` and
  `/** @} */` group with one description for all of them.
- `__name` fields are private and left out of the manual (`EXCLUDE_SYMBOLS` in the Doxyfile).
- Write what the function does and what its arguments mean to the caller, not "Pointer to the instance"
  style placeholders.

Run `tools/gen_docs.sh` after a change: the warnings in `docs/build/warnings.log` point at a `@param`
that does not match its function, an undocumented parameter or a broken reference.
