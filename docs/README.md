# 手册产物说明

本目录存放《lvgl_two 设计与使用手册》的三种形态，内容一致：

| 文件 | 用途 | 打开方式 |
|---|---|---|
| `manual.html` | **首选交付物**。单文件、离线、自带目录与打印样式 | 任意浏览器双击打开；Ctrl+P 可另存 PDF |
| `manual.md` | 源文件，便于版本管理与 Diff | Typora / VS Code / GitHub / Obsidian |
| `build_docx.py` | 由 `manual.md` 生成 Word | 见下 |

## 生成 Word 版本

本机开发环境的 shell 无法启动（0xC0000142），所以 `.docx` 没有预先生成。
在任意装有 Python + python-docx 的机器上执行：

```bash
python build_docx.py manual.md lvgl_two_设计与使用手册.docx
```

依赖仅 `python-docx`：

```bash
pip install python-docx
```

脚本支持的 Markdown 子集与 `manual.md` 用到的语法一致：
标题、围栏代码块、管道表格、无序列表、有序列表、粗体、行内代码。

## 维护约定

- 内容以 `manual.md` 为**唯一事实源**，改完再同步到 `manual.html` 内嵌的 Markdown 块。
- `manual.html` 里的 Markdown 位于 `<script type="text/markdown" id="md">` 与 `</script>` 之间，
  渲染器在文件末尾的 `<script>` 里（约 90 行，无外部依赖）。
- 源码行号会随代码改动漂移，更新手册时请一并核对附录 A。
