msgc
======

## Warning

Using this code could cause the dead to walk amongst the living.

## What is this?

A low-level C MessagePack library.

## Performance

Compiled with `clang -O2` on my laptop (2GHz i7), encoding is about 2GB/s, and decoding is about 1.6GB/s. 

## Compiling

The library doesn't use any compiler builtins, so in principle it should build with anything that supports C99 or later. However, I recommend using [`clang` with `-emit-llvm`](http://llvm.org/docs/LinkTimeOptimization.html) to build the object file, because it will allow for better link-time optimizations. (See the `Makefile`.)