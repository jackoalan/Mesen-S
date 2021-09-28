#pragma once
#include "stdafx.h"
#include "VirtualFile.h"

class ElfLoader
{
  string _filename;
public:
  explicit ElfLoader(string filename) : _filename(std::move(filename)) {}
  bool IsHeaderValid() const;
  vector<uint8_t> ToBinary() const;
	bool GetDwarfInfo(GetDwarfInfoArgs args) const;
};
