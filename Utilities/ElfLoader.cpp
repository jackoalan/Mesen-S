#include "stdafx.h"
#include "ElfLoader.h"
#include "../ElfUtils/config.h"
#include "../ElfUtils/libelf/gelf.h"
#include "../ElfUtils/libelf/libelf.h"
#include "../ElfUtils/libdw/libdw.h"
#include "../ElfUtils/lib/system.h"
#include "../Core/MessageManager.h"
#include "../Core/CartTypes.h"

#include <fcntl.h>
#include <cassert>

class DwarfContext {
	Dwarf *_dw = nullptr;
public:
	explicit DwarfContext(Elf *elf) {
		_dw = dwarf_begin_elf(elf, DWARF_C_READ, nullptr);
	}

	~DwarfContext() {
		if (_dw != nullptr)
			dwarf_end(_dw);
	}

	bool IsValid() const { return _dw != nullptr; }

	template<typename Func>
	void EnumerateLines(const Func &f) {
		if (!IsValid())
			return;

		Dwarf_Off off;
		Dwarf_Off next_off = 0;
		Dwarf_CU *cu = nullptr;
		Dwarf_Lines *lb;
		size_t nlb;
		int res;
		while ((res = dwarf_next_lines(_dw, off = next_off, &next_off, &cu,
												 nullptr, nullptr, &lb, &nlb)) == 0) {
			for (size_t i = 0; i < nlb; ++i) {
				Dwarf_Line *l = dwarf_onesrcline(lb, i);
				if (l == nullptr) {
					MessageManager::Log(
						"[ElfLoader] Cannot get individual line");
					break;
				}

				Dwarf_Addr addr;
				if (dwarf_lineaddr(l, &addr) != 0)
					addr = 0;
				const char *file = dwarf_linesrc(l, nullptr, nullptr);
				int line;
				if (dwarf_lineno(l, &line) != 0)
					line = 0;

				f(addr, file, line);
			}
		}
	}
};

class ElfContext {
	const char *_filename = nullptr;
	int _fd = -1;
	Elf *_elf = nullptr;
	size_t _shstrndx = 0;

	const char *StrPtr(size_t offset) {
		return elf_strptr(_elf, _shstrndx, offset);
	}

public:
	explicit ElfContext(const char *filename, bool printErr = true) : _filename(
		filename) {
		_fd = open(filename, O_RDONLY);
		if (_fd == -1) {
			if (printErr)
				MessageManager::Log(
					"[ElfLoader] Unable to open " + string(filename) +
					" for reading");
			return;
		}

		elf_version(EV_CURRENT);

		_elf = elf_begin(_fd, ELF_C_READ_MMAP, nullptr);
		if (_elf == nullptr) {
			if (printErr)
				MessageManager::Log(
					"[ElfLoader] Problems opening " + string(filename) +
					" as ELF file: " +
					string(
						elf_errmsg(-1)));
			close(_fd);
			_fd = -1;
			return;
		}

		GElf_Ehdr ehdr_mem;
		GElf_Ehdr *ehdr = gelf_getehdr(_elf, &ehdr_mem);
		if (ehdr == nullptr) {
			if (printErr)
				MessageManager::Log(
					"[ElfLoader] no ELF header in " + string(filename));
			elf_end(_elf);
			_elf = nullptr;
			close(_fd);
			_fd = -1;
			return;
		}

		if (elf_getshdrstrndx(_elf, &_shstrndx) < 0) {
			if (printErr)
				MessageManager::Log(
					"[ElfLoader] elf_getshdrstrndx failed in " +
					string(_filename));
			elf_end(_elf);
			_elf = nullptr;
			close(_fd);
			_fd = -1;
			return;
		}
	}

	~ElfContext() {
		if (_elf != nullptr)
			elf_end(_elf);
		if (_fd >= 0)
			close(_fd);
	}

	bool IsValid() const { return _fd >= 0 && _elf != nullptr; }

	template<typename Func>
	void EnumerateSections(const Func &f) {
		if (!IsValid())
			return;

		GElf_Section secidx = 0;
		Elf_Scn *scn = nullptr;
		while ((scn = elf_nextscn(_elf, scn)) != nullptr) {
			GElf_Shdr shdr_mem;
			GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_mem);
			if (!f(scn, shdr, secidx))
				return;
			++secidx;
		}
	}

	SnesCartInformation *GetSfcHeaderData() {
		Elf_Data *data = nullptr;
		EnumerateSections(
			[this, &data](Elf_Scn *scn, GElf_Shdr *shdr, GElf_Section secidx) {
				if (!(shdr->sh_flags & SHF_ALLOC) || shdr->sh_type == SHT_NOBITS)
					return true;

				const char *sname = StrPtr(shdr->sh_name);
				if (!strcmp(sname, ".sfc.header")) {
					data = elf_getdata(scn, nullptr);
					return false;
				}
				return true;
			});
		return data && data->d_size >= sizeof(SnesCartInformation)
				 ? (SnesCartInformation *) data->d_buf : nullptr;
	}

	vector<uint8_t> ToBinary() {
		if (!IsValid())
			return {};

		SnesCartInformation *headerData = GetSfcHeaderData();
		if (!headerData) {
			MessageManager::Log(
				"[ElfLoader] no SNES header data in " +
				string(_filename));
			return {};
		}

		int flags =
			(headerData->MapMode & 0x1) != 0 ? CartFlags::HiRom : CartFlags::LoRom;
		if ((flags & CartFlags::HiRom) && (headerData->MapMode & 0x27) == 0x25) {
			flags |= CartFlags::ExHiRom;
		} else if ((flags & CartFlags::LoRom) &&
					  (headerData->MapMode & 0x27) == 0x22) {
			flags |= CartFlags::ExLoRom;
		}

		vector<uint8_t> binData;

		EnumerateSections(
			[flags, &binData](Elf_Scn *scn, GElf_Shdr *shdr,
									GElf_Section secidx) {
				if (!(shdr->sh_flags & SHF_ALLOC) || shdr->sh_type == SHT_NOBITS)
					return true;

				Elf_Data *data = elf_getdata(scn, nullptr);
				auto *cur = (uint8_t *) data->d_buf;
				int remLen = (int) data->d_size;
				Elf64_Addr inCompAddr = shdr->sh_addr + data->d_off;

				auto advance = [&](int amt) {
					cur += amt;
					remLen -= amt;
					inCompAddr += amt;
				};

				auto copyTo = [&](uint8_t *data, int offset, int length) {
					int rounded = offset + length;
					rounded = (rounded + 0x7fff) & ~0x7fff;
					binData.resize(rounded);
					std::memcpy(binData.data() + offset,
									data,
									length);
					advance(length);
				};

				// Iteratively copy 32K bank segments while resolving ROM addresses
				// from CPU addresses
				while (remLen > 0) {
					uint8_t inBank = inCompAddr >> 16;
					uint16_t inAddr = inCompAddr & 0xffff;

					if (flags & CartFlags::LoRom) {
						if ((inBank >= 0x00 && inBank <= 0x7D) ||
							 (inBank >= 0x80 && inBank <= 0xFF)) {
							if (inAddr < 0x8000) {
								// Skip non-prgrom mappings
								advance(0x8000 - inAddr);
								continue;
							}
							// Copy alternating bank halves
							int romAddr = inAddr - 0x8000;
							int offset = (inBank % 0x80) * 0x8000 + romAddr;
							int copyLen = std::min(remLen, 0x8000 - romAddr);
							copyTo(cur, offset, copyLen);
						} else {
							// Skip non-prgrom banks
							advance(0x10000 - inAddr);
						}
					} else if (flags & CartFlags::HiRom) {
						if ((inBank >= 0x00 && inBank <= 0x3F) ||
							 (inBank >= 0x80 && inBank <= 0xBF)) {
							if (inAddr < 0x8000) {
								// Skip non-prgrom mappings
								advance(0x8000 - inAddr);
								continue;
							}
							// Only copy into upper halves of banks
							int romAddr = inAddr;
							int offset = (inBank % 0x80) * 0x10000 + romAddr;
							int copyLen = std::min(remLen, 0x10000 - romAddr);
							copyTo(cur, offset, copyLen);
						} else if ((inBank >= 0x40 && inBank <= 0x7D) ||
									  (inBank >= 0xC0 && inBank <= 0xFF)) {
							// Full-size bank copy
							int romAddr = inAddr;
							int offset = ((inBank - 0x40) % 0x80) * 0x10000 + romAddr;
							int copyLen = std::min(remLen, 0x10000 - romAddr);
							copyTo(cur, offset, copyLen);
						} else {
							// Skip non-prgrom banks
							advance(0x10000 - inAddr);
						}
					} else {
						// TODO: ExRom
						assert(false && "ExRom loader not implemented");
					}
				}

				return true;
			});

		return binData;
	}

	template<typename Func>
	bool AccessDwarf(const Func &f) {
		if (!IsValid())
			return false;

		DwarfContext dw(_elf);
		return f(dw);
	}

	template<typename Func>
	void EnumerateSyms(const Func &f) {
		EnumerateSections(
			[this, &f](Elf_Scn *scn, GElf_Shdr *shdr, GElf_Section secidx) {
				if (shdr->sh_type != SHT_SYMTAB)
					return true;

				Elf_Data *data = elf_getdata(scn, nullptr);
				unsigned nsyms = (shdr->sh_size
										/ gelf_fsize(_elf, ELF_T_SYM, 1, EV_CURRENT));
				for (unsigned i = 0; i < nsyms; ++i) {
					GElf_Sym sym;
					GElf_Sym *symp = gelf_getsym(data, i, &sym);
					if (symp && GELF_ST_TYPE(symp->st_info) <= STT_FUNC &&
						 strlen(elf_strptr(_elf, shdr->sh_link, symp->st_name)))
						if (!f(symp, shdr))
							return false;
				}
				return true;
			});
	};

	bool GetDwarfInfo(GetDwarfInfoArgs args) {
		if (!IsValid())
			return false;

		return AccessDwarf([this, args](DwarfContext &dw) {
			vector<GElf_Section> uniqueSecs;
			uint32_t numSyms = 0;
			vector<string> uniqueFiles;
			uint32_t numLocs = 0;

			EnumerateSections(
				[&uniqueSecs](Elf_Scn *, GElf_Shdr *shdr, GElf_Section secidx) {
					if ((shdr->sh_flags & SHF_ALLOC) &&
						 (shdr->sh_type != SHT_NOBITS))
						uniqueSecs.push_back(secidx);
					return true;
				});

			EnumerateSyms([&numSyms](GElf_Sym *symp, GElf_Shdr *shdr) {
				++numSyms;
				return true;
			});

			dw.EnumerateLines(
				[&uniqueFiles, &numLocs](Dwarf_Addr addr, const char *file,
												 int line) {
					auto it = std::find(uniqueFiles.begin(), uniqueFiles.end(),
											  file);
					if (it == uniqueFiles.end())
						uniqueFiles.emplace_back(file);

					++numLocs;
				});

			args.countCb(uniqueSecs.size(), uniqueFiles.size(), numLocs, numSyms);

			EnumerateSections(
				[this, appendSecCb = args.appendSecCb](Elf_Scn *, GElf_Shdr *shdr,
																	GElf_Section secidx) {
					if ((shdr->sh_flags & SHF_ALLOC) &&
						 (shdr->sh_type != SHT_NOBITS)) {
						const char *sname = StrPtr(shdr->sh_name);
						appendSecCb(sname, shdr->sh_addr, shdr->sh_size);
					}
					return true;
				});

			EnumerateSyms(
				[this, &uniqueSecs, appendSymCb = args.appendSymCb](GElf_Sym *symp,
																					 GElf_Shdr *shdr) {
					char *symName = elf_strptr(_elf, shdr->sh_link, symp->st_name);
					auto it = std::find(uniqueSecs.begin(), uniqueSecs.end(),
											  symp->st_shndx);
					if (strlen(symName) && it != uniqueSecs.end()) {
						uint32_t secIdx = std::distance(uniqueSecs.begin(), it);
						appendSymCb(symName, secIdx, symp->st_value, symp->st_size);
					}
					return true;
				});

			for (const string &filePath: uniqueFiles)
				args.appendFileCb(filePath.c_str());

			dw.EnumerateLines(
				[&uniqueFiles, appendLocCb = args.appendLocCb](Dwarf_Addr addr,
																			  const char *file,
																			  int line) {
					auto it = std::find(uniqueFiles.begin(), uniqueFiles.end(),
											  file);
					uint32_t fileIdx = std::distance(uniqueFiles.begin(), it);
					appendLocCb(fileIdx, line, addr);
				});

			return true;
		});
	}
};

bool ElfLoader::IsHeaderValid() const {
	ElfContext ctx(_filename.c_str(), false);
	return ctx.IsValid();
}

vector<uint8_t> ElfLoader::ToBinary() const {
	ElfContext ctx(_filename.c_str(), true);
	return ctx.ToBinary();
}

bool ElfLoader::GetDwarfInfo(GetDwarfInfoArgs args) const {
	ElfContext ctx(_filename.c_str(), true);
	return ctx.GetDwarfInfo(args);
}
