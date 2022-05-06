workspace "Katachi"
   architecture "x86_64"
   startproject "Katachi"

   configurations { "Debug", "Developer", "Release" }

project "Katachi"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"

   targetdir ("%{wks.location}/bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}")
   objdir ("%{wks.location}/bin/int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")

   files { "Kr/**.h", "Kr/**.cpp", "*.cpp", "*.h", "SHA1/*.h", "SHA1/*.cpp" }

   ignoredefaultlibraries { "MSVCRT" }
   defines { "NETWORK_OPENSSL_ENABLE" }

   filter "configurations:Debug"
      defines { "DEBUG", "BUILD_DEBUG" }
      symbols "On"
      runtime "Debug"

   filter "configurations:Developer"
      defines { "NDEBUG", "BUILD_DEVELOPER" }
      optimize "On"
      runtime "Release"

   filter "configurations:Release"
      defines { "NDEBUG", "BUILD_RELEASE" }
      optimize "On"
      runtime "Release"

   filter "system:linux"
   		links { "ssl", "crypto" }

   filter "system:macosx"
   		links { "ssl", "crypto" }

   filter "system:windows"
      systemversion "latest"
      files { "Kr/**.natvis" }
      defines { "_CRT_SECURE_NO_WARNINGS" }
      includedirs { "OpenSSL/include" }
