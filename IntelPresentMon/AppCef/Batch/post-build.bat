@echo off
echo Copy CEF binaries...
xcopy /SY "Cef\Bin\" "%~2"

echo Copy CEF resources...
xcopy /SY "Cef\Resources" "%~2"

if "%~1"=="Release" (
	echo "Build Web Assets..."
	call "%~dp0build-web.bat"
)

echo Copy web resources...
xcopy /SY "Web\dist" "%~2Web\"

echo Copy presets...
xcopy /SY "Web\presets" "%~2Presets\"

echo Copy block list...
xcopy /SY "Web\BlockLists" "%~2BlockLists\"

if "%4"=="uiAccessOn" (
	echo "Sign executable..."
	echo SignTool sign /a /v /s PrivateCertStore /n "Test Certificate - For Internal Use Only" /t http://timestamp.comodoca.com/authenticode /fd sha1 "%~2%~3"
	SignTool sign /a /v /s PrivateCertStore /n "Test Certificate - For Internal Use Only" /t http://timestamp.comodoca.com/authenticode /fd sha1 "%~2%~3"
)
