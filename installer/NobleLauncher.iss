#define MyAppName "Noble Launcher"
#define MyAppVersion "2.9.0"
#define MyAppPublisher "Noble Launcher"
#define MyAppExeName "NobleLauncher.exe"

[Setup]
AppId={{A9C1E3F2-6D74-4F2B-9C41-2F4C9B1A0001}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\NobleLauncher
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\release
OutputBaseFilename=NobleLauncherSetup-{#MyAppVersion}
SetupIconFile=..\assets\logo.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
RestartApplications=no
AllowNoIcons=yes
VersionInfoVersion=2.9.0.0
VersionInfoDescription=Noble Launcher 2.9.0 (C++/Qt)
VersionInfoProductName=Noble Launcher
VersionInfoProductVersion=2.9.0.0

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\.env"; DestDir: "{app}"; Flags: onlyifdoesntexist skipifsourcedoesntexist
Source: "..\.env.example"; DestDir: "{app}"; Flags: onlyifdoesntexist skipifsourcedoesntexist
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\Noble Launcher"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Noble Launcher"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,Noble Launcher}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
