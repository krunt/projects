
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
headers = $(wildcard *.h)
object_files = $(filter-out box.o tga2wal.o,$(patsubst %.cpp,%.o,$(sources)))
libs = -lm -Ld3lib -lidlib -lglut -lGL -lSDL -lGLEW -lGLU -lode -lboost_system -lboost_filesystem
flags = -g -O0
compiler = g++

all: box tga2wal

box: $(object_files)
	$(compiler) $(flags) -o box box.cpp $(object_files) $(libs)

tga2wal: $(object_files)
	$(compiler) $(flags) -o tga2wal tga2wal.cpp $(object_files) $(libs)

%.o: %.cpp $(headers)
	$(compiler) $(flags) -c $<	

clean:
	rm -f $(object_files); rm box png2wal
