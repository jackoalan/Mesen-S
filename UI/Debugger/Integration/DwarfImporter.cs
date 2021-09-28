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
		private readonly List<EmuApi.ElfSec> _elfSecs;
		private readonly Dictionary<int, SourceCodeLocation> _linesByPrgAddress;
		private readonly Dictionary<int, SourceSymbol> _symbolsByCpuAddress;
		private readonly Dictionary<int, SourceSymbol> _symbolsByPrgAddress;

		public DwarfImporter(EmuApi.DwarfInfo info)
		{
			SymbolFileStamp = info.SymbolFileStamp;
			SymbolPath = info.SymbolPath;
			_elfSecs = info.Sections;
			SourceFiles = info.SourceFiles;

			_linesByPrgAddress = new Dictionary<int, SourceCodeLocation>(info.LinesByCpuAddress.Count);
			foreach (var line in info.LinesByCpuAddress)
			{
				GetRamLabelAddressAndType(line.Key, out var labelAddress, out var addressType);
				if (addressType.Value != SnesMemoryType.PrgRom)
					continue;
				_linesByPrgAddress.Add(labelAddress, line.Value);
			}

			_symbolsByCpuAddress = info.SymbolsByCpuAddress;
			_symbolsByPrgAddress = new Dictionary<int, SourceSymbol>(info.SymbolsByCpuAddress.Count);
			foreach (var sym in info.SymbolsByCpuAddress)
			{
				GetRamLabelAddressAndType(sym.Key, out var labelAddress, out var addressType);
				if (addressType.Value != SnesMemoryType.PrgRom)
					continue;
				_symbolsByPrgAddress.Add(labelAddress, sym.Value);
			}
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

			return null;
		}

		public string GetSourceCodeLine(int prgRomAddress)
		{
			return _linesByPrgAddress.TryGetValue(prgRomAddress, out var location)
				? location.File.Data[location.LineNumber]
				: null;
		}

		public SourceCodeLocation GetSourceCodeLineInfo(AddressInfo address)
		{
			if (address.Type != SnesMemoryType.PrgRom)
				return null;
			return _linesByPrgAddress.TryGetValue(address.Address, out var location) ? location : null;
		}

		private void GetRamLabelAddressAndType(int address, out int absoluteAddress, out SnesMemoryType? memoryType)
		{
			AddressInfo absAddress = DebugApi.GetAbsoluteAddress(new AddressInfo
				{Address = address, Type = SnesMemoryType.CpuMemory});
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

			GetRamLabelAddressAndType(symbol.Address.Value, out var labelAddress, out var addressType);
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

			GetRamLabelAddressAndType(symbol.Address.Value, out var labelAddress, out var addressType);
			if (addressType != SnesMemoryType.PrgRom)
				return null;
			
			return _linesByPrgAddress.TryGetValue(labelAddress, out var location) ? location : null;
		}

		public SourceSymbol GetSymbol(string word, int prgStartAddress, int prgEndAddress)
		{
			foreach (var sym in _symbolsByPrgAddress)
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
			return _symbolsByCpuAddress.Values.ToList();
		}

		public int GetSymbolSize(SourceSymbol srcSymbol)
		{
			EmuApi.ElfSymData symData = (EmuApi.ElfSymData) srcSymbol.InternalSymbol;
			return symData.Size;
		}

		private void CreateLabels()
		{
			DbgIntegrationConfig config = ConfigManager.Config.Debug.DbgIntegration;
			List<CodeLabel> labels = new List<CodeLabel>(_symbolsByCpuAddress.Count);
			foreach (var sym in _symbolsByCpuAddress)
			{
				GetRamLabelAddressAndType(sym.Key, out var labelAddress, out var addressType);
				EmuApi.ElfSymData symData = (EmuApi.ElfSymData) sym.Value.InternalSymbol;

				if (config.ImportCpuPrgRomLabels && addressType == SnesMemoryType.PrgRom ||
				    config.ImportCpuWorkRamLabels && addressType == SnesMemoryType.WorkRam)
				{
					labels.Add(new CodeLabel
					{
						Address = (uint) labelAddress, Label = sym.Value.Name,
						Length = 1, MemoryType = addressType.Value,
						Comment = String.Empty
					});
				}
			}

			if (config.ResetLabelsOnImport)
				DebugWorkspaceManager.ResetLabels();
			LabelManager.SetLabels(labels, true);
		}

		private void BuildCdlData()
		{
			int prgSize = DebugApi.GetMemorySize(SnesMemoryType.PrgRom);
			if (prgSize <= 0)
				return;

			byte[] cdlFile = new byte[prgSize];

			foreach (var sec in _elfSecs)
			{
				GetRamLabelAddressAndType(sec.Address, out var labelAddress, out var addressType);
				if (addressType != SnesMemoryType.PrgRom)
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

			DebugApi.SetCdlData(CpuType.Cpu, cdlFile, cdlFile.Length);
		}

		public void Integrate()
		{
			CreateLabels();
			BuildCdlData();
		}
	}
}