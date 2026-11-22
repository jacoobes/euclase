add_rules("mode.debug", "mode.release")

target("solid-waffle")
    set_kind("static")
    add_files("solid_str.c")
    add_files("gc.c")
    add_files("table.c")
    add_files("memory.c")
    add_files('reader.c')
    add_files("vm.c")
    if is_plat("linux", "macosx") then
         add_syslinks("readline")
    end
    add_defines("DEBUG_STRESS_GC")
    add_defines("DEBUG_LOG_GC")
    add_defines("DEBUG")
    add_defines("DEBUG_COMPILER")


target("main")
    set_kind("binary")
    add_files('compiler.c')
    add_files("main.c")
    set_languages("c11")
    add_deps("solid-waffle")
    add_defines("DEBUG_STRESS_GC")
    add_defines("DEBUG_LOG_GC")
    add_defines("DEBUG")
    add_defines("DEBUG_COMPILER")


target('asm')
    set_kind("binary")
    add_deps("solid-waffle")
    add_files("asm.c")
    add_defines("DEBUG")

target("testreader")
    set_kind("binary")
    add_files("test/test_reader.c")
    set_languages("c11")
    add_deps("solid-waffle")
    set_group("test")

target("teststring")
    set_kind("binary")
    add_files("test/test_solid_str.c")
    set_languages("c11")
    add_deps("solid-waffle")
    set_group("test")
   

target("testcompiler")
    set_kind("binary")
    add_files("test/compiler_test.c")
    add_files("compiler.c")
    set_languages("c11")
    add_deps("solid-waffle")
    set_group("test")

target("testtable")
    set_kind("binary")
    add_files("test/test_table.c")
    set_languages("c11")
    add_deps("solid-waffle")
    set_group("test")

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

