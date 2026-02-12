#pragma once

// Forward declare ModuleEditor to avoid circular includes
class ModuleEditor;

// Test functions - these can be called with or without an editor
void RunUUID64Tests(ModuleEditor* editor = nullptr);
void RunUUID64Tests(); // Console-only version