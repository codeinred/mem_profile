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
