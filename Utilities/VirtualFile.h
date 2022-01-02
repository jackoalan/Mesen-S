#pragma once
#include "stdafx.h"
#include <sstream>

typedef void(__cdecl *GetDwarfInfoObjectCountCallback)(uint32_t numSecs, uint32_t numFiles, uint32_t numLocs, uint32_t numSyms);
typedef void(__cdecl *GetDwarfInfoAppendSecCallback)(const char *secName, uint32_t addr, uint32_t size);
typedef void(__cdecl *GetDwarfInfoAppendFileCallback)(const char *filePath);
typedef void(__cdecl *GetDwarfInfoAppendLocCallback)(uint32_t fileIdx, uint32_t line, uint32_t addr);
typedef void(__cdecl *GetDwarfInfoAppendSymCallback)(const char *namePtr, uint32_t secIdx, uint32_t addr, uint32_t size);

struct GetDwarfInfoArgsCb {
	GetDwarfInfoObjectCountCallback countCb;
	GetDwarfInfoAppendSecCallback appendSecCb;
	GetDwarfInfoAppendFileCallback appendFileCb;
	GetDwarfInfoAppendLocCallback appendLocCb;
	GetDwarfInfoAppendSymCallback appendSymCb;
};

struct GetDwarfInfoArgs {
	GetDwarfInfoArgsCb main;
	GetDwarfInfoArgsCb spc;
};

class VirtualFile
{
private:
	string _path = "";
	string _innerFile = "";
	int32_t _innerFileIndex = -1;
	vector<uint8_t> _data;

	void FromStream(std::istream &input, vector<uint8_t> &output);

	void LoadFile();

public:
	static const std::initializer_list<string> RomExtensions;

	VirtualFile();
	VirtualFile(const string &archivePath, const string innerFile);
	VirtualFile(const string &file);
	VirtualFile(const void *buffer, size_t bufferSize, string fileName = "noname");
	VirtualFile(std::istream &input, string filePath);

	operator std::string() const;
	
	bool IsValid();
	string GetFilePath();
	string GetFolderPath();
	string GetFileName();
	string GetSha1Hash();

	size_t GetSize();

	bool ReadFile(vector<uint8_t> &out);
	bool ReadFile(std::stringstream &out);
	bool ReadFile(uint8_t* out, uint32_t expectedSize);

	bool ApplyPatch(VirtualFile &patch);

	bool GetDwarfInfo(GetDwarfInfoArgs args) const;
};