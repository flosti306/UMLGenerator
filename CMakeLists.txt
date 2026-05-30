# Legt die minimal benötigte CMake-Version fest
cmake_minimum_required(VERSION 3.15)

# Name des Projekts und die verwendete Sprache (CXX = C++)
project(UMLGenerator CXX)

# Wir erzwingen C++17, da unser Lexer auf std::string_view aufbaut
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Deaktiviert compiler-spezifische Erweiterungen (wie gnu++17) für maximale Portabilität
set(CMAKE_CXX_EXTENSIONS OFF) 

# Sucht alle .cpp-Dateien im src-Ordner
# (Für den Start überaus bequem. Bei gigantischen Projekten geht man später 
#  dazu über, die Dateien explizit aufzulisten.)
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Deklariert das finale Programm und verknüpft es mit dem Quellcode
add_executable(uml_generator ${SOURCES})

# Eine exzellente Praxis: Aktivierung aller wichtigen Compiler-Warnungen
# Dies zwingt uns von Beginn an zu einer absolut sauberen Architektur.
if(MSVC)
    # Für Microsoft Visual C++
    target_compile_options(uml_generator PRIVATE /W4)
else()
    # Für GCC und Clang
    target_compile_options(uml_generator PRIVATE -Wall -Wextra -Wpedantic)
endif()