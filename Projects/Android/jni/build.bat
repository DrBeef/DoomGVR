call ndk-build V=0 -j10 NDK_DEBUG=0 %1

cd ..\libs

del libs.jar
mkdir lib
mkdir lib\armeabi-v7a
copy .\armeabi-v7a\ lib\armeabi-v7a\*
7z a -x!*.jar libs.zip .\lib*
rename libs.zip libs.jar

REM Create an archive of the source
cd ..\src\main\assets\source
del DVRSource.zip
7z a -r -x!.git* -x!*.o -x!*.d -x!obj -x!*.bin -x!app\build -x!app\libs -x!*.jar -x!*.so -x!*.log -x!*.jks -x!*.apk DVRSource.zip ..\..\..\..\..\*
cd ..\..\..\..\jni
