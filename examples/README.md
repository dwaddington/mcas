# Examples

This directory contains general examples of using MCAS with and without ADO.

## Basic

**cpp_basic** : Basic C++ client using MCAS as a plain key-value store.
**python_basic** : Basic Python client using MCAS as a plain key-value store.

## Personalities

These examples provide "personalities" consisting of the ADO, client-side adapter and a test program.

**tabulator_pmdk_fb** : Example ADO using PMDK (side-file). ADO is
written in C++ and the client is Python. Protocol based on
flatbuffers.

**cpp_list** : C++ ADO manipulating immutable list. Basic
demonstration of non-serializing relocation of data structures and ADO
operations without crash-consistency and persistent memory.

**cpp_symtab** : C++ ADO providing an immutable string table.
Pointers are "translated" as symbol ids.  Data is not fully
crash-consistent.

**python_numpy** : 

**example_fb** :


