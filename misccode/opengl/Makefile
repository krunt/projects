
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
headers = $(wildcard *.h)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm -Ld3lib -lidlib -lglut -lGL -lSDL -lGLEW -lGLU -lode
flags = -g -O0
compiler = g++

box: $(object_files)
	$(compiler) $(flags) -o box $(object_files) $(libs)

%.o: %.cpp $(headers)
	$(compiler) $(flags) -c $<	

clean:
	rm -f $(object_files); rm box
