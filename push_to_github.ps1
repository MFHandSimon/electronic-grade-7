# 把本目录推送到 GitHub
#
# 用法（在 PowerShell 里，先 cd 到 小车控制 目录）：
#   .\push_to_github.ps1 -RepoUrl "https://github.com/<你的用户名>/<仓库名>.git"
#
# 首次推送会弹出 GitHub 登录窗口（Git Credential Manager），登录一次即可，
# 之后本机自动记住，不用再输密码。

param(
    [Parameter(Mandatory = $true)]
    [string]$RepoUrl
)

$ErrorActionPreference = 'Stop'

# 确认当前目录是 git 仓库
if (-not (Test-Path -LiteralPath ".git")) {
    throw "当前目录不是 git 仓库。请先 cd 到 小车控制 目录再运行本脚本。"
}

$existing = git remote get-url origin 2>$null
if ($LASTEXITCODE -eq 0 -and $existing) {
    Write-Host "已存在 origin：$existing，改为 $RepoUrl"
    git remote set-url origin $RepoUrl
} else {
    git remote add origin $RepoUrl
}

Write-Host "当前分支："
git branch --show-current

Write-Host "`n开始推送……（如弹出 GitHub 登录窗口，请完成登录）`n"
git push -u origin main

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n推送成功。仓库地址：$RepoUrl" -ForegroundColor Green
} else {
    Write-Host "`n推送失败。常见原因：" -ForegroundColor Yellow
    Write-Host "  1. 仓库是新建的空仓库但勾选了「初始化 README」，远程已有提交 → 先 git pull --rebase origin main"
    Write-Host "  2. 仓库名或用户名填错 → 检查 $RepoUrl"
    Write-Host "  3. 网络问题 → 试 ssh 方式：git@github.com:<用户名>/<仓库名>.git"
}
