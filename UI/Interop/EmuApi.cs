using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Mesen.GUI.Config;
using Mesen.GUI.Forms;
using Mesen.GUI.Debugger.Integration;

namespace Mesen.GUI
{

	public class EmuApi
	{
		public enum ConsoleId
		{
			Main = 0,
			HistoryViewer = 1
		}

		private const string DllPath = "MesenSCore.dll";
		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool TestDll();
		[DllImport(DllPath)] public static extern void InitDll();

		[DllImport(DllPath, EntryPoint = "GetMesenVersion")] private static extern UInt32 GetMesenVersionWrapper();
		public static Version GetMesenVersion()
		{
			UInt32 version = GetMesenVersionWrapper();
			UInt32 revision = version & 0xFF;
			UInt32 minor = (version >> 8) & 0xFF;
			UInt32 major = (version >> 16) & 0xFFFF;
			return new Version((int)major, (int)minor, (int)revision);
		}

		[DllImport(DllPath)] public static extern void SetMasterVolume(double volume, ConsoleId consoleId);
		[DllImport(DllPath)] public static extern void SetVideoScale(double scale, ConsoleId consoleId);

		[DllImport(DllPath)] public static extern IntPtr RegisterNotificationCallback(NotificationListener.NotificationCallback callback);
		[DllImport(DllPath)] public static extern void UnregisterNotificationCallback(IntPtr notificationListener);

		[DllImport(DllPath)] public static extern void InitializeEmu([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string homeFolder, IntPtr windowHandle, IntPtr dxViewerHandle, [MarshalAs(UnmanagedType.I1)]bool noAudio, [MarshalAs(UnmanagedType.I1)]bool noVideo, [MarshalAs(UnmanagedType.I1)]bool noInput);
		[DllImport(DllPath)] public static extern void Release();

		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool IsRunning();
		[DllImport(DllPath)] public static extern void Stop();

		[DllImport(DllPath)] public static extern void Reset();
		[DllImport(DllPath)] public static extern void PowerCycle();
		[DllImport(DllPath)] public static extern void ReloadRom();

		[DllImport(DllPath)] public static extern void Pause(ConsoleId consoleId = ConsoleId.Main);
		[DllImport(DllPath)] public static extern void Resume(ConsoleId consoleId = ConsoleId.Main);
		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool IsPaused(ConsoleId consoleId = ConsoleId.Main);

		[DllImport(DllPath)] public static extern void TakeScreenshot();

		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool LoadRom(
			[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string filepath,
			[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string patchFile = ""
		);

		[DllImport(DllPath, EntryPoint = "GetRomInfo")] private static extern void GetRomInfoWrapper(out InteropRomInfo romInfo);
		public static RomInfo GetRomInfo()
		{
			InteropRomInfo info;
			EmuApi.GetRomInfoWrapper(out info);
			return new RomInfo(info);
		}

		[DllImport(DllPath)] public static extern void LoadRecentGame([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string filepath, [MarshalAs(UnmanagedType.I1)]bool resetGame);

		[DllImport(DllPath)] public static extern void AddKnownGameFolder([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string folder);

		[DllImport(DllPath)] public static extern void SetFolderOverrides(
			[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string saveDataFolder,
			[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string saveStateFolder,
			[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string screenshotFolder
		);

		[DllImport(DllPath)] public static extern void SetDisplayLanguage(Language lang);
		[DllImport(DllPath)] public static extern void SetFullscreenMode([MarshalAs(UnmanagedType.I1)]bool fullscreen, IntPtr windowHandle, UInt32 monitorWidth, UInt32 monitorHeight);

		[DllImport(DllPath)] public static extern ScreenSize GetScreenSize([MarshalAs(UnmanagedType.I1)]bool ignoreScale, ConsoleId consoleId = ConsoleId.Main);

		[DllImport(DllPath, EntryPoint = "GetLog")] private static extern IntPtr GetLogWrapper();
		public static string GetLog() { return Utf8Marshaler.PtrToStringUtf8(EmuApi.GetLogWrapper()).Replace("\n", Environment.NewLine); }
		[DllImport(DllPath)] public static extern void WriteLogEntry([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string message);
		[DllImport(DllPath)] public static extern void DisplayMessage([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string title, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string message, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string param1 = null);

		[DllImport(DllPath)] public static extern IntPtr GetArchiveRomList([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string filename);

		[DllImport(DllPath)] public static extern void SaveState(UInt32 stateIndex);
		[DllImport(DllPath)] public static extern void LoadState(UInt32 stateIndex);
		[DllImport(DllPath)] public static extern void SaveStateFile([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string filepath);
		[DllImport(DllPath)] public static extern void LoadStateFile([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string filepath);

		[DllImport(DllPath, EntryPoint = "GetSaveStatePreview")] private static extern Int32 GetSaveStatePreviewWrapper([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))]string saveStatePath, [Out]byte[] imgData);
		public static Image GetSaveStatePreview(string saveStatePath)
		{
			if(File.Exists(saveStatePath)) {
				byte[] buffer = new byte[512*478*4];
				Int32 size = EmuApi.GetSaveStatePreviewWrapper(saveStatePath, buffer);
				if(size > 0) {
					Array.Resize(ref buffer, size);
					using(MemoryStream stream = new MemoryStream(buffer)) {
						return Image.FromStream(stream);
					}
				}
			}
			return null;
		}

		[DllImport(DllPath)] public static extern void SetCheats([In]UInt32[] cheats, UInt32 cheatCount);
		[DllImport(DllPath)] public static extern void ClearCheats();


		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void GetDwarfInfoObjectCountCallback(UInt32 numSecs, UInt32 numFiles, UInt32 numLocs, UInt32 numSyms);

		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void GetDwarfInfoAppendSecCallback(IntPtr secNamePtr, UInt32 addr, UInt32 size);

		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void GetDwarfInfoAppendFileCallback(IntPtr filePathPtr);

		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void GetDwarfInfoAppendLocCallback(UInt32 fileIdx, UInt32 line, UInt32 addr);

		[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
		public delegate void GetDwarfInfoAppendSymCallback(IntPtr namePtr, UInt32 secIdx, UInt32 addr, UInt32 size);

		[DllImport(DllPath, EntryPoint = "GetDwarfInfo")]
		public static extern UInt32 GetDwarfInfoWrapper(
			GetDwarfInfoObjectCountCallback countCb,
			GetDwarfInfoAppendSecCallback appendSecCb,
			GetDwarfInfoAppendFileCallback appendFileCb,
			GetDwarfInfoAppendLocCallback appendLocCb,
			GetDwarfInfoAppendSymCallback appendSymCb);


		public struct ElfSec
		{
			public string Name;
			public int Address;
			public int Size;
		}

		public struct ElfSymData
		{
			public ElfSec Section;
			public int Size;
		}
		
		public struct DwarfInfo
		{
			public DateTime SymbolFileStamp;
			public string SymbolPath;
			public List<ElfSec> Sections;
			public List<SourceFileInfo> SourceFiles;
			public Dictionary<int, SourceCodeLocation> LinesByCpuAddress;
			public Dictionary<int, SourceSymbol> SymbolsByCpuAddress;
		}

		private static string[] ReadSourceFileOrEmpty(string filePath)
		{
			try
			{
				return File.ReadAllLines(filePath, Encoding.UTF8);
			}
			catch (Exception)
			{
				WriteLogEntry("[EmuApi] Unable to read source file: " + filePath);
				return new string[0];
			}
		}

		public static DwarfInfo? GetDwarfInfo()
		{
			List<ElfSec> secs = new List<ElfSec>();
			List<SourceFileInfo> files = new List<SourceFileInfo>();
			Dictionary<int, SourceCodeLocation> locs = new Dictionary<int, SourceCodeLocation>();
			Dictionary<int, SourceSymbol> syms = new Dictionary<int, SourceSymbol>();

			if (GetDwarfInfoWrapper((UInt32 numSecs, UInt32 numFiles, UInt32 numLocs, UInt32 numSyms) =>
				{
					secs = new List<ElfSec>((int) numSecs);
					files = new List<SourceFileInfo>((int) numFiles);
					locs = new Dictionary<int, SourceCodeLocation>((int) numLocs);
					syms = new Dictionary<int, SourceSymbol>((int) numSyms);
				},
				(IntPtr secNamePtr, UInt32 addr, UInt32 size) =>
				{
					string secName = Utf8Marshaler.GetStringFromIntPtr(secNamePtr);
					secs.Add(new ElfSec
					{
						Name = secName,
						Address = (int) addr,
						Size = (int) size
					});
				},
				(IntPtr filePathPtr) =>
				{
					string filePath = Utf8Marshaler.GetStringFromIntPtr(filePathPtr);
					files.Add(new SourceFileInfo
					{
						Name = filePath,
						Data = ReadSourceFileOrEmpty(filePath)
					});
				},
				(UInt32 fileIdx, UInt32 line, UInt32 addr) =>
				{
					locs[(int) addr] = new SourceCodeLocation
					{
						File = files[(int) fileIdx],
						LineNumber = Math.Max(0, (int) line - 1)
					};
				},
				(IntPtr namePtr, UInt32 secIdx, UInt32 addr, UInt32 size) =>
				{
					syms[(int) addr] = new SourceSymbol
					{
						Name = Utf8Marshaler.GetStringFromIntPtr(namePtr),
						Address = (int) addr,
						InternalSymbol = new ElfSymData{Section = secs[(int) secIdx], Size = (int) size}
					};
				}) == 1)
			{
				string path = GetRomInfo().RomPath;
				return new DwarfInfo
				{
					SymbolFileStamp = File.GetLastWriteTime(path),
					SymbolPath = path,
					Sections = secs,
					SourceFiles = files,
					LinesByCpuAddress = locs,
					SymbolsByCpuAddress = syms
				};
			}
			else
			{
				return null;
			}
		}
	}

	public struct ScreenSize
	{
		public Int32 Width;
		public Int32 Height;
		public double Scale;
	}

	public struct InteropRomInfo
	{
		public IntPtr RomPath;
		public IntPtr PatchPath;
		public CoprocessorType CoprocessorType;
		public SnesCartInformation Header;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 40)]
		public byte[] Sha1;
	}

	public struct RomInfo
	{
		public string RomPath;
		public string PatchPath;
		public CoprocessorType CoprocessorType;
		public SnesCartInformation Header;
		public string Sha1;

		public RomInfo(InteropRomInfo romInfo)
		{
			RomPath = (ResourcePath)Utf8Marshaler.GetStringFromIntPtr(romInfo.RomPath);
			PatchPath = (ResourcePath)Utf8Marshaler.GetStringFromIntPtr(romInfo.PatchPath);
			Header = romInfo.Header;
			CoprocessorType = romInfo.CoprocessorType;
			Sha1 = Encoding.UTF8.GetString(romInfo.Sha1);
		}

		public string GetRomName()
		{
			return Path.GetFileNameWithoutExtension(((ResourcePath)RomPath).FileName);
		}
	}

	public struct SnesCartInformation
	{
		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
		public byte[] MakerCode;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
		public byte[] GameCode;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 7)]
		public byte[] Reserved;

		public byte ExpansionRamSize;
		public byte SpecialVersion;
		public byte CartridgeType;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 21)]
		public byte[] CartName;

		public byte MapMode;
		public byte RomType;
		public byte RomSize;
		public byte SramSize;

		public byte DestinationCode;
		public byte Reserved2;
		public byte Version;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
		public byte[] ChecksumComplement;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
		public byte[] Checksum;

		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 0x20)]
		public byte[] CpuVectors;
	}

	public enum CoprocessorType
	{
		None,
		DSP1,
		DSP1B,
		DSP2,
		DSP3,
		DSP4,
		GSU,
		OBC1,
		SA1,
		SDD1,
		RTC,
		Satellaview,
		SPC7110,
		ST010,
		ST011,
		ST018,
		CX4,
		Gameboy,
		SGB
	}

	public static class CoprocessorTypeExtensions
	{
		public static CpuType? ToCpuType(this CoprocessorType type)
		{
			switch(type) {
				case CoprocessorType.CX4: 
					return CpuType.Cx4;

				case CoprocessorType.DSP1: case CoprocessorType.DSP1B: case CoprocessorType.DSP2: case CoprocessorType.DSP3: case CoprocessorType.DSP4: 
					return CpuType.NecDsp;

				case CoprocessorType.SA1:
					return CpuType.Sa1;

				case CoprocessorType.GSU:
					return CpuType.Gsu;

				case CoprocessorType.Gameboy: 
				case CoprocessorType.SGB: 
					return CpuType.Gameboy;

				default:
					return null;
			}
		}
	}

	public enum FirmwareType
	{
		CX4,
		DSP1,
		DSP1B,
		DSP2,
		DSP3,
		DSP4,
		ST010,
		ST011,
		ST018,
		Satellaview,
		Gameboy,
		GameboyColor,
		Sgb1GameboyCpu,
		Sgb2GameboyCpu,
		SGB1,
		SGB2,
	}

	public struct MissingFirmwareMessage
	{
		public IntPtr Filename;
		public FirmwareType Firmware;
		public UInt32 Size;
	}
}
