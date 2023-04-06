# About
Content of this folder will be used to create prepkg archive. This README file will not be included.

# How to make tools and why where is no sources for them

## pkg-linux

On linux it's easy! Get PSL1GHT from ps3dev repo: 
    https://github.com/ps3dev/PSL1GHT/

Enter in /tools/ps3py run make (make sure You have Python 2.7) to build pkgcrypt and then bundle in single file using PyInstaller 3.6:
```sh
$ python2 -m PyInstaller -F pkg.py
```
You will end up with `./dist/pkg`

## pkg-win64
On Windows you need to pass eight gates of hell to make it.

### List of things You need:
    
- Python 2.7
- Visual Studio 2008 (you may use Visual Studio 2008 Express With SP1 as I did)
- Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1
- Microsoft Visual C++ Compiler for Python 2.7

Main problem is Microsoft loves to delete old things from their site.
I can not share Microsoft products even they was free back in the day, I think. So here is hashes to things you looking for.

- Visual Studio 2008 Express With SP1 \
  Install only Visual C++ 2008 and uncheck Silverlight Runtime and SQL Server 2008 during its installation\
  File: VS2008ExpressWithSP1ENUX1504728.ISO \
  SHA1: 0x937BD4A1B299505F286E371B3A5212B9098C38F2 \
- Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1 \
  During installation at Installation Option uncheck everything except `Developer Tools > Visual C++ Compilers` \
  File: GRMSDKX_EN_DVD.ISO \
  SHA1: 0x3393C98B8468CB3505557854922707510F8B65E1
- Microsoft Visual C++ Compiler for Python 2.7 \
  File: VCForPython27.msi \
  SHA1: 0x7800D037BA962F288F9B952001106D35EF57BEFE

Once you get all things needed close PSL1GHT repo, enter in /tools/ps3py and open cmd and run something like this:
```
> "C:\Users\username\AppData\Local\Programs\Common\Microsoft\Visual C++ for Python\9.0\VC\bin\vcvars64.bat"
```
If everything okey You will get following output:
```
Setting environment for using Microsoft Visual Studio 2008 x64 tools.
```

Now edit crypto.c:
- Define macros for uint64_t and uint_8 (I not found header for them in Visual C++ 2008)
  ```dsdsd
  #define uint8_t unsigned char
  #define uint64_t unsigned long long
  ```
- Move all definitions of variables in middle of function `pkg_crypt` to it's begining:
    - int bytes_to_dump;
    - uint8_t *outHash;
    - int outHash_length;
    - PyObject *py_ret;

And only then run
```
> C:\Python27\python.exe setup.py build_ext
> copy .\build\lib.win-amd64-2.7\pkgcrypt.pyd pkgcrypt.pyd
```
After this point all left to do is install PyInstaller 3.6 and create bundle:
```
> C:\Python27\python.exe -m pip install pyinstaller==3.6
> C:\Python27\python.exe PyInstaller -F pkg.py 
```
This will create `.\dist\pkg.exe`