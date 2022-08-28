# Spl
Simple programming language

## Quick start

### Bootstrapping
This compiler is self-hosted and needs to be bootstrapped.
```
$ make bootstrap
```

### Compiling
Compiling is as simple as it gets, pass the file of source code that you want to be compiled.
```
$ ./spl <program>
```

And that's it.

### Options

There are a number of configurable options that you can pass to the compiler. To view those, run the compiler without any arguments.

```
$ ./spl
Usage; ./spl [OPTIONS]

OPTIONS:
  <filename>      - path to file
  run             - run program directly
  no-com          - do not compile target executable
  no-debug        - do not write debug information
  enable-warnings - enable basic warnings
  disable-dce     - disable dead code elimination
  dce-all         - eliminate all dead code, including all symbols
  verbose         - enable verbose output
```

### Examples
Hello, World:
```rust

include "lib/common.spl"

fn main -> none {
  printf("Hello, World\n", NULL);
}
```

Program that counts from 0 to 9:
```rust

fn main -> none {
  let i : u64 0;
  let count : u64 10;
  while < i count {
    print i;
    store64 @i + 1 i;
  }
}
```

You can find more examples in the examples directory, or you could read the compiler source code to get familiar with the language.
