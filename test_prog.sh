#!/bin/bash


src_file="$1"


./run_ipc.sh IPCAnalysisPass "$src_file" live_runtime.cpp
./run_pass.sh InstructionAnalysisPass MemoryAnalysisPass "$src_file" live_runtime.cpp