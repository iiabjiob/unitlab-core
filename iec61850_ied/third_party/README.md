# Third-Party Dependencies

This directory contains source dependencies vendored for the standalone IEC 61850 native build.

## pugixml

- Version: 1.14
- Upstream: https://pugixml.org/
- Source: https://github.com/zeux/pugixml
- License: MIT
- Local path: `pugixml/`

`pugixml` is used internally by the C++ SCL compiler. It does not cross the public C ABI; consumers continue to use `scl_compiler/unitlab_scl_compiler.h`.
