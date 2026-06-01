cmake_minimum_required(VERSION 3.20)
project(local_gemma_agent_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(local_agent
    src/main.cpp
    src/config.cpp
    src/document_loader.cpp
    src/json_utils.cpp
    src/ollama_client.cpp
    src/text_splitter.cpp
    src/vector_store.cpp
)

target_include_directories(local_agent PRIVATE include)
target_link_libraries(local_agent PRIVATE CURL::libcurl SQLite3::SQLite3)
