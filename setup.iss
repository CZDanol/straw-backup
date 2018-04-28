; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "Straw Backup"
#define MyAppVersion "1.1.0.0"
#define MyAppPublisher "Straw Solutions"
#define MyAppCopyright "(c) 2018 Straw Solutions, Daniel �ejchan"
#define MyAppURL "http://straw-solutions.cz"
#define MyAppExeName "atisExporter.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{72F98CFA-1491-4BFC-8432-4D196E8F9359}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyRight={#MyAppCopyright}
DisableProgramGroupPage=yes
OutputDir=setup
OutputBaseFilename=atisExporter_win32
Compression=lzma
SolidCompression=yes
UninstallDisplayIcon="{app}\{#MyAppExeName}"
; Sign tools configured in inno setup ide
SignTool=ComodoSign
SignedUninstaller=yes

[Languages]
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
Source: "installDeps\vc2015_redist.x86.exe"; DestDir: {tmp}; AfterInstall: InstallVc2015Redist; Flags: deleteafterinstall
Source: "bin\atisExporter.exe"; DestDir: "{app}"; Flags: ignoreversion sign
Source: "bin\pandoc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "bin\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "bin\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "bin\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Dirs]

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
procedure InstallVc2015Redist;
var
  StatusText: string;
  ResultCode: integer;
begin
  StatusText := WizardForm.StatusLabel.Caption;
  WizardForm.StatusLabel.Caption := 'Instaluji Microsoft Visual Studio 2015 Redistributable...';
  WizardForm.ProgressGauge.Style := npbstMarquee;
  try
    if not Exec(ExpandConstant('{tmp}\vc2015_redist.x86.exe'), '/install /passive /norestart', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode)
    then
      MsgBox('Nepoda�ilo se spustit instal�tor MSVC 2015 Redist. Aplikace PhotoBox bez t�to knihovny nemus� fungovat.' + #13#10 + SysErrorMessage(ResultCode), mbError, MB_OK);
  finally
    WizardForm.StatusLabel.Caption := StatusText;
    WizardForm.ProgressGauge.Style := npbstNormal;
  end;
end;

