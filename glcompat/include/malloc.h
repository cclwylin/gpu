// Linux-only malloc.h shim (glcompat). On macOS only stdlib.h has
// malloc/free; tests/examples/glpuzzle.c says `#include <malloc.h>`,
// our include path picks this up first.
#pragma once
#include <stdlib.h>
