@echo off
set "SCRIPT_DIR=%~dp0"
docker run --rm -v "%SCRIPT_DIR%:/build" --network=host devkitpro/devkitarm 3dslink /build/3dsAppy.3dsx -a 192.168.1.104
arm-none-eabi-objdump -S -l 3dsAppy.elf > out.txt