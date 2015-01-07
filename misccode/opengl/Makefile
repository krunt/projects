
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
headers = $(wildcard *.h)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm -Ld3lib -lidlib -lglut -lGL -lSDL -lGLEW -lGLU -lode -lboost_system -lboost_filesystem
flags = -g -O2
compiler = g++

box: $(object_files)
	$(compiler) $(flags) -o box $(object_files) $(libs)

%.o: %.cpp $(headers)
	$(compiler) $(flags) -c $<	

clean:
	rm -f $(object_files); rm box
