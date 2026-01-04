# Collib - Collection Library

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![MSVC](https://img.shields.io/badge/Compiler-MSVC-red.svg)](https://visualstudio.microsoft.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Semantic Versioning](https://img.shields.io/badge/Versioning-SemVer%202.0.0-blueviolet.svg)](https://semver.org/)

**Collib** is a modern C++ container library designed as a simpler, more intuitive alternative to the STL. It emphasizes easy iteration, custom allocators, performance, and ease of modification.

## Key Features

- **Simple Iteration**: Single "range" object iteration (no iterator pairs) for cleaner, less error-prone code.
- **Custom Allocators**: Easy-to-use `IAllocator` interface with `tryExpand` support and scoped overrides.
- **B+ Tree Map**: Default associative container outperforms `std::map` in most scenarios.
- **Virtual Ranges (vrange)**: Polymorphic ranges for encapsulation, filtering, and transformation without exposing internals.
- **Spans**: Non-owning contiguous memory views with bidirectional iteration.
- **Dynamic Array (darray)**: Allocator-aware `std::vector` equivalent with range construction.
- **C++20 Concepts**: Type-safe generic programming with `Range`, `SimpleIterator`, and `HasSize`.

## Quick Start

### Prerequisites
- **MSVC 2022** (currently supported)
- **Clang, GCC, and Linux support coming soon**
- Visual Studio 2022 with C++20 support

### Build Instructions (MSVC)
Open the provided Visual Studio solution (`.sln`) file and build using Visual Studio 2022.

**CMake support planned for future cross-platform builds**

### Basic Usage

```cpp
#include <darray.h>
#include <btree.h>
#include <vrange.h>
#include <string>

int main() 
{
    coll::darray<int> arr;
    arr.emplace_back(1).emplace_back(2).emplace_back(3);

    coll::BTreeMap<int, std::string> map;
    map.emplace(1, "one");

    // Simple single-object iteration
    for (auto&& item : coll::make_range(arr))
        std::cout << item << " ";
    
    // Output: 1 2 3
}
```

## Versioning

**Collib** follows [Semantic Versioning 2.0.0](https://semver.org/):
- **MAJOR**: Breaking API changes
- **MINOR**: New features, backwards compatible
- **PATCH**: Bug fixes, backwards compatible

## Core Components

### Allocator System (`#include <allocator.h>`)
Custom `IAllocator` interface with `alloc()`, `tryExpand()`, `free()`. Includes `DebugAllocator` for leak detection and `AllocatorHolder` for scoped defaults.

**More allocators coming soon.**

**Example:**

```cpp
coll::DebugAllocator debugAlloc;
coll::darray<int> arr1(debugAlloc); // Uses debug allocator
coll::darray<int> arr2;             // Does not use debug allocator, but previous defaultAllocator()

{
    coll::AllocatorHolder scope(debugAlloc);    // defaultAllocator() will return 'debugAlloc' while in scope.
    coll::darray<int> arr11;                    // Uses debug allocator
}
coll::darray<int> arr3;             // Does not use debug allocator, but previous defaultAllocator()
```

### Virtual Ranges (`#include <vrange.h>`)
Polymorphic ranges supporting `filter()` and `transform()` views with lazy evaluation. They provide encapsulation to collections / iteration.

**Example:**
```cpp
coll::vrange<int> integer_source();

auto evens = integer_source().filter([](int x){ return x % 2 == 0; });
```

### Span & rspan (`#include <span.h>`)
Non-owning views over contiguous memory with forward/reverse iteration.

### darray (`#include <darray.h>`)
Dynamic array with full STL-like interface plus range construction and allocator support.

### BTreeMap<Key, Value> (`#include <btree.h>`)
High-performance ordered map using B+ tree. Better cache locality than red-black trees.

**Example:**
```cpp
coll::BTreeMap<std::string, int> scores;
auto [handle, inserted] = scores.emplace("Alice", 95);
if (inserted)
    std::cout << "New entry\n";
```

## Design Philosophy

- **Simplicity first**: Easy to understand (and fork).
- **Allocator friendly**: Memory control without complexity.
- **Unified iteration**: Single range objects throughout.
- **Performance**: B+ trees for maps, in-place expansion, span views.
- **Modern C++**: Concepts, spaceship operator, perfect forwarding.

## Performance Highlights
- **BTreeMap**: O(log n) operations with better cache locality than std::map. Benchmarks shows it has better performance in most scenarios.
- **darray**: tryExpand() optimization reduces reallocations. Takes advantage of bigger-than-requested memory blocks from allocators.
- **vrange**: Lazy evaluation, zero-copy transformations. Virtual call latency is hidden by modern Out-of-order CPUs in most cases.
- **allocators**: Easier to use allocators will make easier to optimice container memory allocation.

## Platform Support

| Platform/Compiler     | Status  |
|-----------------------|---------|
| MSVC 2022 (Windows)   | ✅ Supported |
| GCC/Clang (Linux)     | 🔄 Planned |
| Clang (macOS)         | 🔄 Planned |
| CMake Build System    | 🔄 Planned |

## Testing
Build and run tests using the Visual Studio solution (tests included with Catch2).

## Contributing
1. Fork and create feature branch
2. Add tests with Catch2
3. Ensure 100% coverage
4. Submit PR with benchmarks
5. Follow Semantic Versioning for changes

See [CONTRIBUTING.md](CONTRIBUTING.md)

## License
MIT License - see [LICENSE](LICENSE) [web:1]

---
