build:
	ndk-build NDK_DEBUG=1 V=1

clean:
	ndk-build clean
run:
	adb push libs/armeabi/andhookxx /data/local/tmp
	adb shell su -c "/data/local/tmp/andhookxx"

debug:
	adb push libs/armeabi/andhookxx /data/local/tmp
	adb push libs/armeabi/gdbserver /data/local/tmp
	adb shell "su -c /data/local/tmp/gdbserver&"
