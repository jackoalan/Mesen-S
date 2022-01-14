using System;
using System.Collections.Generic;
using System.Linq;
using Mesen.GUI.Config;
using Mesen.GUI.Debugger.Labels;
using Mesen.GUI.Debugger.Workspace;

namespace Mesen.GUI.Debugger.Integration
{
	public class DwarfImporter : ISymbolProvider
	{
		private readonly List<EmuApi.ElfSec> _elfSecsCpu;
		private readonly List<EmuApi.ElfSec> _elfSecsSpc;
		private readonly Dictionary<int, SourceCodeLocation> _linesByPrgAddress;
		private readonly Dictionary<int, SourceCodeLocation> _linesBySpcAddress;
		private readonly Dictionary<int, SourceSymbol> _symbolsByCpuAddress;
		private readonly Dictionary<int, SourceSymbol> _symbolsByPrgAddress;
		private readonly Dictionary<int, SourceSymbol> _symbolsBySpcAddress;
		private readonly List<SourceSymbol> _allSymbols;

		public DwarfImporter(EmuApi.DwarfInfo info)
		{
			SymbolFileStamp = info.SymbolFileStamp;
			SymbolPath = info.SymbolPath;
			_elfSecsCpu = info.Cpu.Sections;
			_elfSecsSpc = info.Spc.Sections;
			SourceFiles = new List<SourceFileInfo>();
			SourceFiles.AddRange(info.Cpu.SourceFiles);
			SourceFiles.AddRange(info.Spc.SourceFiles);

			_linesByPrgAddress = new Dictionary<int, SourceCodeLocation>(info.Cpu.LinesByCpuAddress.Count);
			foreach (var line in info.Cpu.LinesByCpuAddress)
			{
				GetLabelAddressAndType(line.Key, SnesMemoryType.CpuMemory, out var labelAddress, out var addressType);
				if (addressType.Value != SnesMemoryType.PrgRom)
					continue;
				_linesByPrgAddress.Add(labelAddress, line.Value);
			}

			_linesBySpcAddress = new Dictionary<int, SourceCodeLocation>(info.Spc.LinesByCpuAddress.Count);
			foreach (var line in info.Spc.LinesByCpuAddress) {
				GetLabelAddressAndType(line.Key, SnesMemoryType.SpcMemory, out var labelAddress, out var addressType);
				if (addressType.Value != SnesMemoryType.SpcRam)
					continue;
				_linesBySpcAddress.Add(labelAddress, line.Value);
			}
			
			_symbolsByCpuAddress = info.Cpu.SymbolsByCpuAddress;
			_symbolsByPrgAddress = new Dictionary<int, SourceSymbol>(info.Cpu.SymbolsByCpuAddress.Count);
			foreach (var sym in info.Cpu.SymbolsByCpuAddress)
			{
				GetLabelAddressAndType(sym.Key, SnesMemoryType.CpuMemory, out var labelAddress, out var addressType);
				if (addressType.Value != SnesMemoryType.PrgRom)
					continue;
				_symbolsByPrgAddress.Add(labelAddress, sym.Value);
			}

			_symbolsBySpcAddress = info.Spc.SymbolsByCpuAddress;

			_allSymbols = _symbolsByCpuAddress.Values.ToList();
			_allSymbols.AddRange(_symbolsBySpcAddress.Values);
		}

		public DateTime SymbolFileStamp { get; }
		public string SymbolPath { get; }
		public List<SourceFileInfo> SourceFiles { get; }

		public AddressInfo? GetLineAddress(SourceFileInfo file, int lineIndex)
		{
			foreach (var line in _linesByPrgAddress)
			{
				if (line.Value.File == file && line.Value.LineNumber == lineIndex)
					return new AddressInfo{Address = line.Key, Type = SnesMemoryType.PrgRom};
			}

			foreach (var line in _linesBySpcAddress) {
				if (line.Value.File == file && line.Value.LineNumber == lineIndex)
					return new AddressInfo { Address = line.Key, Type = SnesMemoryType.SpcRam };
			}

			return null;
		}

		public string GetSourceCodeLine(int prgRomAddress)
		{
			throw new NotImplementedException();
		}

		public SourceCodeLocation GetSourceCodeLineInfo(AddressInfo address)
		{
			if (address.Type == SnesMemoryType.PrgRom)
				return _linesByPrgAddress.TryGetValue(address.Address, out var location) ? location : null;
			if (address.Type == SnesMemoryType.SpcRam)
				return _linesBySpcAddress.TryGetValue(address.Address, out var location) ? location : null;
			return null;
		}

		private void GetLabelAddressAndType(int address, SnesMemoryType type, out int absoluteAddress, out SnesMemoryType? memoryType)
		{
			AddressInfo absAddress = DebugApi.GetAbsoluteAddress(new AddressInfo
				{Address = address, Type = type});
			absoluteAddress = absAddress.Address;
			if (absoluteAddress >= 0)
			{
				memoryType = absAddress.Type;
			}
			else
			{
				memoryType = null;
			}
		}

		public AddressInfo? GetSymbolAddressInfo(SourceSymbol symbol)
		{
			if (!symbol.Address.HasValue)
				return null;

			EmuApi.ElfSymData symData = (EmuApi.ElfSymData)symbol.InternalSymbol;
			SnesMemoryType memType = symData.Spc ? SnesMemoryType.SpcMemory : SnesMemoryType.CpuMemory;
			GetLabelAddressAndType(symbol.Address.Value, memType, out var labelAddress, out var addressType);
			if (addressType.HasValue)
			{
				return new AddressInfo {Address = labelAddress, Type = addressType.Value};
			}

			return null;
		}

		public SourceCodeLocation GetSymbolDefinition(SourceSymbol symbol)
		{
			if (!symbol.Address.HasValue)
				return null;

			EmuApi.ElfSymData symData = (EmuApi.ElfSymData)symbol.InternalSymbol;
			SnesMemoryType memType = symData.Spc ? SnesMemoryType.SpcMemory : SnesMemoryType.CpuMemory;
			GetLabelAddressAndType(symbol.Address.Value, memType, out var labelAddress, out var addressType);
			if (addressType == SnesMemoryType.PrgRom)
				return _linesByPrgAddress.TryGetValue(labelAddress, out var location) ? location : null;
			if (addressType == SnesMemoryType.SpcRam)
				return _linesBySpcAddress.TryGetValue(labelAddress, out var location) ? location : null;
			return null;
		}

		public SourceSymbol GetSymbol(CpuType cpuType, string word, int prgStartAddress, int prgEndAddress)
		{
			Dictionary<int, SourceSymbol> syms = cpuType == CpuType.Spc ? _symbolsBySpcAddress : _symbolsByPrgAddress;
			foreach (var sym in syms)
			{
				if (sym.Value.Name != word)
					continue;

				if (sym.Key >= prgStartAddress && sym.Key <= prgEndAddress)
					return sym.Value;

				int spanPrgOffset = sym.Key;
				EmuApi.ElfSymData symData = (EmuApi.ElfSymData) sym.Value.InternalSymbol;
				if (prgStartAddress < spanPrgOffset + symData.Size && prgEndAddress >= spanPrgOffset)
					return sym.Value;
			}

			return null;
		}

		public List<SourceSymbol> GetSymbols()
		{
			return _allSymbols;
		}

		public int GetSymbolSize(SourceSymbol srcSymbol)
		{
			EmuApi.ElfSymData symData = (EmuApi.ElfSymData) srcSymbol.InternalSymbol;
			return symData.Size;
		}

		private void CreateLabels()
		{
			DbgIntegrationConfig config = ConfigManager.Config.Debug.DbgIntegration;
			List<CodeLabel> labels = new List<CodeLabel>(_allSymbols.Count);
			foreach (var sym in _allSymbols)
			{
				EmuApi.ElfSymData symData = (EmuApi.ElfSymData)sym.InternalSymbol;
				SnesMemoryType memType = symData.Spc ? SnesMemoryType.SpcMemory : SnesMemoryType.CpuMemory;
				GetLabelAddressAndType(sym.Address.Value, memType, out var labelAddress, out var addressType);

				if (config.ImportCpuPrgRomLabels && addressType == SnesMemoryType.PrgRom ||
				    config.ImportCpuWorkRamLabels && addressType == SnesMemoryType.WorkRam ||
				    config.ImportSpcRamLabels && addressType == SnesMemoryType.SpcRam)
				{
					labels.Add(new CodeLabel
					{
						Address = (uint) labelAddress, Label = sym.Name,
						Length = 1, MemoryType = addressType.Value,
						Comment = String.Empty
					});
				}
			}

			if (config.ResetLabelsOnImport)
				DebugWorkspaceManager.ResetLabels();
			LabelManager.SetLabels(labels, true);
		}

		private void BuildCdlData(CpuType type)
		{
			int prgSize = DebugApi.GetMemorySize(type == CpuType.Spc ? SnesMemoryType.SpcRam : SnesMemoryType.PrgRom);
			if (prgSize <= 0)
				return;

			byte[] cdlFile = new byte[prgSize];

			List<EmuApi.ElfSec> secs = type == CpuType.Spc ? _elfSecsSpc : _elfSecsCpu;
			SnesMemoryType memType = type == CpuType.Spc ? SnesMemoryType.SpcMemory : SnesMemoryType.CpuMemory;
			foreach (var sec in secs)
			{
				GetLabelAddressAndType(sec.Address, memType, out var labelAddress, out var addressType);
				if (addressType != SnesMemoryType.PrgRom && addressType != SnesMemoryType.SpcRam)
					continue;

				if (sec.Name.Equals(".text") || sec.Name.Equals(".init"))
				{
					for (int i = 0; i < sec.Size; ++i)
						cdlFile[labelAddress + i] =
							(byte) CdlFlags.Code | (byte) CdlFlags.IndexMode8 | (byte) CdlFlags.MemoryMode8;
				}
				else if (sec.Name.Equals(".rodata"))
				{
					for (int i = 0; i < sec.Size; ++i)
						cdlFile[labelAddress + i] = (byte) CdlFlags.Data;
				}
			}

			DebugApi.SetCdlData(type, cdlFile, cdlFile.Length);
		}

		public void Integrate()
		{
			CreateLabels();
			BuildCdlData(CpuType.Cpu);
			BuildCdlData(CpuType.Spc);
		}
	}
}