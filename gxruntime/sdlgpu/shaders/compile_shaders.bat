@echo off
setlocal
set HERE=%~dp0
if "%DXC_EXE%"=="" set DXC_EXE=%HERE%..\..\..\tools\dxc-x64\dxc.exe
"%DXC_EXE%" -spirv -T vs_6_0 -E VSMain blit.hlsl -Fo blit_vs.spv || exit /b 1
"%DXC_EXE%" -spirv -T ps_6_0 -E PSMain blit.hlsl -Fo blit_ps.spv || exit /b 1
"%DXC_EXE%" -T vs_6_0 -E VSMain blit.hlsl -Fo blit_vs.dxil || exit /b 1
"%DXC_EXE%" -T ps_6_0 -E PSMain blit.hlsl -Fo blit_ps.dxil || exit /b 1
"%DXC_EXE%" -spirv -T vs_6_0 -E VSMain mesh.hlsl -Fo mesh_vs.spv || exit /b 1
"%DXC_EXE%" -spirv -T ps_6_0 -E PSMain mesh.hlsl -Fo mesh_ps.spv || exit /b 1
"%DXC_EXE%" -T vs_6_0 -E VSMain mesh.hlsl -Fo mesh_vs.dxil || exit /b 1
"%DXC_EXE%" -T ps_6_0 -E PSMain mesh.hlsl -Fo mesh_ps.dxil || exit /b 1
