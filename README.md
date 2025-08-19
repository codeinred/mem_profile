## Project Layout

This project is split into a number of different components.

Plugin code:

- `mp::ast` - code for interacting with Clang's AST. Used to implement the Clang
  Plugin that annotates the stack with information about the object currently
  being destroyed.
- `mp::hook_prelude` - contains header file that should be included in all
  translation units compiled with the Clang Plugin.

Code for runtime module:

- `mp::unwind` - module containing a basic unwinding API
- `mp::runtime` - module which overrides `malloc`, `free`, and other allocation
  functions, in order to track allocations at runtime.

Utility Modules:

- `mp::types` - utility module containing type definitions shared by all modules
- `mp::core` - utility module for other core bits that are shared between
  modules
- `mp::error` - utility module containing error handling code + `strerror`
  functionality
- `mp::fs` - utility module for filesystem stuff

## Project TODO list

- [ ] add object file information, object address, etc into `malloc_stats.json`.
      This information is pretty useful.

  It will also be really useful in debugging why some dtors aren't annotated.

- [ ] See if it's possible to store the mangled symbol names, in addition to the
      demangled versions
- [ ] Fix type names - right now type names are provided as
      `class std::unique_ptr<char[]>` or `struct test::my_object`. Update clang
      plugin code to _not_ onnotate with `class` or `struct` before typenames
- [ ] Gracefully handle missing destructor annotations. Some destructors aren't
      annotated - I think this occurs because an inlined version is already
      present, or because the destructor definition is provided hard-coded by
      the standard library. We should have some notion of "pool of objects that
      can't be individually distinguished". Eg, "this vector owns a bunch of
      strings, but we don't know how many, because the string dtor was not
      properly annotated."

  We can do this by scanning the stack to look for destructor calls that didn't
  get annotated. Any function containing `::~` when demangled is probably a
  destructor call

- [ ] Clean up python module code + make printing less ugly
- [ ] Add facilities for pretty-printing traces after the fact (from
      `malloc_stats.json`)
- [ ] Print information about _layout_ of objects - eg, in addition to just
      displaying children, print out information about where the member is in
      the class (member offset, etc)
- [ ] Right now we're dealing with individual objects. We should figure out how
  to nicely aggregate statistics for display
