# Squashfs-tools-ng Software Architecture

Generally speaking, the package tries to somewhat imitate a typical Unix
filesystem structure.

The source code for an executable program is located in `bin/<program-name>/`,
while the source code for a library is located in `lib/<library-name>/`,
without the typical `lib` prefix.

Shared header files for the libraries are in `include/`. So far, a header
sub-directory is only used for `libsquashfs`, since those headers are somewhat
more numerous and are installed on the system in the same sub-directory.

If a binray program comes with a man page, the man page is located at the same
location as the program source (i.e. `bin/<program-name>/<program-name>.1`).

Extra documentation (like this file) is located in the `doc` directory, and
source code for example programs which are not installed is in `extras`.

Unit tests for the libraries are in `tests/<library-name>`, with a `lib` prefix
and tests for programs are in `tests/<program-name>`.

## Library Stacking

To achieve loose coupling, core functionality is implemented by libraries in a
reasonably generic way, and may in-turn make use of other libraries to implement
their functionality.

To the extent possible, the actual application programs are merely frontends
for the underlying libraries.

The following diagram tries to illustrate how the libraries are stacked:

     _______________________________________
    |                                       |
    |         Application Programs          |
    |_______________________________________|
                ____________________________
               |                            |
               |         libcommon          |
     __________|____________________________|
    |          |              |             |
    |  libtar  |  libfstree   |             |
    |__________|_______       |   libsqfs   |
    |                  |      |             |
    |    libfstream    |      |             |
    |__________________|______|_____________|
    |                         |             |
    |        libcompat        |   libutil   |
    |_________________________|_____________|


At the bottom, `libutil` contains common helper functions and container
data structures (dynamic array, hash table, rb-tree, et cetera) used by
both `libsqfs` and the application programs.

The `libcompat` library contains fallback implementations for OS library
functions that might not be available everywhere (e.g. POSIX stuff missing
on Windows or GNU stuff missing on BSD).

The `libfstream` library implements stream based I/O abstraction, i.e. it has
an abstract data structure for a non-seek-able read-only input streams and
write-only output streams. It has concrete implementations wrapping the
underlying OS functionality, as well as stream-compressor based implementations
that wrap an existing interface instance.

On top of `libfstream`, the `libtar` library is implemented. It supports
simple reading & decoding of tar headers from an input stream, as well as
generating and writing tar headers to an output stream and supports various
extensions (e.g. GNU, SCHILY). Thanks to the `libfstream` compressor wrappers,
it supports transparent compression/decompression of tar headers and data.

The `libfstree` library contains functionality related to managing a
and manipulating a hierarchical filesystem tree. It can directly parse the
description format for `gensquashfs` or scan a directory.

The `libsqfs` (actually `libsquashfs`) library implements the bulk of the
squashfs reading/writing functionality. It is built as a shared library and
is installed on the target system along with the application programs and a
bunch of public headers.

Finally, `libcommon` contains miscellaneous stuff shared between the
application programs, such as common CLI handling code, some higher level
utilities, higher level wrappers around `libsqfs` for some tool specific
tasks.

### Licensing Implications

The application programs and the static libraries are GPL licensed,
while `libsquashfs` is licensed under the LGPL. Because the code
of `libutil` is compiled into `libsquashfs`, it also needs to be under
the LGPL and only contain 3rd party code under a compatible license.

Furthermore, since the LZO compressor library is GPL licensed, `libsquashfs`
cannot use it directly and thus does not support LZO compression. Instead,
the `libcommon` library contains an implementation of the `libsquashfs`
compressor interface that uses the LZO library, so the application
programs *do support* LZO, but the library doesn't.


### Managing Symbols and Visiblity

All symbols exported from `libsquashfs` must start with a `sqfs_` prefix.
Likewise, all data structures and typedefs in the public header use this prefix
and macros use an `SQFS_` prefix, in order to prevent namespace pollution.

The `sqfs/predef.h` header contains a macro called `SQFS_API` for marking
exported symbols. Whether the symbols are imported or exported, depends on
the presence of the `SQFS_BUILDING_DLL` macro.

To mark symbols as explicitly not exported (required on some platforms), the
macro `SQFS_INTERNAL` is used (e.g. on all `libutil` functions to keep
the internal).

An additional `SQFS_INLINE` macro is provided for inline functions declared
in headers.

The public headers of `libsquashfs` also must not include any headers of the
other libraries, as they are not installed on the target system.

However, somewhat contradictory to the diagtram, a number of the libraries
outlined above need declarations, typedefs and macros from `sqfs/predef.h`
and simply include thta header.


## Object Oriented Design

Anybody who has done C programming to a reasonable degree should be familiar
with the technique. An interface is basically a `struct` with function pointers
where the first argument is a pointer to the instance (`this` pointer).

Single inheritance basically means making the base struct the first member of
the extended struct. A pointer to the extended object can be down cast to a
pointer to the base struct and used as such.

To the extent possible, concrete implementations are made completely opaque and
only have a factory function to instantiate them, for a more loose coupling.

The `sqfs/predef.h` defines and typedefs a `sqfs_object_t` structure, which
is at the bottom of the inheritance hierarchy.

It contains two function pointers `delete` and `copy`. The former destroys and
frees the object itself, the later creates an exact copy of the object.
The `copy` callbacks may be `NULL`, if creating copies is not applicable for a
particular type of object.

For convenience, two inline helpers `sqfs_destroy` and `sqfs_copy` are provided
that cast a `void` pointer into an object pointer and call the respecive
callbacks. The later also checks if the callback is `NULL`.


## The libsquashfs malloc/free Issue

While most code in `libsquashfs` works with objects that have a `destroy` hook,
some functions return pointers to data blobs or dumb structures that have been
allocated with `malloc` and expect the caller to free them again.

This turned out to be a design issue, since the shared library could in theory
end up being linked against a different C runtime then an application using it.
On Unix like systems this would require a rather freakish circumstances, but
on Windows this actually happens fairly easily.

As a result, a `sqfs_free` function was added to `libsquashfs` to expose access
to the `free` function of the libraries run-time. All new code
using `libsquashfs` should use that function, but to maintain backward
compatibility with existing code, the library has to continue using regular
malloc at those places, so programs that currently work with a simple `free`
also continue to work.
