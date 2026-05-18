# Notepad — Implementation Notes

## Overview

This project extends the base Notepad application (built across Practices 5–8) into a
richer WordPad-like editor. The application is built with C++20 and Qt 6 Widgets.

---

## Required Features

### 1. Exception Handling (`notepad_exception.h`)

A small exception hierarchy rooted at `notepad_exception` (itself a subclass of
`std::runtime_error`) is defined in `notepad_exception.h`:

| Class | Thrown when |
|---|---|
| `notepad_exception` | Base class; never thrown directly |
| `file_not_found_exception` | `QFile::exists()` returns `false` before opening |
| `file_read_exception` | `QFile::open()` fails in read mode, or `QTextStream::status()` is not `Ok` |
| `file_write_exception` | `QFile::open()` fails in write mode |

`open_file()` and `save_file()` in `main_window.cpp` each wrap their Qt file I/O in a
`try / catch (const notepad_exception&)` block and surface errors via
`QMessageBox::critical(this, "Error", e.what())`.

A standalone test program (`test_exceptions.cpp`, built as the `test_exceptions` target
in CMake) verifies single catch, multiple-catch ordering, and rethrowing.

---

### 2. Spell Checker (`spell_checker.h`, `spell_checker_highlighter.h`)

#### Dictionary loading (`spell_checker`)

At startup `main_window::setup_spell_checker()` constructs a `spell_checker` with the
path `data/words.txt` (370 105 lowercase ASCII entries from
[dwyl/english-words](https://github.com/dwyl/english-words), public domain). The file is
read line-by-line into a `std::set<std::string>`. Windows-style `\r` endings are stripped
before insertion.

#### Correctness check

`spell_checker::is_correct(const QString&)` lowercases the word and strips all
non-ASCII letters via the private `clean()` helper, then does a `set::count` lookup.
Single-character tokens and any word that survives cleaning as empty are always accepted.
If the dictionary failed to load the method returns `true` (silent no-op mode).

#### Real-time highlighting (`spell_checker_highlighter`)

`spell_checker_highlighter` subclasses `QSyntaxHighlighter` and overrides
`highlightBlock()`. It scans each block character-by-character, collecting letter runs
(apostrophes mid-word are included). For each collected word it calls `is_correct()`; on
failure it reads the existing `QTextCharFormat` for that range (preserving bold, italic,
etc.) and overlays `SpellCheckUnderline` in red (`#FF3232`). This means rich-text
formatting applied by the user is never destroyed by the highlighter.

#### Spelling suggestions

`spell_checker::suggestions()` iterates the dictionary, skips entries whose length
differs from the target by more than 2, and computes the classic two-row Levenshtein
distance for the rest. Candidates with distance ≤ 2 are collected, sorted by distance,
and the top 5 are returned as a `QVector<QString>`.

#### Context menu

The editor's context-menu policy is set to `Qt::CustomContextMenu`.
`main_window::show_suggestions_menu()` finds the word under the cursor with
`QTextCursor::WordUnderCursor`, checks it, and if misspelled prepends up to 5 suggestion
actions to the standard context menu. Clicking a suggestion calls `cursor.insertText(s)`.

#### Manual re-check

`Tools > Check Spelling...` calls `highlighter->rehighlight()` to force a full
document pass.

---

## Optional Features

### Optional 1 — Cursor Line / Column Indicator

**Where:** status bar (permanent right-side widget, `status_cursor`).

`main_window::setup_status_bar()` connects `QTextEdit::cursorPositionChanged` to
`update_cursor_position()`, which reads `QTextCursor::blockNumber()` (+1) and
`columnNumber()` (+1) and formats them as `Ln N, Col M`. The label sits in
`statusBar()->addPermanentWidget()` so it is always right-aligned and never pushed out
by temporary messages.

---

### Optional 2 — Font Dialog

**Where:** `Format > Font...` menu item.

`main_window::show_font_dialog()` opens `QFontDialog::getFont()` pre-populated with the
editor's current font. If the user confirms:

- **With a selection** — a `QTextCharFormat` carrying only the new font is merged into
  the selection via `QTextCursor::mergeCharFormat()`, leaving all other character
  properties (color, bold, underline) untouched.
- **Without a selection** — `QTextEdit::setFont()` replaces the document-wide default
  font. The `line_number_editor` catches the resulting `QEvent::FontChange` in
  `changeEvent()` and recalculates the line-number margin width automatically.

---

### Optional 3 — Line Numbers

**Where:** left margin of the editor; implemented in `line_number_area.h`.

Two classes cooperate:

- **`line_number_editor`** subclasses `QTextEdit`. It owns a `line_number_area` widget
  and maintains a viewport left-margin equal to `line_number_area_width()`. Width is
  recomputed whenever the block count or font changes (the latter matters for zoom). The
  `reposition_number_area()` helper is called from both `resizeEvent()` and
  `update_left_margin()` to keep the overlay widget flush with the editor's left edge.

- **`line_number_area`** is a plain `QWidget` that delegates all painting to
  `line_number_editor::line_number_area_paint()`. The paint method iterates
  `QTextDocument` blocks, maps each block's bounding rectangle through the vertical
  scroll offset to screen coordinates, and draws the 1-based line number right-aligned
  in the margin. The current line's number is drawn in `QPalette::Text` (full contrast);
  all others use `QPalette::PlaceholderText` (dimmed), giving a visual cue for the
  cursor position.

---

### Optional 4 — Zoom

**Where:** `View > Zoom In / Zoom Out / Reset Zoom` and keyboard shortcuts.

All font-size changes funnel through `main_window::set_font_size(int pt)`, which clamps
the value to `[6, 72]` before calling `QTextEdit::setFont()`. Step size is 2 pt.

Because `QAction` shortcuts may not fire when a child widget (the editor) holds focus,
dedicated `QShortcut` objects with `Qt::ApplicationShortcut` context are registered on
the main window for all zoom keys:

| Action | Keys |
|---|---|
| Zoom In | `Ctrl++`, `Ctrl+=` |
| Zoom Out | `Ctrl+-`, `Ctrl+_` |
| Reset Zoom | `Ctrl+0`, `Ctrl+Shift+0` |

`set_font_size()` updates the editor font; `line_number_editor::changeEvent()` responds
to the resulting `FontChange` event and recalculates the line-number margin so the
numbers always match the editor text size.

---

## Other Pre-existing Features

| Feature | Location |
|---|---|
| File operations (New / Open / Save / Save As) | `main_window.cpp` — `setup_file_menu()` |
| Edit menu (Undo / Redo / Cut / Copy / Paste / Select All) | `setup_edit_menu()` |
| Text case transforms (5 variants) | `text_transform.h`, `setup_format_menu()` |
| Rich-text formatting toolbar (Bold / Italic / Underline) | `setup_format_toolbar()` |
| Find / Replace dialog | `find_replace_dialog.ui`, `show_find_replace_dialog()` |
| Word frequency analysis | `word_frequency_dialog.ui`, `show_word_frequency()` |
| Status bar word / line counts | `setup_status_bar()` |
| Merge sort (`merge_sort`) and insertion sort (`insertion_sort`) | `sort.h` |

---

## Build Instructions

```bash
cmake -S . -B build
cmake --build build
```

Set the run configuration's **Working Directory** to `$ProjectFileDir$` in CLion so that
`data/images/*.svg` and `data/words.txt` resolve correctly at runtime.