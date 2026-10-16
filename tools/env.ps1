# Dot-source this in any shell:  . C:\CLAUDE\TOOLS\env.ps1
$T = 'C:\CLAUDE\TOOLS'
$jdk   = (Get-ChildItem "$T\jdk21" -Directory | Select -First 1).FullName
$node  = (Get-ChildItem "$T\node" -Directory | Select -First 1).FullName
$llvm  = (Get-ChildItem "$T\llvm-mingw" -Directory | Select -First 1).FullName
$ghid  = (Get-ChildItem "$T\ghidra" -Directory | Select -First 1).FullName
$env:JAVA_HOME = $jdk
$env:GHIDRA_INSTALL_DIR = $ghid
$env:NPM_CONFIG_PREFIX = "$T\npm-global"
$env:PATH = "$jdk\bin;$node;$T\npm-global;$llvm\bin;$ghid\support;$env:PATH"
