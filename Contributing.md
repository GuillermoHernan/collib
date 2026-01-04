# Contributing to Collib

**Thank you for your interest!** 🎉 Collib is in **early development** and actively welcomes **feedback, bug reports, and suggestions**.

## 🎯 Design Philosophy: Small & Focused

**Collib deliberately stays small and focused.** New features are added **very sparingly** after careful consideration:

Core Principle: Do one thing, and do it well

- ✅ Essential containers (darray, BTreeMap, vrange, span)
- ✅ Simple iteration model
- ✅ Allocator integration
- ❌ General-purpose STL replacement
- ❌ Kitchen sink features
- ❌ Experimental algorithms

**This keeps Collib:**
- Easy to understand and fork
- Highly optimized and tested
- Stable and predictable

## Current Focus: Feedback & Discussion

**At this stage, we prioritize:**
- 🐛 **Bug reports** with minimal reproducible examples ⭐
- 💡 **Feature suggestions** (must align with "small & focused") ⭐
- 📊 **Performance feedback** and benchmarks ⭐
- 🤔 **Design discussions** about existing components

**Code contributions are currently limited** - please **discuss changes first** to avoid wasted effort.

## How to Help (Most Valuable → Least)

### 1. **Report Issues** ⭐ **Most Helpful**
Great bug report includes:

```cpp
// Minimal reproducible example
#include <darray.h>
coll::darray<int> arr;
// ... your code
```

- Expected vs actual behavior
- Compiler version and build config
- Steps to reproduce

### 2. **Share Feedback** ⭐ **Highly Welcome**
- Performance observations vs STL
- Missing use cases
- Documentation improvements
- Design philosophy questions
- Ergonomy suggestions

### 3. **Discuss Features** ⭐ **Always Welcome** *(Small & Focused Filter Applied)*

Process:

1. Open Issue: "Feature Request: [description]"
2. Explain: Why does this fit "small & focused"?
3. Discuss and agree

### 4. **Code Contributions** (Discuss First - Very Selective)

Current policy:
- ✅ Small fixes for approved issues (bugs only)
- ✅ Performance improvements to existing containers
- ❌ New container types
- ❌ API redesigns
- ❌ Refactoring without benchmarks

Process:

1. Open Issue first - "I'd like to fix/improve X"
2. Discuss & wait for explicit approval
3. Unit tests are mandatory
4. Every PR must be reviewed & approved

## Submitting Bugs - Template

- Summary
- Bugs description
- Steps to Reproduce
- Minimal code example
- Expected vs Actual
- Environment
  - Collib: [commit/version]
  - Compiler: [version]
  - OS / platform: [details]

## Process Summary
- 🐛 Bug? → Issue with repro
- 💡 Idea? → Issue with "small & focused" reasoning
- 🔧 Fix? → Discuss → Approved → PR

## Why This Approach?

Early-stage goals:
- ✅ Stabilize core components (darray, BTreeMap, vrange, span, allocator)
- ✅ Validate performance claims
- ✅ Perfect existing APIs first
- ✅ Avoid feature creep

## Future: Selective Code Contributions

**After v1.0 (stable APIs):**
- An ergonomic API (hopefully)
- Approved enhancements to core containers
- Optimized allocators
- Cross-platform support
- Still: **No general STL replacement**

## Get in Touch

**Questions? Ideas?** Open an Issue.

---

*"Small libraries that do one thing well > Large libraries that do many things poorly"*
