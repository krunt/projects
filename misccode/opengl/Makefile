
all = $(wildcard *.cpp *.h)
sources = $(wildcard *.cpp)
object_files = $(patsubst %.cpp,%.o,$(sources))
libs = -lm -Ld3lib -lidlib -lglut -lGL -lSDL -lGLEW -lGLU
flags = -g
compiler = g++

box: $(object_files)
	$(compiler) $(flags) -o box $(object_files) $(libs)

%.o: %.cpp
	$(compiler) $(flags) -c $<	

clean:
	rm -f $(object_files)
