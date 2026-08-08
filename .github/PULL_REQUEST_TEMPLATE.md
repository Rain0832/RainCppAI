## 变更类型

<!-- 请勾选适用的类型 -->

- [ ] feat（新功能）
- [ ] fix（Bug 修复）
- [ ] docs（文档变更）
- [ ] refactor（重构）
- [ ] chore（构建/CI/依赖）
- [ ] test（测试）

## 关联 Issue

<!-- 使用 Closes #n 或 Ref #n 关联对应 issue -->

Closes #

## 变更说明

<!--
简述本次变更的底层逻辑，例如：
- 是否涉及 C++ 内存管理（new/delete、智能指针、RAII）？
- 是否涉及网络库路由变更（HttpServer 模块）？
- 是否引入新的第三方依赖？
- 是否修改了现有接口签名？
-->

## 测试清单

<!-- 合并前必须全部确认 -->

- [ ] 本地编译通过 (`cmake --build build`)
- [ ] 单元测试通过 (`cd build && ctest`)
- [ ] clang-format 格式检查通过 (`make format`)
- [ ] 未引入新的编译警告
- [ ] 相关 TECHDOC 已同步更新
