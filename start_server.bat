@echo off
chcp 65001 >nul
echo ========================================
echo    OTA 服务器启动脚本
echo ========================================
echo.

echo [1] 检查 Python 环境...
python --version
if errorlevel 1 (
    echo [错误] Python 未安装或不在 PATH 中
    pause
    exit /b 1
)

echo.
echo [2] 检查 Flask 安装...
python -c "import flask; print('Flask 版本:', flask.__version__)"
if errorlevel 1 (
    echo [警告] Flask 未安装，正在安装...
    python -m pip install flask flask-cors
)

echo.
echo [3] 进入服务器目录...
cd /d "%~dp0\server"

echo.
echo [4] 启动服务器...
echo 服务器将在 http://localhost:5000 上运行
echo 按 Ctrl+C 停止服务器
echo.
python server.py

pause
