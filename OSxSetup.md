---

# AAMP on Mac OS X

This document contains the instructions to setup and debug stand alone AAMP (aamp-cli) on Mac OS X.

## Install dependancies

**1. Install XCode from the Apple Store**

**2. Install XCode Command Line Tools**

This is required for MacOS version < 10.15

```
xcode-select --install
sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_<version>.pkg -target /
```

For MacOS 10.15 & above, we can check the SDK install path as
```
xcrun --sdk macosx --show-sdk-path
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk
```

**3. Install GStreamer packages**

Install Homebrew, if not available in your Mac:
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

Install gst packages:
```
brew install gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav
```

You can find help [here](https://wesleyli.co/2016/10/running-gstreamer-on-mac-os-x).
More details about packages available at [freedesktop.org](https://gstreamer.freedesktop.org/documentation/installing/on-mac-osx.html)

**4. Install [Cmake](https://cmake.org/download/)**

like cmake-3.18.0-Darwin-x86_64.dmg or latest

Set link for CMake command line.
``` ln -s /Applications/CMake.app/Contents/bin/cmake /usr/local/bin```

**5. Install OpenSsl**

```
brew install openssl
brew reinstall openssl		//if already installed
sudo ln -s /usr/local/Cellar/openssl\@1.1/1.1.1g /usr/local/ssl
```
Here 1.1.1g is the version 

**6. Install libXML2**

```
brew install libxml2
ln -s /usr/local/opt/libxml2/lib/pkgconfig/* /usr/local/lib/pkgconfig/
```
**7. Install libdash**

```
source install_libdash.sh
```
or
	
***Build***:

```
git clone git://github.com/bitmovin/libdash.git
cd libdash/libdash
git checkout stable_3_0
```

Apply patches downloaded from (**patch -p1 < file.patch**):
```https://code.rdkcentral.com/r/plugins/gitiles/components/generic/rdk-oe/meta-rdk-ext/+/rdk-next/recipes-multimedia/libdash/libdash/```

```
mkdir build
cd build
cmake ..
make
```

***Install***:

```
cp bin/libdash.dylib /usr/local/lib/
mkdir /usr/local/include
mkdir /usr/local/include/libdash
mkdir /usr/local/include/libdash/xml
mkdir /usr/local/include/libdash/mpd
mkdir /usr/local/include/libdash/helpers
mkdir /usr/local/include/libdash/network
mkdir /usr/local/include/libdash/portable
mkdir /usr/local/include/libdash/metrics
cp -pr ../libdash/include/*.h /usr/local/include/libdash
cp -pr ../libdash/source/xml/*.h /usr/local/include/libdash/xml
cp -pr ../libdash/source/mpd/*.h /usr/local/include/libdash/mpd
cp -pr ../libdash/source/network/*.h /usr/local/include/libdash/network
cp -pr ../libdash/source/portable/*.h /usr/local/include/libdash/portable
cp -pr ../libdash/source/helpers/*.h /usr/local/include/libdash/helpers
cp -pr ../libdash/source/metrics/*.h /usr/local/include/libdash/metrics

echo -e 'prefix=/usr/local \nexec_prefix=${prefix} \nlibdir=${exec_prefix}/lib \nincludedir=${prefix}/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}'  > /usr/local/lib/pkgconfig/libdash.pc
```
**8. Install libuuid**

```
brew install ossp-uuid
```

**9. Install cjson

```
brew install cjson
```

**10. Install aampabr**

```
git clone https://code.rdkcentral.com/r/rdk/components/generic/aampabr aampabr
cd aampabr
mkdir build
cd build
cmake ..
make
make install
```
##Build and execute aamp-cli
**1. Open aamp.xcodeproj in Xcode**

```
git clone "https://code.rdkcentral.com/r/rdk/components/generic/aamp" -b dev_sprint
```

**2. Build the code**

```
	Product -> Build
```

**3. Select target to execute**

```
	Product -> Scheme -> Edit scheme
	Run page-> Info
	Select Executable -> Other and open the ‘aamp-cli’ image name from {AAMP_PATH}/build/aamp_cli
```
**4. Execute**

```
Product -> Run
```
