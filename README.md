# mni: a Nuru interpreter in C++

[![CI](https://github.com/ms3c/mni/actions/workflows/ci.yml/badge.svg)](https://github.com/ms3c/mni/actions/workflows/ci.yml)

`mni` is an interpreter for [Nuru](https://github.com/NuruProgramming/Nuru), the programming language with Swahili keywords. It is a tree-walking interpreter written in C++20: a lexer, a Pratt parser, and an evaluator, with the standalone executable `nuru`.

> **Status:** work in progress. The language core, packages and imports, and the `hisabati`, `jsoni`, `muda`, `mtandao` (HTTP) and `os` modules work. See [What's missing](#whats-missing).

```nuru
fanya salamu = unda(jina, lugha = "sw") {
    kama (lugha == "sw") {
        rudisha "Habari, " + jina + "!"
    }
    rudisha "Hello, " + jina + "!"
}

kwa i, jina ktk ["Juma", "Asha"] {
    andika(i, salamu(jina))
}
```

```
0 Habari, Juma!
1 Habari, Asha!
```

`mni` implements the language as described in Nuru's [documentation](https://nuruprogramming.org). It is not affiliated with the Nuru project.

## Build

You need CMake 3.24 or newer and a C++20 compiler. Dependencies are downloaded by CMake on first configure, so the first build needs network access: [fmt](https://github.com/fmtlib/fmt), [nlohmann/json](https://github.com/nlohmann/json), [cpr](https://github.com/libcpr/cpr) (HTTP) and, for the tests, [doctest](https://github.com/doctest/doctest).

cpr uses the system libcurl on Linux and macOS (macOS ships it; on Debian and Ubuntu install `libcurl4-openssl-dev`) and builds curl itself on Windows.

```sh
cmake -Sstandalone -Bbuild/standalone -DCMAKE_BUILD_TYPE=Release
cmake --build build/standalone
./build/standalone/nuru --toleo
```

Without `-DCMAKE_BUILD_TYPE=Release` the build is unoptimized and noticeably slower.

Pass `-DMNI_ENABLE_HTTP=OFF` to build without the `mtandao` module and without libcurl and cpr. Installing the library (`cmake --install`) currently requires this, because the installed package cannot carry cpr; with HTTP on, install stops with an explanatory error.

## Usage

```
nuru                     start the REPL
nuru file.nr             run a file (.nr or .sw)
nuru --ast file.nr       print the parsed program instead of running it
nuru --tokens file.nr    print the tokens
nuru --toleo             print the version        (-v, --version)
nuru --msaada            print help               (-h, --help)
```

The REPL keeps variables between lines, continues a statement while `(`, `[` or `{` are open, and prints any result that is not `tupu`. Leave it with `toka()`, `exit()` or end of input.

Errors name the line, in Swahili like Nuru does: `Mstari 3: Kitambulishi hakipatikani: x`. A runtime error in a file exits with status 1.

## The language

Semicolons are optional and newlines are not significant. Comments are `// …` and `/* … */`.

Because newlines do not end a statement, a line that starts with `(` or `[` continues the previous line: `x = 3` followed by `[1, 2]` on the next line parses as `3[1, 2]`. End the earlier line with `;` when that matters.

### Values

| Type | Examples | `aina()` |
|---|---|---|
| integer | `42`, `-7` | `NAMBA` |
| float | `3.14` | `DESIMALI` |
| string | `"habari"`, `'habari'` | `NENO` |
| boolean | `kweli`, `sikweli` | `BOOLEAN` |
| null | `tupu` | `TUPU` |
| array | `[1, "pili", kweli]` | `ORODHA` |
| dictionary | `{"jina": "Juma", 1: "moja"}` | `KAMUSI` |
| function | `unda(x) { rudisha x }` | `KAZI` |

Only `tupu` and `sikweli` are false; `0` and `""` are true. Strings are UTF-8 and are indexed and looped over by character. Dictionary keys can be strings, integers, floats or booleans, and keep their insertion order. Arrays and dictionaries are shared by reference.

### Variables and operators

```nuru
fanya x = 10        // always creates x in the current scope
y = 20              // updates an existing y, or creates it
x += 5
y %= 7
x++

2 + 3 * 5           // 17
3 / 5               // 0.6      (exact integer division stays an integer: 6 / 3 is 2)
2 ** 10             // 1024
"ab" * 3;           // "ababab"
[1, 2] + [3];       // [1, 2, 3]
20 ktk [10, 20]     // kweli    (membership: arrays, dictionary keys, substrings)
```

Integer arithmetic that would overflow continues as a float. Operator precedence follows Nuru, including two oddities: `%` binds tighter than `**`, and `&&` and `||` share one level.

### Control flow

```nuru
kama (x > 5) { andika("kubwa") } au kama (x == 5) { andika("tano") } sivyo { andika("ndogo") }

wakati (i < 10) { i++; kama (i == 3) { endelea }; kama (i == 8) { vunja } }

kwa v ktk [10, 20] { andika(v) }              // values
kwa i, v ktk "abc" { andika(i, v) }           // index (or key) and value

badili (x) {
    ikiwa 1, 2 { andika("moja au mbili") }
    kawaida    { andika("nyingine") }
}
```

### Functions

```nuru
fanya jumla = unda(a, b = 10) { rudisha a + b }
jumla(1)            // 11
jumla(1, 2)         // 3

fanya hesabu = unda() {
    n = 0
    rudisha unda() { n++; rudisha n }      // closures
}
```

A function returns the value of its last statement unless `rudisha` is used. Functions create a scope; `kama`, `wakati` and `kwa` blocks do not. Recursion is limited to 1000 nested calls, or fewer if the platform stack runs low; either way it is an error, not a crash.

### Packages and imports

A `pakeji` is a named scope. Its variables are its members, and `@` inside it refers to the package itself.

```nuru
pakeji benki {
    andaa = unda(salio) { @.salio = salio }
    weka = unda(kiasi) { @.salio += kiasi; rudisha @.salio }
}

benki.andaa(100)
benki.weka(50)      // 150
benki.salio         // 150
```

`tumia name` imports a built-in module, or loads `name.nr` from the directory of the running script (falling back to the working directory). The file must define `pakeji name`. A file is loaded once, and its top-level variables stay private to it.

```nuru
tumia hisabati, jsoni
tumia benki         // loads benki.nr
```

Method calls accept named arguments: `benki.andaa(salio = 100)`. Plain calls do not, because `f(x = 1)` is an assignment.

### Built-in functions

| Function | |
|---|---|
| `andika(...)` | print the arguments separated by spaces |
| `jaza(prompt?)` | read a line of input (`tupu` at end of input) |
| `aina(x)` | the type name of `x` |
| `mfululizo(end)`, `mfululizo(start, end, step?)` | array of integers |
| `fungua(path)` | open a file; `.soma()` returns its contents |
| `toka(code?)`, `exit(code?)` | exit |

### Methods

| On | Methods |
|---|---|
| string | `idadi()`, `herufikubwa()`, `herufindogo()`, `gawa(separator?)` |
| array | `idadi()`, `sukuma(...)`, `yamwisho()`, `map(f)`, `chuja(f)` |

### Modules

| Module | Contents |
|---|---|
| `hisabati` | constants `PI`, `e`, `phi`, `ln2`, `ln10`, `log2e`, `log10e`, `sqrt1_2`, `sqrt2`, `sqrt3`, `sqrt5`, `EPSILON`; functions `abs sign sqrt cbrt root factorial ceil floor round max min hypot random` and the trigonometric, hyperbolic, exponential and logarithmic functions |
| `jsoni` | `dikodi(text)` parses JSON into dictionaries and arrays; `enkodi(value)` produces compact JSON |
| `muda` | `hasahivi()`, `leo()`, `tangu(t)`, `lala(seconds)`, `baada_ya(seconds)`, `tofauti(a, b)`, and `t.ongeza(sekunde=, dakika=, masaa=, siku=, wiki=, miezi=, miaka=)` |
| `mtandao` | HTTP: `peruzi(url)` or `peruzi(yuareli=, vichwa=, mwili=)` sends a GET (`mwili` becomes query parameters); `tuma(yuareli=, vichwa=, mwili=)` sends a POST. `vichwa` is a dictionary of headers. A dictionary or array `mwili` is sent as JSON, a string as is. Both return the response body as a string whatever the status code, and raise an error if the request cannot be made (30 second timeout). |
| `os` | `kimbiza(command)` runs a shell command and returns what it printed; `toka(code?)` exits |

## What's missing

- Dictionary methods and `dict.key` access (use `dict["key"]`).
- Running scripts you do not trust: `fungua`, `tumia`, `os.kimbiza` and `mtandao` can all reach outside the program, and there are no step or time limits yet.
- `hisabati.round` with a rounding-method argument.
- Memory from closures and self-referencing structures is not reclaimed until the program exits.

## Tests

```sh
cmake -Stest -Bbuild/test
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

## Using it as a library

```cpp
#include <mni/interpreter.h>

mni::Interpreter interp;                       // prints to std::cout, reads std::cin
mni::Value result = interp.run("fanya x = 6 * 7; x");
std::cout << result.inspect();                 // 42
```

`run` throws `mni::SyntaxError` (all parser messages in `errors`), `mni::NuruError` for runtime errors, and `mni::ExitRequest` for `toka()`. State persists between `run` calls.

## Project layout

```
include/mni/   public headers: token, lexer, ast, parser, symbol, value, environment, interpreter, repl
source/
  lexer, parser, ast, token         the frontend
  value, symbol, environment        values, interned names, variable scopes
  interpreter, operators, stack     the evaluator (operators.cpp: arithmetic, comparison, indexing;
                                    stack.cpp: turns runaway recursion into an error)
  imports.cpp                       pakeji and tumia: package scopes, module and file loading
  builtins.cpp, methods.cpp         global functions; string and array methods
  hisabati, jsoni, muda, modules    the built-in modules and their registry
  native.h                          internal helpers shared by the native code (not installed)
standalone/    the `nuru` executable
test/          doctest unit tests
bench/         benchmark programs; `bench/run.sh` prints their results, time them with a Release build
cmake/         CPM and helper scripts
```

## License

MIT, see [LICENSE](LICENSE). Nuru itself is a separate project under the GPL-2.0.
