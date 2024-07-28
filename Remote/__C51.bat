@echo off
::This file was created automatically by CrossIDE to compile with C51.
D:
cd "\Course Work\Second Year\ELEC 291\Project2\EFM8\LCD\"
"D:\CrossIDE\CrossIDE\Call51\Bin\c51.exe" --use-stdout  "D:\Course Work\Second Year\ELEC 291\Project2\EFM8\LCD\main.c"
if not exist hex2mif.exe goto done
if exist main.ihx hex2mif main.ihx
if exist main.hex hex2mif main.hex
:done
echo done
echo Crosside_Action Set_Hex_File D:\Course Work\Second Year\ELEC 291\Project2\EFM8\LCD\main.hex
