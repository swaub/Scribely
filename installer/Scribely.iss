; Scribely — Inno Setup 6 script
;
; Produces a small (~600 KB) per-user installer. It installs only the launcher
; (Scribely.exe); the app downloads its helpers and models (yt-dlp, FFmpeg,
; whisper.cpp, llama.cpp, the Whisper + Qwen models) on first run into the
; per-user %LOCALAPPDATA%\Scribely store (system-wide ffmpeg/yt-dlp copies
; are detected and reused instead when available).
;
; Build:  "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\Scribely.iss
; Output: installer\Output\Scribely-Setup-2.0.0.exe

#define AppName     "Scribely"
#define AppVersion  "2.0.0"
#define AppPublisher "swaub"
#define AppURL      "https://github.com/swaub/Scribely"
#define AppExe      "Scribely.exe"

[Setup]
AppId={{5D8A1C42-7F0B-4E9D-A6C3-91B2E4F7D058}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=Output
OutputBaseFilename=Scribely-Setup-{#AppVersion}
SetupIconFile=..\res\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\{#AppExe}";         DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE";                DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";                        Filename: "{app}\{#AppExe}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";                  Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
{ Components and models live in the per-user %LOCALAPPDATA%\Scribely store
  (legacy installs may still hold them beside the exe).  On uninstall, offer
  to remove that data; default to keeping it so a reinstall does not
  re-download ~2 GB. }
procedure RemoveStore(Root: string);
begin
  DelTree(Root + '\bin', True, True, True);
  DelTree(Root + '\models', True, True, True);
  DelTree(Root + '\output', True, True, True);
  DelTree(Root + '\temp', True, True, True);
  DeleteFile(Root + '\installed.cfg');
  DeleteFile(Root + '\settings.cfg');
  DeleteFile(Root + '\summ_perf.cfg');
end;

procedure CurUninstallStepChanged(CurStep: TUninstallStep);
var
  Store: string;
begin
  if CurStep = usUninstall then
  begin
    Store := ExpandConstant('{localappdata}') + '\Scribely';
    if MsgBox('Also delete the downloaded components, models and transcripts in '
        + Store + '?' + #13#10#13#10
        + 'Choose No to keep them - useful if you plan to reinstall, since it'
        + ' avoids re-downloading about 2 GB.',
        mbConfirmation, MB_YESNO) = IDYES then
    begin
      RemoveStore(Store);
      RemoveDir(Store);
      RemoveStore(ExpandConstant('{app}'));
    end;
  end;
end;
