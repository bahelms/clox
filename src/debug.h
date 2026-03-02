#pragma once
#include <string>

#include "chunk.h"

void disassemble_chunk(const Chunk &chunk, std::string name);
int disassemble_instruction(const Chunk &chunk, int offset);
