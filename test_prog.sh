#!/bin/bash


src_file="$1"


./scripts/mca_insert.sh MCAInserterPass "$src_file"
./run_pass.sh InstructionAnalysisPass MemoryAnalysisPass "$src_file" live_runtime.cpp