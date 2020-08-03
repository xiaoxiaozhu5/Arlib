PROGRAM = arlibtest
#valid values: exe, dll, hybrid (usable as both exe and dll)
ARTYPE = hybrid
ARGUI = 0
ARGAME = 1
AROPENGL = 0
ARTHREAD = 1
ARWUTF = 0
ARSOCKET = 1
#valid values: openssl (default), gnutls, bearssl, no
ARSOCKET_SSL = openssl
#valid values: schannel (default), bearssl, no (others may work, not tested)
ARSOCKET_SSL_WINDOWS = schannel
ARSANDBOX = 1

# Currently not supported on Windows.
ARGUI = 0
ARSANDBOX = 0

# For Windows, Arlib can target either XP or 7. Latter is recommended; XP support slows down some things and disables some functionality.
# Arlib does not support targetting any other Windows version.
ARXPSUPPORT = 1

#honored variables, in addition to the ones listed here:
#OPT, DEBUG, PROFILE (CLI)
#  OPT=1 enables heavy optimizations; DEBUG=0 removes debug info; PROFILE=gen/opt are for PGO
#CFLAGS, LFLAGS, CC, CXX, LD (CLI)
#  override compiler choice, add additional flags
#ASAN (CLI)
#  if true, compiles with ASan; 
#CONF_CFLAGS, CONF_LFLAGS
#  additional compiler/linker flags needed by this program
#EXCEPTIONS
#  set to 1 if needed
#  not recommended: many parts of Arlib are not guaranteed exception safe, exception support inhibits many optimizations,
#  and it adds a few hundred kilobytes on Windows
#  (tests throw exceptions on failure, and automatically enable them, but tests don't need to be fast, leak-free, or shipped)
#SOURCES
#  extra files to compile, in addition to *.cpp
#  supports .c and .cpp
#SOURCES_NOWARN
#  like SOURCES, but compiled with warnings disabled
#  should only be used for third-party code that can't be fixed
#SOURCES_FOO, CFLAGS_FOO, DOMAINS
#  SOURCES_FOO is compiled with CFLAGS_FOO, in addition to the global ones
#  requires 'DOMAINS += FOO'
#  (SOURCES and SOURCES_NOWARN are implemented as domains)
#OBJNAME (CLI)
#  added to object file names, to allow building for multiple platforms without a make clean
#  if not set, it's autogenerated from target platform, OPT, and whether it's compiling tests or not
#  rarely if ever needs to be manually specified
#the ones listed (CLI) should not be set by the program, but should instead be reserved for command-line arguments

#don't use this, it specifies that Arlib itself is the project here (i.e. enables Arlib's own tests)
ARLIB_MAIN = 1
include arlib/Makefile

#TODO:
#./configure; possibly only verifies dependencies and lets makefile do the real job
#  or maybe makefile calls configure?
#  must make sure multiple platforms can be configured simultaneously
#./test (bash? python?)
#  make test calls some python script, which calls the makefile with new arguments? the current setup is fairly stupid
#    maybe the make test script is in arlib/, not ./
#test all SSLs at once, rename socketssl_impl to socketssl_openssl/etc and ifdef socketssl::create
#  maybe always compile all SSLs, rely on --gc-sections to wipe the unused ones
#    except that screws up if gnutls headers aren't installed
