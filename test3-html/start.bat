@echo off
taskkill /F /IM java.exe 2>nul
set HUAWEICLOUD_SDK_AK=JpadGfUK
set HUAWEICLOUD_SDK_SK=hDDms2ZYfrMfXvRpqgfUW2tJqnSUya8D
"C:\Program Files\apache-maven-3.9.16\bin\mvn.cmd" exec:java -Dexec.mainClass="CarCloudServer"
pause