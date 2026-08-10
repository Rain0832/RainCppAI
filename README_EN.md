# Dr.Rain — Healthcare AI Assistant

> Full-stack C++ AI medical platform. Multi-model LLM · SSE Streaming · MCP · ONNX Vision · Admin Dashboard.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.16-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v3.0.0-orange.svg)](CHANGELOG.md)

📖 **Full documentation (bilingual CN/EN):** [README.md](README.md)

---

### Quick Links

- [Full README with API reference, architecture diagram, env vars](README.md#english)
- [Contributing Guide](CONTRIBUTING.md)
- [Coding Standard](DEVELOP_STANDARD.md)
- [Changelog](CHANGELOG.md)
- [Security Policy](SECURITY.md)

### Build

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
./http_server -p 8088
```

### License

[Apache License 2.0](LICENSE)
