# RagAgent

RagAgent 是一个运行在 Mac 本地的 C++ RAG 知识库 Agent。它可以读取本地 PDF、Word、Markdown、文本和代码文件，把文档切成 chunk 后生成 embedding，存入本地 SQLite，然后在提问时检索相关片段并交给本地大模型生成答案。

当前实现直接调用 Ollama HTTP API：

- `bge-m3`：生成 query 和文档 chunk 的 embedding
- `gemma4:26b-a4b-it-q4_K_M`：基于检索片段回答问题
- SQLite：保存文档 chunk、来源信息和 embedding
- `pdftotext`：提取文字型 PDF
- `textutil`：提取 `.doc` / `.docx` 文本

## 功能

- 支持索引本地文件夹
- 支持 PDF、Word、Markdown、TXT、JSON、CSV、YAML 和常见代码文件
- PDF 来源保留页码
- Word 简历可通过 macOS `textutil` 提取
- 检索时使用余弦相似度
- 命中 chunk 后会带上同文件相邻片段，减少回答只看到局部内容的问题
- 默认只把 3 个 chunk 交给大模型，降低本地生成耗时
- 交互时打印检索调试信息和耗时统计

## 环境要求

- macOS
- CMake
- C++20 编译器
- Ollama
- SQLite3
- libcurl
- Poppler 的 `pdftotext`

安装常用依赖：

```bash
brew install cmake poppler
```

确认工具存在：

```bash
cmake --version
pdftotext -v
ollama list
```

## 模型准备

拉取 embedding 模型：

```bash
ollama pull bge-m3
```

确认你的本地生成模型名称：

```bash
ollama list
```

默认配置使用：

```text
LLM_MODEL=gemma4:26b-a4b-it-q4_K_M
EMBEDDING_MODEL=bge-m3
```

如果你的模型名不同，修改 `.env`。

## 配置

复制配置示例：

```bash
cp .env.example .env
```

`.env.example`：

```text
LLM_MODEL=gemma4:26b-a4b-it-q4_K_M
EMBEDDING_MODEL=bge-m3
OLLAMA_BASE_URL=http://localhost:11434
DB_PATH=storage/knowledge.sqlite3
CHUNK_SIZE=800
CHUNK_OVERLAP=120
RETRIEVAL_K=5
```

## 编译

```bash
cmake -S . -B build
cmake --build build
```

生成的可执行文件：

```text
build/local_agent
```

## 准备文档

把你的本地资料放到：

```text
data/docs/
```

支持的文件类型包括：

```text
.pdf
.doc
.docx
.md
.txt
.json
.csv
.yaml
.yml
.cpp
.hpp
.h
.c
.py
.js
.ts
.tsx
.jsx
```

注意：当前 PDF 支持的是文字型 PDF。扫描版 PDF 需要 OCR 才能检索。

## 建立索引

首次索引或重新索引：

```bash
./build/local_agent index --path data/docs --reset
```

新增、删除或修改文档后，也需要重新索引：

```bash
./build/local_agent index --path data/docs --reset
```

## 项目结构

```text
.
├── CMakeLists.txt
├── include/
│   ├── config.hpp
│   ├── document.hpp
│   ├── document_loader.hpp
│   ├── json_utils.hpp
│   ├── ollama_client.hpp
│   ├── text_splitter.hpp
│   └── vector_store.hpp
├── src/
│   ├── config.cpp
│   ├── document_loader.cpp
│   ├── json_utils.cpp
│   ├── main.cpp
│   ├── ollama_client.cpp
│   ├── text_splitter.cpp
│   └── vector_store.cpp
├── data/
│   └── docs/
│       └── .gitkeep
└── storage/
```

## 工作流程

索引阶段：

```text
读取文档
  -> 提取文本
  -> 切分 chunk
  -> 调用 Ollama bge-m3 生成 embedding
  -> 写入 SQLite
```

查询阶段：

```text
用户问题
  -> 调用 bge-m3 生成 query embedding
  -> 从 SQLite 读取 chunk embedding
  -> 计算余弦相似度
  -> 选择相关 chunk
  -> 构造 prompt
  -> 调用 Gemma4 生成答案
```

## 查看 SQLite

数据库默认保存在：

```text
storage/knowledge.sqlite3
```

查看表结构：

```bash
sqlite3 storage/knowledge.sqlite3 ".schema chunks"
```

表结构：

```sql
CREATE TABLE chunks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  source TEXT NOT NULL,
  file_name TEXT NOT NULL,
  page INTEGER,
  text TEXT NOT NULL,
  embedding BLOB NOT NULL
);
```

## 注意事项

- embedding 模型一旦更换，需要重新索引
- 文档数量变多后，目前的 SQLite 全量扫描会变慢，后续可替换为 FAISS、hnswlib、Qdrant 或 Milvus
- 本地 26B 模型生成速度较慢，耗时主要在 `llm_generate_ms`
- 对于复杂 PDF、扫描版 PDF，需要增加 OCR 流程
