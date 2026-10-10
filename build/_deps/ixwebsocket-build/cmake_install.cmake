# Install script for directory: D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/MusicPadDaemon")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "D:/msys64/ucrt64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/libixwebsocket.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ixwebsocket" TYPE FILE FILES
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXBase64.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXBench.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXCancellationRequest.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXConnectionState.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXDNSLookup.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXExponentialBackoff.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXGetFreePort.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXGzipCodec.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXHttp.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXHttpClient.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXHttpServer.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXNetSystem.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXProgressCallback.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterrupt.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptFactory.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptPipe.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSelectInterruptEvent.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSetThreadName.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSocket.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketConnect.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketFactory.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketServer.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXSocketTLSOptions.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXStrCaseCompare.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUdpSocket.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUniquePtr.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUrlParser.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUuid.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUtf8Validator.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXUserAgent.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocket.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseConstants.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketCloseInfo.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketErrorInfo.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshake.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHandshakeKeyGen.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketHttpHeaders.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketInitResult.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessage.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketMessageType.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketOpenInfo.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflate.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateCodec.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketPerMessageDeflateOptions.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketProxyServer.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketSendData.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketSendInfo.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketServer.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketTransport.h"
    "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-src/ixwebsocket/IXWebSocketVersion.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/ixwebsocket-config.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/ixwebsocket.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake"
         "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket/ixwebsocket-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/ixwebsocket" TYPE FILE FILES "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/CMakeFiles/Export/dbc99e06a99e696141dafd40631f8060/ixwebsocket-targets-noconfig.cmake")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/AndroidStudioProjects/MusicPad-C++/build/_deps/ixwebsocket-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
